#ifndef _AUTH
#define _AUTH

// ===========================================================================
//  auth.h — Registro y login de usuario. Port de Plant (ESP-IDF).
// ===========================================================================
// Reglas idénticas al firmware Arduino: validación por carácter UTF-8,
// contraseña derivada con PBKDF2-HMAC-SHA256 (nunca en claro), salt por registro,
// comparación en tiempo constante, comando "**reset**" de fábrica (solo por AP)
// y límite de intentos (módulo session).

#include <string>
#include "Constants.h"   // requestStatus

namespace auth {

// Registro (POST /usercredentials): valida y guarda usuario + hash. En éxito
// marca hasRegisteredUser y devuelve STATUS_OK (el llamador emite el token).
requestStatus registerUser(const std::string& body);

// Login (POST /authusercredentials): valida credenciales contra las guardadas.
//   fromAP: ¿la petición entró por la interfaz del AP? (necesario para el reset).
// Devuelve STATUS_OK (login OK), HARD_RESET (reset ejecutado), o un error.
requestStatus loginUser(const std::string& body, bool fromAP);

} // namespace auth

#endif
