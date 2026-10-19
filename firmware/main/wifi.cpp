// ===========================================================================
//  wifi.cpp — Conectividad Wi-Fi AP+STA (esp_wifi + esp_netif). Ver wifi.h.
//  Equivalente IDF del bloque de red del firmware Arduino: SoftAP WPA2 siempre
//  arriba, STA condicional, modem-sleep off, una sola radio (AP+STA).
// ===========================================================================
#include "wifi.h"
#include <cstring>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "sensible.h"      // apSsid, apPassword (no versionado; ver plantilla)

static const char* TAG = "wifi";

// Grupo de eventos para reflejar el estado del STA.
static EventGroupHandle_t s_events = nullptr;
static const int STA_CONNECTED_BIT = BIT0;

static esp_netif_t* s_ap_netif  = nullptr;
static esp_netif_t* s_sta_netif = nullptr;

// Manejador de eventos Wi-Fi/IP. Reconexión automática del STA (equivalente a
// setAutoReconnect del firmware Arduino) y seguimiento del bit de conexión.
static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, STA_CONNECTED_BIT);
        esp_wifi_connect();                       // reintenta mantener el enlace
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, STA_CONNECTED_BIT);
        ESP_LOGI(TAG, "STA con IP (conectado a la red del usuario)");
    }
}

namespace wifimod {

static void applyStaConfig(const char* ssid, const char* pass) {
    wifi_config_t sta = {};
    strncpy((char*)sta.sta.ssid, ssid, sizeof(sta.sta.ssid) - 1);
    strncpy((char*)sta.sta.password, pass, sizeof(sta.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
}

bool begin(const char* staSsid, const char* staPass) {
    s_events = xEventGroupCreate();

    if (esp_netif_init() != ESP_OK) return false;
    if (esp_event_loop_create_default() != ESP_OK) return false;
    s_ap_netif  = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) return false;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &onEvent, nullptr, nullptr);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onEvent, nullptr, nullptr);

    // Modo dual EXPLÍCITO: el AP y el STA coexisten; evita que el arranque del STA
    // reconfigure el modo por debajo.
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;

    // SoftAP WPA2-PSK (credenciales en sensible.h). create() de Arduino elegía
    // WPA2 con passphrase >= 8; aquí se fija authmode WPA2_PSK explícito.
    wifi_config_t ap = {};
    strncpy((char*)ap.ap.ssid, apSsid, sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = strlen(apSsid);
    strncpy((char*)ap.ap.password, apPassword, sizeof(ap.ap.password) - 1);
    ap.ap.max_connection = 4;
    ap.ap.authmode = (strlen(apPassword) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    if (esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK) return false;

    // STA condicional: solo si hay credenciales guardadas.
    bool hasSta = staSsid && strlen(staSsid) > 0;
    if (hasSta) applyStaConfig(staSsid, staPass);

    if (esp_wifi_start() != ESP_OK) return false;

    // Modem-sleep OFF: imprescindible para que el SoftAP responda estable en AP+STA.
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (hasSta) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "STA: intentando conectar a %s", staSsid);
    }

    ESP_LOGI(TAG, "Wi-Fi AP+STA iniciado (SoftAP '%s')", apSsid);
    return true;
}

bool isStaConnected() {
    if (!s_events) return false;
    return (xEventGroupGetBits(s_events) & STA_CONNECTED_BIT) != 0;
}

void connectSta(const char* ssid, const char* pass) {
    esp_wifi_disconnect();
    applyStaConfig(ssid, pass);
    esp_wifi_connect();
    ESP_LOGI(TAG, "STA: nuevo intento de conexión a %s", ssid);
}

} // namespace wifimod
