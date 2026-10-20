#ifndef _SESSION
#define _SESSION

// ===========================================================================
//  session.h — Sesión de edición con token + límite de intentos de login.
// ===========================================================================
// Porta la sesión del firmware Arduino: token de 128 bits (esp_random) en RAM,
// TTL FIJO desde el login (la actividad NO lo renueva), y límite de intentos de
// login con espera creciente. Todo vive en RAM: un reinicio lo invalida.

#include <cstdint>
#include <string>

namespace session {

// Emite un token nuevo (login/registro OK) y fija su expiración a SESSION_TTL_MS.
// Sobrescribe el anterior: solo hay UNA sesión activa. Devuelve el token (hex).
std::string issueToken();

// ¿El token coincide con el vigente y no ha expirado? No renueva la ventana.
bool isValid(const std::string& token);

// Invalida la sesión (logout /exit y reset de fábrica).
void clear();

// ---- Límite de intentos de login ----
// ¿El login está bloqueado ahora mismo por demasiados fallos consecutivos?
bool loginLocked();
// Registra un fallo; pasado el umbral fija un bloqueo con espera creciente.
void registerLoginFailure();
// Limpia el contador de fallos (login exitoso).
void resetLoginFailures();

} // namespace session

#endif
