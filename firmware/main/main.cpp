// SmartPlant — punto de entrada ESP-IDF (hito 1 de la migración).
//
// Esqueleto mínimo: arranca, imprime info del sistema por el log de IDF y entra en
// un bucle de latido. Todavía SIN lógica de negocio ni periféricos: eso llega en
// los hitos siguientes (NVS, I2C/RTC, LEDC/GPIO, Wi-Fi, HTTP), portando la lógica
// desde ESP32_controller/ (Arduino), que queda como especificación de referencia.
//
// Equivalente conceptual al setup()/loop() del sketch Arduino: aquí app_main hace
// el papel de setup() y lanza el "loop" como una tarea de FreeRTOS.

#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "storage.h"
#include "rtc.h"

static const char *TAG = "smartplant";

static void log_system_info(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);

    ESP_LOGI(TAG, "SmartPlant firmware (ESP-IDF) arrancando");
    ESP_LOGI(TAG, "Chip: %d nucleo(s), revision %d", chip.cores, chip.revision);
    ESP_LOGI(TAG, "Flash: %lu MB", (unsigned long)(flash_size / (1024 * 1024)));
    ESP_LOGI(TAG, "Heap libre: %lu bytes", (unsigned long)esp_get_free_heap_size());
}

extern "C" void app_main(void)
{
    log_system_info();

    // NVS: subsistema de persistencia (reemplaza a Preferences de Arduino).
    if (storage::begin()) {
        CropConfig cfg;
        bool has = storage::loadConfig(cfg);
        ESP_LOGI(TAG, "NVS OK — config de cultivo %s",
                 has ? "cargada" : "no configurada (defaults)");
    } else {
        ESP_LOGE(TAG, "NVS no disponible");
    }

    // RTC DS3231 (I²C): lectura inicial. Si el OSF indica reloj no fijado, la
    // hora no es fiable hasta que el usuario la sincronice desde el portal.
    if (rtc::begin()) {
        uint8_t t[currentTimeCount] = {0};
        if (rtc::getCurrentTime(t)) {
            ESP_LOGI(TAG, "RTC OK — 20%02u-%02u-%02u %02u:%02u:%02u",
                     t[year], t[month], t[day], t[hour], t[minute], t[second]);
        } else {
            ESP_LOGW(TAG, "RTC sin hora fiable (OSF o fuera de rango)");
        }
    } else {
        ESP_LOGE(TAG, "RTC/I2C no disponible");
    }

    // Latido: confirma que el firmware corre y cede CPU al IDLE (alimenta el WDT).
    // Se reemplazará por la inicialización real (begin()) y el control periódico
    // de actuadores/RTC conforme avance la migración.
    while (true) {
        ESP_LOGI(TAG, "latido");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
