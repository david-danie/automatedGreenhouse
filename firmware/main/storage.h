#ifndef _STORAGE
#define _STORAGE

// ===========================================================================
//  storage.h — Persistencia en NVS (ESP-IDF). Reemplaza a Preferences.
// ===========================================================================
// Esquema COMPACTO (un solo namespace "smartplant") con tres grupos separados por
// su ciclo de vida:
//   - "cfg"  : struct CropConfig versionado (TODO lo del cultivo, un solo blob).
//              Lo borra un "cultivo nuevo" y el reset de fábrica.
//   - "auth" : credenciales del usuario (salt + hash PBKDF2 + nombre). Se CONSERVA
//              en "cultivo nuevo"; solo el reset de fábrica la borra.
//   - "wifi" : credenciales de la red del usuario (STA). Mismo ciclo que auth.
//
// Frecuencias de riego/ventilación como uint16_t (admiten > 255 h, p. ej. 336 =
// cada 2 semanas), a diferencia del firmware Arduino (uint8_t).

#include <cstdint>
#include <cstddef>
#include "Constants.h"

// Versión del esquema del blob "cfg". SUBIR si cambia la forma de CropConfig,
// para detectar/descartar un blob viejo incompatible al leerlo.
constexpr uint8_t CONFIG_SCHEMA_VERSION = 1;

// Configuración del cultivo (un único blob en NVS). Empaquetado para que su
// tamaño en flash sea estable y predecible entre builds.
#pragma pack(push, 1)
struct CropConfig {
    uint8_t  schemaVersion;      // = CONFIG_SCHEMA_VERSION
    uint8_t  flags;              // bit0 hasRegisteredUser, bit1 hasWifiCredentials, bit2 systemEnable
    uint8_t  photoperiodOn;      // 0-23
    uint8_t  photoperiodOff;     // 0-23
    uint8_t  blueDutyCycle;      // 0-100
    uint8_t  redDutyCycle;       // 0-100
    uint8_t  whiteLedOn;         // 0/1
    uint8_t  irrigationDuration; // 0-59 min
    uint8_t  ventilationDuration;// 0-59 min
    uint8_t  _reserved;          // alineación / crecimiento futuro
    uint16_t irrigationFrequency;// horas entre riegos (validFrequencies)
    uint16_t ventilationFrequency;// horas entre ventilaciones (validFrequencies)
    uint32_t cropStartDay;       // ancla daysSinceEpoch; 0 = sin anclar
};
#pragma pack(pop)

namespace storage {

// Inicializa el subsistema NVS (nvs_flash_init, con erase+retry si está corrupto
// o la versión de página cambió). Llamar una vez al arranque antes de todo lo demás.
// Devuelve true si NVS quedó operativo.
bool begin();

// ---- Configuración del cultivo ("cfg") ----
// Carga el blob en `out`. Si no existe, o la versión/tamaño no coinciden, deja
// `out` con valores por defecto (todo 0 + schemaVersion actual) y devuelve false
// (= "sin config válida", equivalente a cultivo sin configurar).
bool loadConfig(CropConfig& out);
// Guarda el blob (fija schemaVersion). Devuelve true si se escribió y commit OK.
bool saveConfig(const CropConfig& in);
// Borra solo "cfg" (cultivo nuevo): el cultivo queda sin configurar, auth/wifi intactos.
bool clearConfig();

// ---- Credenciales de usuario ("auth") ----
// salt: pwSaltBytes, hash: pwHashBytes, name: cadena (hasta maxUsernameChars*2+1).
bool loadAuth(uint8_t* salt, uint8_t* hash, char* name, size_t nameCap);
bool saveAuth(const uint8_t* salt, const uint8_t* hash, const char* name);

// ---- Nombre de la planta ("cfg" acompañante; cadena, no cabe en el struct) ----
bool loadPlantName(char* out, size_t cap);
bool savePlantName(const char* name);

// ---- Wi-Fi del usuario ("wifi") ----
bool loadWifi(char* ssid, size_t ssidCap, char* pass, size_t passCap);
bool saveWifi(const char* ssid, const char* pass);
bool clearWifi();

// ---- Reset de fábrica: borra TODO el namespace (cfg + auth + wifi). ----
bool eraseAll();

} // namespace storage

#endif
