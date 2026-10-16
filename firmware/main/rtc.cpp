// ===========================================================================
//  rtc.cpp — Driver DS3231 por I²C (ESP-IDF driver/i2c_master.h). Ver rtc.h.
//  Lógica (enmascarado, validación de rango, OSF) idéntica al firmware de
//  referencia (ESP32_controller/Plant.cpp), reescrita sobre la API nueva de I²C.
// ===========================================================================
#include "rtc.h"
#include "utils.h"          // bcd2bin, bin2bcd
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "rtc";

// Pines I²C del ESP32-C3 (ajustar al cableado real de la placa).
static const int I2C_SDA = 8;
static const int I2C_SCL = 9;
static const uint32_t I2C_FREQ_HZ = 100000;  // 100 kHz, estándar para el DS3231

static i2c_master_bus_handle_t s_bus = nullptr;
static i2c_master_dev_handle_t s_dev = nullptr;
static bool s_valid = false;

namespace rtc {

bool begin() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = (gpio_num_t)I2C_SDA;
    bus_cfg.scl_io_num = (gpio_num_t)I2C_SCL;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus falló");
        return false;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = DS3231Adress;
    dev_cfg.scl_speed_hz = I2C_FREQ_HZ;

    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device falló");
        return false;
    }
    return true;
}

// Lee un registro de 1 byte del DS3231. Devuelve false si la transacción falla.
static bool readReg(uint8_t reg, uint8_t* out) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, 1,
                                       100 /*ms timeout*/) == ESP_OK;
}

// ---- OSF (registro de estado 0x0F): ¿se detuvo el oscilador? ----
static bool lostPower() {
    uint8_t status;
    if (!readReg(DS3231StatusReg, &status))
        return true;                       // sin poder leer: no confiar
    return (status & DS3231OsfMask) != 0;
}

static void clearLostPower() {
    uint8_t status;
    if (!readReg(DS3231StatusReg, &status)) return;
    uint8_t buf[2] = { DS3231StatusReg, (uint8_t)(status & ~DS3231OsfMask) };
    i2c_master_transmit(s_dev, buf, 2, 100);   // baja solo el OSF (lee-modifica-escribe)
}

bool getCurrentTime(uint8_t* t) {
    // Lee los 7 registros de tiempo desde 0x00.
    uint8_t reg = 0x00;
    uint8_t raw[rtcReadBytes];
    if (i2c_master_transmit_receive(s_dev, &reg, 1, raw, rtcReadBytes, 100) != ESP_OK) {
        s_valid = false;                   // el RTC no respondió (bus/alimentación)
        return false;
    }

    // Enmascara bits de control ajenos al BCD:
    //   0x00 segundos: bit7 CH            -> & 0x7F
    //   0x02 horas:    bits 12/24h, AM/PM -> & 0x3F (modo 24h)
    //   0x05 mes:      bit7 Century       -> & 0x1F
    raw[0] &= 0x7F;
    raw[2] &= 0x3F;
    raw[5] &= 0x1F;

    // t se indexa con el enum currentTime (second=1..year=7); raw[0] = segundos.
    for (uint8_t i = 0; i < rtcReadBytes; i++)
        t[i + 1] = bcd2bin(raw[i]);

    bool inRange = (t[second]    <= 59) &&
                   (t[minute]    <= 59) &&
                   (t[hour]      <= 23) &&
                   (t[dayOfWeek] >= 1 && t[dayOfWeek] <= 7) &&
                   (t[day]       >= 1 && t[day]       <= 31) &&
                   (t[month]     >= 1 && t[month]     <= 12) &&
                   (t[year]      <= 99);

    // El OSF distingue "hora fijada por el usuario" de "default tras perder el reloj".
    s_valid = inRange && !lostPower();
    return s_valid;
}

bool setCurrentTime(const uint8_t* t) {
    // Escribe 0x00 (puntero) + 7 bytes BCD (segundos..año).
    uint8_t buf[1 + rtcReadBytes];
    buf[0] = 0x00;
    for (uint8_t i = second; i <= year; i++)
        buf[i] = bin2bcd(t[i]);            // buf[1..7] = _currentTime[second..year]

    bool ok = (i2c_master_transmit(s_dev, buf, sizeof(buf), 100) == ESP_OK);
    if (ok) {
        clearLostPower();                  // la hora ya es de fiar: baja el OSF
        s_valid = true;
    }
    return ok;
}

bool isValid() { return s_valid; }

} // namespace rtc
