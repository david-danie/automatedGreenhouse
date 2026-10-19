#ifndef _CAPTIVEDNS
#define _CAPTIVEDNS

// ===========================================================================
//  captivedns.h — Servidor DNS captivo mínimo (ESP-IDF).
// ===========================================================================
// Reemplaza el DNSServer de Arduino. Responde la IP del SoftAP (apGatewayIp) a
// TODA consulta DNS tipo A, para que el sistema operativo del cliente detecte el
// portal captivo y abra el formulario solo. Corre en su propia tarea FreeRTOS
// escuchando UDP en el puerto 53.

namespace captivedns {

// Arranca la tarea del servidor DNS. Debe llamarse con el SoftAP ya activo.
// Devuelve false si no pudo crear el socket/tarea.
bool begin();

} // namespace captivedns

#endif
