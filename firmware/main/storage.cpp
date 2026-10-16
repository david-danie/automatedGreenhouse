// ===========================================================================
//  storage.cpp — Persistencia en NVS (ESP-IDF). Ver storage.h.
// ===========================================================================
#include "storage.h"
#include <cstring>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "storage";
static const char* NS  = "smartplant";

// Claves dentro del namespace único.
static const char* K_CFG   = "cfg";       // blob CropConfig
static const char* K_PNAME = "pname";     // nombre de la planta (str)
static const char* K_SALT  = "pwsalt";    // blob salt
static const char* K_HASH  = "pwhash";    // blob hash
static const char* K_UNAME = "uname";     // usuario (str)
static const char* K_SSID  = "ssid";      // str
static const char* K_WPASS = "wpass";     // str

namespace storage {

bool begin() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Partición NVS corrupta o de otra versión: borrar y reintentar.
        ESP_LOGW(TAG, "NVS requiere erase (%s); reintentando", esp_err_to_name(err));
        if (nvs_flash_erase() != ESP_OK) return false;
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// Helpers de apertura.
static bool openRead(nvs_handle_t& h) {
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    return err == ESP_OK;   // ESP_ERR_NVS_NOT_FOUND si el namespace aún no existe
}
static bool openWrite(nvs_handle_t& h) {
    return nvs_open(NS, NVS_READWRITE, &h) == ESP_OK;
}

// ---- Configuración del cultivo ----
bool loadConfig(CropConfig& out) {
    // Valores por defecto: cultivo sin configurar.
    std::memset(&out, 0, sizeof(out));
    out.schemaVersion = CONFIG_SCHEMA_VERSION;

    nvs_handle_t h;
    if (!openRead(h)) return false;

    CropConfig tmp;
    size_t len = sizeof(tmp);
    esp_err_t err = nvs_get_blob(h, K_CFG, &tmp, &len);
    nvs_close(h);

    if (err != ESP_OK) return false;                 // no existe aún
    if (len != sizeof(CropConfig)) return false;      // tamaño distinto = esquema viejo
    if (tmp.schemaVersion != CONFIG_SCHEMA_VERSION) return false;  // versión incompatible

    out = tmp;
    return true;
}

bool saveConfig(const CropConfig& in) {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    CropConfig c = in;
    c.schemaVersion = CONFIG_SCHEMA_VERSION;
    esp_err_t err = nvs_set_blob(h, K_CFG, &c, sizeof(c));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) { ESP_LOGE(TAG, "saveConfig: %s", esp_err_to_name(err)); return false; }
    return true;
}

bool clearConfig() {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    // Borra cfg y el nombre de la planta (van juntos con el cultivo). auth/wifi se conservan.
    nvs_erase_key(h, K_CFG);     // ESP_ERR_NVS_NOT_FOUND es aceptable
    nvs_erase_key(h, K_PNAME);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// ---- Helpers str ----
static bool loadStr(const char* key, char* out, size_t cap) {
    nvs_handle_t h;
    if (!openRead(h)) { if (cap) out[0] = '\0'; return false; }
    size_t len = cap;
    esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    if (err != ESP_OK) { if (cap) out[0] = '\0'; return false; }
    return true;
}
static bool saveStr(const char* key, const char* val) {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    esp_err_t err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// ---- Credenciales de usuario ----
bool loadAuth(uint8_t* salt, uint8_t* hash, char* name, size_t nameCap) {
    nvs_handle_t h;
    if (!openRead(h)) return false;
    size_t sl = pwSaltBytes, hl = pwHashBytes;
    esp_err_t e1 = nvs_get_blob(h, K_SALT, salt, &sl);
    esp_err_t e2 = nvs_get_blob(h, K_HASH, hash, &hl);
    size_t nl = nameCap;
    esp_err_t e3 = nvs_get_str(h, K_UNAME, name, &nl);
    nvs_close(h);
    return e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK &&
           sl == pwSaltBytes && hl == pwHashBytes;
}

bool saveAuth(const uint8_t* salt, const uint8_t* hash, const char* name) {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    esp_err_t err = nvs_set_blob(h, K_SALT, salt, pwSaltBytes);
    if (err == ESP_OK) err = nvs_set_blob(h, K_HASH, hash, pwHashBytes);
    if (err == ESP_OK) err = nvs_set_str(h, K_UNAME, name);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) { ESP_LOGE(TAG, "saveAuth: %s", esp_err_to_name(err)); return false; }
    return true;
}

// ---- Nombre de la planta ----
bool loadPlantName(char* out, size_t cap) { return loadStr(K_PNAME, out, cap); }
bool savePlantName(const char* name)      { return saveStr(K_PNAME, name); }

// ---- Wi-Fi ----
bool loadWifi(char* ssid, size_t ssidCap, char* pass, size_t passCap) {
    bool a = loadStr(K_SSID, ssid, ssidCap);
    bool b = loadStr(K_WPASS, pass, passCap);
    return a && b;
}
bool saveWifi(const char* ssid, const char* pass) {
    return saveStr(K_SSID, ssid) && saveStr(K_WPASS, pass);
}
bool clearWifi() {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    nvs_erase_key(h, K_SSID);
    nvs_erase_key(h, K_WPASS);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

// ---- Reset de fábrica ----
bool eraseAll() {
    nvs_handle_t h;
    if (!openWrite(h)) return false;
    esp_err_t err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

} // namespace storage
