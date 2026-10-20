#ifndef _CROPPARAMS
#define _CROPPARAMS

// ===========================================================================
//  cropparams.h — Validación y guardado de parámetros del cultivo (/newparams)
//  y cierre de cultivo (/newcrop). Port de Plant (ESP-IDF).
// ===========================================================================
// Reglas idénticas al firmware Arduino (validación por carácter UTF-8, rangos,
// coherencia frecuencia/duración, fotoperiodo con cruce de medianoche). Diferencia
// intencional de la migración: las frecuencias se validan como uint16_t, así que
// admiten 336 h (cada 2 semanas), que no cabía en el uint8_t del firmware Arduino.
//
// Ambas operaciones exigen token de sesión vigente. /newparams además sincroniza
// la hora del RTC desde el payload y ancla cropStartDay en el primer guardado.

#include <string>
#include "Constants.h"   // requestStatus

namespace cropparams {

// POST /newparams: valida y guarda la configuración del cultivo; sincroniza RTC.
requestStatus validateAndSave(const std::string& body);

// POST /newcrop: cierra el cultivo (borra config del cultivo, conserva cuenta y
// Wi-Fi). Exige token de sesión.
requestStatus startNewCrop(const std::string& body);

} // namespace cropparams

#endif
