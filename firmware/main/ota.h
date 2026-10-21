#ifndef _OTA
#define _OTA

// ===========================================================================
//  ota.h — Actualización OTA local (ESP-IDF: esp_ota_ops). Port de /otaupdate.
// ===========================================================================
// Reemplaza la librería Update de Arduino. Escribe el .bin recibido por el portal
// en el slot OTA inactivo y, al validar, fija el arranque en ese slot. El control
// de autorización (token + fromAP) y el troceo del cuerpo los hace el handler HTTP;
// este módulo solo encapsula el ciclo de escritura en flash.

#include <cstdint>
#include <cstddef>

namespace ota {

// Inicia una sesión OTA (localiza el slot inactivo y reserva). Devuelve false si
// no se pudo iniciar (p. ej. sin partición OTA disponible).
bool begin();

// Escribe un trozo del binario. Devuelve false ante error (aborta la sesión).
bool write(const uint8_t* data, size_t len);

// Finaliza: valida la imagen y fija el arranque en el nuevo slot. Devuelve true
// si la imagen es válida y quedó marcada para arrancar.
bool finish();

// Aborta la sesión en curso (libera el handle sin cambiar el arranque).
void abort();

} // namespace ota

#endif
