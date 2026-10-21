// ===========================================================================
//  ota.cpp — OTA local con esp_ota_ops. Ver ota.h.
//  Equivalente IDF de la librería Update de Arduino: escribe en el slot OTA
//  inactivo y fija el arranque al validar. El doble slot lo garantiza
//  partitions.csv (ota_0 / ota_1).
// ===========================================================================
#include "ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

static const char* TAG = "ota";

static esp_ota_handle_t s_handle = 0;
static const esp_partition_t* s_target = nullptr;

namespace ota {

bool begin() {
    s_target = esp_ota_get_next_update_partition(nullptr);  // slot inactivo
    if (!s_target) { ESP_LOGE(TAG, "sin partición OTA de destino"); return false; }

    // OTA_SIZE_UNKNOWN: el tamaño real se conoce al terminar; esp_ota_end valida.
    esp_err_t err = esp_ota_begin(s_target, OTA_SIZE_UNKNOWN, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        s_target = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "OTA inicio en partición '%s'", s_target->label);
    return true;
}

bool write(const uint8_t* data, size_t len) {
    if (!s_target) return false;
    esp_err_t err = esp_ota_write(s_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool finish() {
    if (!s_target) return false;
    esp_err_t err = esp_ota_end(s_handle);     // valida la imagen (magic, hash estructural)
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        s_target = nullptr;
        return false;
    }
    err = esp_ota_set_boot_partition(s_target);
    s_target = nullptr;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "OTA completado: arrancará el firmware nuevo");
    return true;
}

void abort() {
    if (s_target) {
        esp_ota_abort(s_handle);
        s_target = nullptr;
        ESP_LOGW(TAG, "OTA abortado");
    }
}

} // namespace ota
