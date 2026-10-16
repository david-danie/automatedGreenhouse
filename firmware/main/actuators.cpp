// ===========================================================================
//  actuators.cpp — PWM (LEDC) y salidas digitales (GPIO). Ver actuators.h.
//  Equivalente a la inicialización y al control de salidas del firmware Arduino
//  (ledcAttachChannel/ledcWrite, pinMode/digitalWrite), reescrito en IDF nativo.
// ===========================================================================
#include "actuators.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

// LEDC: un timer de baja velocidad compartido por los dos canales PWM.
static const ledc_timer_t     LEDC_TIMER   = LEDC_TIMER_0;
static const ledc_mode_t      LEDC_MODE    = LEDC_LOW_SPEED_MODE;
static const ledc_channel_t   CH_BLUE      = LEDC_CHANNEL_0;
static const ledc_channel_t   CH_RED       = LEDC_CHANNEL_1;

namespace actuators {

// Escala 0-100 % a 0-maxDutyCycle (equivalente a map(pct,0,100,0,maxDutyCycle)).
static uint32_t pctToDuty(uint8_t pct) {
    if (pct >= 100) return maxDutyCycle;
    return ((uint32_t)pct * maxDutyCycle) / 100;
}

// Configura un GPIO como salida simple.
static void gpioOutput(int pin) {
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << pin);
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
}

void begin() {
    // ---- PWM (LEDC): timer 1 kHz, 8 bits ----
    ledc_timer_config_t tcfg = {};
    tcfg.speed_mode      = LEDC_MODE;
    tcfg.timer_num       = LEDC_TIMER;
    tcfg.duty_resolution = LEDC_TIMER_8_BIT;      // pwmResolution = 8
    tcfg.freq_hz         = pwmFrequency;          // 1000 Hz
    tcfg.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&tcfg);

    ledc_channel_config_t blue = {};
    blue.gpio_num   = blueLedPin;
    blue.speed_mode = LEDC_MODE;
    blue.channel    = CH_BLUE;
    blue.timer_sel  = LEDC_TIMER;
    blue.duty       = 0;
    blue.hpoint     = 0;
    ledc_channel_config(&blue);

    ledc_channel_config_t red = blue;
    red.gpio_num = redLedPin;
    red.channel  = CH_RED;
    ledc_channel_config(&red);

    // ---- Salidas digitales ----
    gpioOutput(whiteLedPin);
    gpioOutput(waterPumpPin);
    gpioOutput(fanPin);
    gpioOutput(buzzerPin);
    gpio_set_level((gpio_num_t)buzzerPin, deviceOff);   // buzzer en silencio

    // Estado inicial seguro: todo apagado antes de leer configuración.
    allOff();
}

static void setDuty(ledc_channel_t ch, uint32_t duty) {
    ledc_set_duty(LEDC_MODE, ch, duty);
    ledc_update_duty(LEDC_MODE, ch);
}

void setBlueDuty(uint8_t pct) { setDuty(CH_BLUE, pctToDuty(pct)); }
void setRedDuty(uint8_t pct)  { setDuty(CH_RED,  pctToDuty(pct)); }

void setWhite(bool on)  { gpio_set_level((gpio_num_t)whiteLedPin,  on ? deviceOn : deviceOff); }
void setPump(bool on)   { gpio_set_level((gpio_num_t)waterPumpPin, on ? deviceOn : deviceOff); }
void setFan(bool on)    { gpio_set_level((gpio_num_t)fanPin,       on ? deviceOn : deviceOff); }
void setBuzzer(bool on) { gpio_set_level((gpio_num_t)buzzerPin,    on ? deviceOn : deviceOff); }

void allOff() {
    setWhite(false);
    setDuty(CH_BLUE, 0);
    setDuty(CH_RED, 0);
    setPump(false);
    setFan(false);
}

} // namespace actuators
