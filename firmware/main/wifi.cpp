// ===========================================================================
//  wifi.cpp — Conectividad Wi-Fi AP+STA (esp_wifi + esp_netif). Ver wifi.h.
//  Equivalente IDF del bloque de red del firmware Arduino: SoftAP WPA2 siempre
//  arriba, STA condicional, modem-sleep off, una sola radio (AP+STA).
// ===========================================================================
#include "wifi.h"
#include <cstring>
#include <string>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "sensible.h"      // apSsid, apPassword (no versionado; ver plantilla)
#include "storage.h"       // persistencia diferida de credenciales
#include "Constants.h"     // límites de SSID/pass
#include <ArduinoJson.h>
#include "session.h"

static const char* TAG = "wifi";

// Escapa comillas y barras invertidas para que un SSID no rompa el JSON.
static std::string jsonEscape(const char* s) {
    std::string o;
    for (const char* p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') o += '\\';
        o += *p;
    }
    return o;
}

// Grupo de eventos para reflejar el estado del STA.
static EventGroupHandle_t s_events = nullptr;
static const int STA_CONNECTED_BIT = BIT0;

static esp_netif_t* s_ap_netif  = nullptr;
static esp_netif_t* s_sta_netif = nullptr;

// Credenciales en RAM aún NO persistidas: entre connectSta() y la confirmación
// de conexión. Se guardan en NVS solo al conectar con éxito (updateWifiPersist).
static bool s_wifiPending = false;
static char s_pendSsid[33] = {0};
static char s_pendPass[64] = {0};

// Estado del escaneo asíncrono ("arranca y consulta").
static bool s_scanRunning = false;

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
    // Recuerda las credenciales como "pendientes": se persisten solo al conectar.
    strncpy(s_pendSsid, ssid, sizeof(s_pendSsid) - 1);
    strncpy(s_pendPass, pass, sizeof(s_pendPass) - 1);
    s_pendSsid[sizeof(s_pendSsid) - 1] = '\0';
    s_pendPass[sizeof(s_pendPass) - 1] = '\0';
    s_wifiPending = true;
    esp_wifi_connect();
    ESP_LOGI(TAG, "STA: nuevo intento de conexión a %s", ssid);
}

bool updateWifiPersist() {
    if (!s_wifiPending) return false;
    if (!isStaConnected()) return false;     // aún no conecta: no persistir
    s_wifiPending = false;
    bool ok = storage::saveWifi(s_pendSsid, s_pendPass);

    // Marca hasWifiCredentials (bit1) en la config, para que el próximo arranque
    // intente el STA. Igual que _systemStatus[hasWifiCredentials] en Arduino.
    if (ok) {
        CropConfig cfg;
        storage::loadConfig(cfg);
        if (!(cfg.flags & 0x02)) {
            cfg.flags |= 0x02;
            storage::saveConfig(cfg);
        }
    }
    ESP_LOGI(TAG, "STA conectado — credenciales %s", ok ? "guardadas" : "NO guardadas");
    return ok;
}

// Escaneo asíncrono. Devuelve JSON construido a mano (lista pequeña y controlada).
std::string scanNetworks() {
    if (!s_scanRunning) {
        wifi_scan_config_t sc = {};             // escaneo activo en todos los canales
        if (esp_wifi_scan_start(&sc, false /*no bloquear*/) == ESP_OK) {
            s_scanRunning = true;
        }
        return "{\"scanning\":true,\"networks\":[]}";
    }

    // ¿Terminó? Si aún no hay registros, sigue corriendo.
    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);
    if (num == 0) {
        return "{\"scanning\":true,\"networks\":[]}";
    }

    // Trae los resultados (hasta un tope razonable para no gastar RAM).
    const uint16_t CAP = 20;
    wifi_ap_record_t recs[CAP];
    uint16_t got = CAP;
    if (esp_wifi_scan_get_ap_records(&got, recs) != ESP_OK) {
        s_scanRunning = false;
        return "{\"scanning\":false,\"networks\":[]}";
    }
    s_scanRunning = false;                       // resultados consumidos

    // Top-5 por RSSI, deduplicado por SSID (conserva la señal más fuerte).
    const uint8_t MAX = 5;
    char    bestSsid[MAX][maxWifiSsidChars + 1];
    int8_t  bestRssi[MAX];
    bool    bestOpen[MAX];
    uint8_t count = 0;

    for (uint16_t i = 0; i < got; i++) {
        const char* ssid = (const char*)recs[i].ssid;
        if (ssid[0] == '\0') continue;           // red oculta
        int8_t rssi = recs[i].rssi;
        bool open = (recs[i].authmode == WIFI_AUTH_OPEN);

        int found = -1;
        for (uint8_t k = 0; k < count; k++)
            if (strncmp(ssid, bestSsid[k], maxWifiSsidChars) == 0) { found = k; break; }

        if (found >= 0) {
            if (rssi > bestRssi[found]) { bestRssi[found] = rssi; bestOpen[found] = open; }
            continue;
        }
        if (count < MAX) {
            strncpy(bestSsid[count], ssid, maxWifiSsidChars); bestSsid[count][maxWifiSsidChars] = '\0';
            bestRssi[count] = rssi; bestOpen[count] = open; count++;
        } else {
            uint8_t weakest = 0;
            for (uint8_t k = 1; k < MAX; k++) if (bestRssi[k] < bestRssi[weakest]) weakest = k;
            if (rssi > bestRssi[weakest]) {
                strncpy(bestSsid[weakest], ssid, maxWifiSsidChars); bestSsid[weakest][maxWifiSsidChars] = '\0';
                bestRssi[weakest] = rssi; bestOpen[weakest] = open;
            }
        }
    }

    std::string out = "{\"scanning\":false,\"networks\":[";
    for (uint8_t k = 0; k < count; k++) {
        if (k) out += ',';
        out += "{\"ssid\":\"";
        out += jsonEscape(bestSsid[k]);
        out += "\",\"rssi\":";
        out += std::to_string(bestRssi[k]);
        out += ",\"secure\":";
        out += (bestOpen[k] ? "false" : "true");
        out += '}';
    }
    out += "]}";
    return out;
}

requestStatus saveCredentials(const std::string& body) {
    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, body)) return INVALID_JSON;

    // Autorización por token (misma puerta que /newparams).
    if (!doc.containsKey("token")) return INVALID_SESSION;
    const char* tok = doc["token"] | "";
    if (!session::isValid(std::string(tok))) return INVALID_SESSION;

    if (!doc.containsKey("ssid")) return MISSING_WIFI_FIELDS;

    std::string ssid = doc["ssid"] | "";
    // trim
    { size_t a=ssid.find_first_not_of(" \t\r\n"), b=ssid.find_last_not_of(" \t\r\n");
      ssid = (a==std::string::npos)? "" : ssid.substr(a, b-a+1); }
    if (ssid.empty() || ssid.length() > maxWifiSsidChars) return INVALID_SSID;

    // Contraseña: vacía = red abierta (válido); si viene, WPA2 8–63.
    std::string pass = doc["pass"] | "";
    if (!pass.empty() && (pass.length() < minWifiPassChars || pass.length() > maxWifiPassChars))
        return INVALID_WIFI_PASS;

    connectSta(ssid.c_str(), pass.c_str());   // marca pendiente + arranca el intento
    return STATUS_OK;
}

} // namespace wifimod
