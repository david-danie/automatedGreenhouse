// ===========================================================================
//  cropparams.cpp — Parámetros del cultivo (/newparams) y /newcrop. Ver .h.
//  Port fiel de validateCropParameters/startNewCrop del firmware Arduino.
// ===========================================================================
#include "cropparams.h"
#include <ArduinoJson.h>
#include <string>
#include "esp_log.h"
#include "utils.h"
#include "storage.h"
#include "rtc.h"
#include "session.h"

static const char* TAG = "crop";
static const uint8_t FLAG_SYS_EN = 0x04;   // bit2 systemEnable

static std::string jstr(JsonVariantConst v) {
    const char* s = v | "";
    std::string r(s);
    size_t a = r.find_first_not_of(" \t\r\n");
    size_t b = r.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : r.substr(a, b - a + 1);
}

namespace cropparams {

requestStatus validateAndSave(const std::string& body) {
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, body)) return INVALID_JSON;

    const char* req[] = {"planta","enable","fpOn","fpOff","ledAzul","ledRojo",
                         "ledBlanco","irrH","irrM","ventH","ventM"};
    for (auto k : req) if (!doc.containsKey(k)) return MISSING_FIELDS;

    // Autorización por token de sesión vigente.
    if (!doc.containsKey("token")) return INVALID_SESSION;
    if (!session::isValid(jstr(doc["token"]))) return INVALID_SESSION;

    // Nombre de la planta.
    std::string plant = jstr(doc["planta"]);
    if (utf8Len(plant) < minPlantNameChars || utf8Len(plant) > maxPlantNameChars)
        return INVALID_PLANTNAME_LENGTH;
    if (!isValidReadableString(plant, true)) return INVALID_PLANTNAME_CHARS;
    if (hasConsecutiveSpaces(plant)) return PLANTNAME_REPEATED_SPACES;
    if (isAllDigits(plant)) return PLANTNAME_ONLY_DIGITS;
    if (hasTooManyRepeatedChars(plant)) return PLANTNAME_REPEATED_CHARS;

    // Fotoperiodo (0-23, no iguales; puede cruzar medianoche).
    if (!doc["fpOn"].is<uint8_t>() || doc["fpOn"] > 23 ||
        !doc["fpOff"].is<uint8_t>() || doc["fpOff"] > 23)
        return INVALID_PHOTOPERIOD_TYPE;
    if ((uint8_t)doc["fpOn"] == (uint8_t)doc["fpOff"])
        return INVALID_PHOTOPERIOD_TYPE;

    // Frecuencias: uint16_t (admiten 336 = cada 2 semanas) + validFrequencies.
    // Duración 0-59.
    if (!doc["irrH"].is<uint16_t>() || !doc["irrM"].is<uint8_t>() ||
        !isValidFrequency(doc["irrH"]) || doc["irrM"] > 59)
        return INVALID_IRRIGATION_TYPE;
    if (!doc["ventH"].is<uint16_t>() || !doc["ventM"].is<uint8_t>() ||
        !isValidFrequency(doc["ventH"]) || doc["ventM"] > 59)
        return INVALID_VENTILATION_TYPE;

    // Coherencia frecuencia/duración: frecuencia activa con duración 0 nunca
    // enciende; se rechaza (para apagar existe la frecuencia 0).
    if ((uint16_t)doc["irrH"] > 0 && (uint8_t)doc["irrM"] == 0)
        return INVALID_IRRIGATION_DURATION;
    if ((uint16_t)doc["ventH"] > 0 && (uint8_t)doc["ventM"] == 0)
        return INVALID_VENTILATION_DURATION;

    // Espectros PWM 0-100; blanco digital 0/1.
    if (!doc["ledAzul"].is<uint8_t>() || doc["ledAzul"] > 100 ||
        !doc["ledRojo"].is<uint8_t>() || doc["ledRojo"] > 100)
        return INVALID_LED_VALUE;
    if (!doc["ledBlanco"].is<uint8_t>() || doc["ledBlanco"] > 1)
        return INVALID_WHITE_LED_VALUE;

    // Fecha/hora (para sincronizar el RTC).
    if (!doc["seg"].is<uint8_t>()   || doc["seg"]   > 59) return INVALID_SECOND_FORMAT;
    if (!doc["min"].is<uint8_t>()   || doc["min"]   > 59) return INVALID_MINUTE_FORMAT;
    if (!doc["hr"].is<uint8_t>()    || doc["hr"]    > 23) return INVALID_HOUR_FORMAT;
    if (!doc["diaSem"].is<uint8_t>()|| doc["diaSem"]< 1 || doc["diaSem"] > 7) return INVALID_WEEKDAY_FORMAT;
    if (!doc["dia"].is<uint8_t>()   || doc["dia"]   < 1 || doc["dia"] > 31)  return INVALID_DAY_FORMAT;
    if (!doc["mes"].is<uint8_t>()   || doc["mes"]   < 1 || doc["mes"] > 12)  return INVALID_MONTH_FORMAT;
    if (!doc["anio"].is<uint8_t>()  || doc["anio"]  > 99) return INVALID_YEAR_FORMAT;

    // ---- Construir y guardar la config (preserva flags/cropStart previos) ----
    CropConfig cfg;
    storage::loadConfig(cfg);

    bool enable = doc["enable"] | false;
    if (enable) cfg.flags |= FLAG_SYS_EN; else cfg.flags &= ~FLAG_SYS_EN;
    cfg.photoperiodOn        = doc["fpOn"]  | 0;
    cfg.photoperiodOff       = doc["fpOff"] | 0;
    cfg.blueDutyCycle        = doc["ledAzul"]  | 0;
    cfg.redDutyCycle         = doc["ledRojo"]  | 0;
    cfg.whiteLedOn           = doc["ledBlanco"]| 0;
    cfg.irrigationFrequency  = doc["irrH"]  | 0;
    cfg.irrigationDuration   = doc["irrM"]  | 0;
    cfg.ventilationFrequency = doc["ventH"] | 0;
    cfg.ventilationDuration  = doc["ventM"] | 0;

    // Sincroniza el RTC con la fecha/hora del navegador.
    uint8_t t[currentTimeCount] = {0};
    t[second]    = doc["seg"];
    t[minute]    = doc["min"];
    t[hour]      = doc["hr"];
    t[dayOfWeek] = doc["diaSem"];
    t[day]       = doc["dia"];
    t[month]     = doc["mes"];
    t[year]      = doc["anio"];
    rtc::setCurrentTime(t);

    // Ancla cropStartDay en el PRIMER guardado con fecha válida (no reescribe).
    if (cfg.cropStartDay == 0) {
        cfg.cropStartDay = daysSinceEpoch(2000 + t[year], t[month], t[day]);
    }

    if (!storage::saveConfig(cfg)) return STORAGE_ERROR;
    if (!storage::savePlantName(plant.c_str())) return STORAGE_ERROR;

    ESP_LOGI(TAG, "Parámetros del cultivo actualizados");
    return STATUS_OK;
}

requestStatus startNewCrop(const std::string& body) {
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, body)) return INVALID_JSON;
    if (!doc.containsKey("token")) return INVALID_SESSION;
    if (!session::isValid(jstr(doc["token"]))) return INVALID_SESSION;

    // El cultivo nuevo CONSERVA la cuenta y la Wi-Fi: borra la config del cultivo
    // pero preserva el flag hasRegisteredUser (la cuenta vive en 'auth', aparte).
    CropConfig prev;
    bool hadUser = storage::loadConfig(prev) && (prev.flags & 0x01);  // bit0

    if (!storage::clearConfig()) return STORAGE_ERROR;

    if (hadUser) {
        CropConfig fresh;             // loadConfig deja defaults (todo 0)
        storage::loadConfig(fresh);
        fresh.flags |= 0x01;          // re-marca hasRegisteredUser
        storage::saveConfig(fresh);
    }
    ESP_LOGI(TAG, "Cultivo cerrado (cuenta y Wi-Fi conservadas)");
    return NEW_CROP_DONE;
}

} // namespace cropparams
