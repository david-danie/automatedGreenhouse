#ifndef _CONTROL
#define _CONTROL

// ===========================================================================
//  control.h — Lógica de control de actuadores (ESP-IDF).
// ===========================================================================
// Porta turnOnDevices()/manageDevice() del firmware Arduino. Orquesta, a partir
// de la configuración del cultivo (CropConfig) y la hora actual (arreglo
// currentTime del RTC), qué salidas deben estar activas, accionándolas vía el
// módulo actuators. No toca I²C ni NVS: recibe el estado ya leído.
//
// Mantiene intactas las tres garantías del diseño:
//   1. Interruptor general (systemEnable): si está off, TODO apagado.
//   2. Fail-safe por RTC: sin hora fiable, TODO apagado.
//   3. Scheduling STATELESS: el encendido se recalcula desde un contador continuo
//      de horas (epochHours % intervalo), así sobrevive cortes de luz.

#include <cstdint>
#include "storage.h"   // CropConfig

namespace control {

// Re-evalúa y acciona luces, riego y ventilación.
//   cfg       : configuración del cultivo (frecuencias en horas, duty, etc.).
//   t         : hora actual indexada por el enum currentTime (second..year).
//   rtcValid  : ¿la última lectura del RTC es fiable? (fail-safe si false).
void update(const CropConfig& cfg, const uint8_t* t, bool rtcValid);

// Edad del cultivo en días (>=1), derivada del RTC y del ancla cfg.cropStartDay.
// Devuelve 0 si no hay hora fiable, el cultivo no está anclado, o el RTC retrocedió.
int cropDayFromRtc(const CropConfig& cfg, const uint8_t* t, bool rtcValid);

} // namespace control

#endif
