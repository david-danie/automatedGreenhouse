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
#include "actuators.h"
#include "control.h"

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

    // Actuadores (PWM + GPIO): estado inicial seguro (todo apagado) antes de
    // leer configuración, igual que allDevicesOff() en el firmware Arduino.
    actuators::begin();
    ESP_LOGI(TAG, "Actuadores inicializados (todo apagado)");

    // NVS: subsistema de persistencia (reemplaza a Preferences de Arduino).
    if (!storage::begin()) {
        ESP_LOGE(TAG, "NVS no disponible");
    }

    // RTC DS3231 (I²C).
    bool rtcReady = rtc::begin();
    if (!rtcReady) ESP_LOGE(TAG, "RTC/I2C no disponible");

    // Bucle de control: re-evalúa luces/riego/ventilación cada
    // deviceUpdateInterval, leyendo RTC + config (equivalente al loop() del
    // firmware Arduino, sin la capa HTTP que llega en el hito de red).
    uint8_t t[currentTimeCount] = {0};
    while (true) {
        CropConfig cfg;
        storage::loadConfig(cfg);                 // defaults si no hay config

        bool rtcValid = rtcReady && rtc::getCurrentTime(t);
        control::update(cfg, t, rtcValid);

        vTaskDelay(pdMS_TO_TICKS(deviceUpdateInterval));
    }
}
