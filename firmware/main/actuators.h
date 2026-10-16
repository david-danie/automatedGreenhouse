#ifndef _ACTUATORS
#define _ACTUATORS

// ===========================================================================
//  actuators.h — Salidas: PWM (LEDC) y digitales (GPIO) en ESP-IDF.
// ===========================================================================
// Reemplaza ledcAttachChannel/ledcWrite y pinMode/digitalWrite de Arduino.
//   - Azul y rojo: PWM por LEDC (1 kHz, 8 bits). La API recibe 0-100 % y escala
//     internamente a 0-maxDutyCycle (como el map() del firmware Arduino).
//   - Blanco, bomba, ventilador, buzzer: salidas digitales con la polaridad
//     centralizada en deviceOn/deviceOff (Constants.h).
//
// La inicialización va en begin() (llamado desde el arranque de la app), NO en
// construcción estática, por el mismo motivo que el firmware Arduino: configurar
// periféricos antes de que el sistema esté listo puede dejar un pin mal definido.

#include <cstdint>
#include "Constants.h"

namespace actuators {

// Configura LEDC (timer + canales azul/rojo) y los GPIO de salida, y deja TODO
// apagado (estado inicial seguro). Llamar una vez al arranque.
void begin();

// Espectros PWM: duty en PORCENTAJE 0-100 (se escala a 0-maxDutyCycle).
void setBlueDuty(uint8_t pct);
void setRedDuty(uint8_t pct);

// Salidas digitales ON/OFF (true = encender, con la polaridad deviceOn/deviceOff).
void setWhite(bool on);
void setPump(bool on);
void setFan(bool on);
void setBuzzer(bool on);

// Apaga TODOS los actuadores (luces incluidas). Punto único para el "todo
// apagado" (sistema deshabilitado o RTC sin hora fiable).
void allOff();

} // namespace actuators

#endif
