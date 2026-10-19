#ifndef _WIFI_MOD
#define _WIFI_MOD

// ===========================================================================
//  wifi.h — Conectividad Wi-Fi AP+STA (ESP-IDF: esp_wifi + esp_netif).
// ===========================================================================
// Reemplaza la capa WiFi de Arduino. Modelo idéntico al firmware de referencia:
//   - SoftAP SIEMPRE arriba (WPA2-PSK, credenciales en sensible.h), sirve el
//     portal captivo. Gateway en apGatewayIp (192.168.4.1).
//   - STA condicional: solo se conecta a la red del usuario si hay credenciales
//     guardadas. Una sola radio → AP y STA comparten canal.
//   - Modem-sleep DESACTIVADO: con STA activo, el power-save dejaría al SoftAP
//     sin responder y el portal no cargaría.
//
// La persistencia diferida de credenciales (guardar solo al conectar con éxito)
// y el escaneo de redes se añaden en el siguiente sub-paso.

#include <cstdint>
#include <string>

namespace wifimod {

// Arranca el stack de red (netif + event loop + esp_wifi) en modo AP+STA y crea
// el SoftAP. Si staSsid no está vacío, además arranca el intento de conexión STA
// (no bloqueante). Llamar una vez al arranque. Devuelve false si el init falla.
bool begin(const char* staSsid, const char* staPass);

// ¿El STA está asociado y con IP en la red del usuario?
bool isStaConnected();

// Lanza/reinicia un intento de conexión STA con nuevas credenciales (no bloquea;
// el resultado se observa con isStaConnected()). Usado por la config de red.
void connectSta(const char* ssid, const char* pass);

// Escaneo ASÍNCRONO de redes ("arranca y consulta"): la primera llamada lanza el
// escaneo y devuelve scanning=true; las siguientes devuelven scanning=true
// mientras corre, y al terminar la lista JSON de las 5 más fuertes sin SSID
// repetidos: {"scanning":false,"networks":[{"ssid","rssi","secure"}]}.
std::string scanNetworks();

// Persistencia diferida: llamar periódicamente. Si hay un intento pendiente
// (connectSta) y el STA quedó conectado, guarda las credenciales en NVS y marca
// hasWifiCredentials. Así una contraseña incorrecta nunca queda persistida.
// Devuelve true la primera vez que persiste (para que el llamador registre el flag).
bool updateWifiPersist();

} // namespace wifimod

#endif
