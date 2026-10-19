#ifndef _HTTPSERVER
#define _HTTPSERVER

// ===========================================================================
//  httpserver.h — Servidor HTTP del portal (ESP-IDF: esp_http_server).
// ===========================================================================
// Reemplaza el WebServer de Arduino. Sirve el portal (gzip) y los endpoints de
// la API del dispositivo. En este sub-paso: raíz '/' (portal) + catch-all de
// portal captivo. Los demás endpoints se registran en sub-pasos siguientes.

namespace httpserver {

// Arranca el servidor HTTP y registra los handlers. Devuelve false si falla.
bool begin();

} // namespace httpserver

#endif
