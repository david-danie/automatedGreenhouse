#ifndef _RTC
#define _RTC

// ===========================================================================
//  rtc.h — Driver del DS3231 por I²C (ESP-IDF, driver/i2c_master.h).
// ===========================================================================
// Reemplaza el uso de Wire del firmware Arduino. Mantiene el modelo de "modo
// seguro por OSF": mientras el bit OSF del registro de estado (0x0F) indique que
// el oscilador se detuvo, la hora NO es fiable aunque esté dentro de rango (un
// RTC virgen devuelve 2000-01-01, que "parece" válida).
//
// La hora se maneja como un arreglo indexado por el enum `currentTime`
// (second=1..year=7, tamaño currentTimeCount), igual que el firmware de
// referencia, para que la lógica que lo consume no cambie.

#include <cstdint>
#include "Constants.h"

namespace rtc {

// Inicializa el bus I²C maestro y el handle del dispositivo DS3231. Llamar una
// vez al arranque. Devuelve false si la instalación del bus/dispositivo falla.
bool begin();

// Lee los 7 registros de tiempo (0x00..0x06) a `t` (indexado por currentTime).
// Enmascara los bits de control y valida rango + OSF. Actualiza el estado
// interno de validez. Devuelve true si la lectura es fiable.
bool getCurrentTime(uint8_t* t);

// Escribe la hora en `t` (valores ya validados por rango) al DS3231 y, si la
// transmisión tiene éxito, limpia el OSF (la hora pasa a ser de fiar).
// Devuelve true si se escribió correctamente.
bool setCurrentTime(const uint8_t* t);

// ¿La última lectura/escritura dejó una hora fiable? (rango OK y OSF limpio).
bool isValid();

} // namespace rtc

#endif
