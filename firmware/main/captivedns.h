#ifndef _CAPTIVEDNS
#define _CAPTIVEDNS

// ===========================================================================
//  captivedns.h — Servidor DNS captivo mínimo (ESP-IDF).
// ===========================================================================
// Reemplaza el DNSServer de Arduino. Responde la IP del SoftAP (apGatewayIp) a
// TODA consulta DNS tipo A, para que el sistema operativo del cliente detecte el
// portal captivo y abra el formulario solo. Corre en su propia tarea FreeRTOS
// escuchando UDP en el puerto 53, ligado SOLO a la IP del SoftAP (no responde por
// la interfaz STA).

namespace captivedns {

// Arranca la tarea del servidor DNS. Debe llamarse con el SoftAP ya activo
// (la tarea reintenta unos segundos el bind a la IP del AP por si la interfaz
// aún no la tiene asignada). Devuelve false si no pudo crear la tarea.
bool begin();

} // namespace captivedns

#endif
