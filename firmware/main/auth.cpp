// ===========================================================================
//  auth.cpp — Registro y login de usuario. Ver auth.h.
//  Port fiel de validateUserCredentials/authUserCredentials del firmware Arduino.
// ===========================================================================
#include "auth.h"
#include <ArduinoJson.h>
#include <cstring>
#include "esp_random.h"
#include "esp_log.h"
#include "utils.h"
#include "storage.h"
#include "session.h"

static const char* TAG = "auth";
static const uint8_t FLAG_HAS_USER = 0x01;   // bit0 de CropConfig.flags

namespace auth {

requestStatus registerUser(const std::string& body) {
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, body)) return INVALID_JSON;
    if (!doc.containsKey("user") || !doc.containsKey("pass")) return MISSING_CREDENTIALS;

    std::string user = doc["user"] | "";
    std::string pass = doc["pass"] | "";
    // trim equivalente
    auto trim = [](std::string& s){ size_t a=s.find_first_not_of(" \t\r\n");
        size_t b=s.find_last_not_of(" \t\r\n");
        s = (a==std::string::npos)? "" : s.substr(a, b-a+1); };
    trim(user); trim(pass);

    // Validación por carácter UTF-8 (igual que el formulario).
    if (utf8Len(user) < minUsernameChars || utf8Len(user) > maxUsernameChars)
        return INVALID_USERNAME_LENGTH;
    if (utf8Len(pass) < minUserpassChars || utf8Len(pass) > maxUserpassChars)
        return INVALID_USERPASS_LENGTH;
    if (!isValidReadableString(user, false)) return INVALID_USERNAME_CHARS;
    if (!isValidReadableString(pass, false)) return INVALID_USERPASS_CHARS;
    if (hasTooManyRepeatedChars(user)) return USERNAME_REPEATED_CHARS;
    if (hasTooManyRepeatedChars(pass)) return USERPASS_REPEATED_CHARS;

    // Salt nuevo de 128 bits (esp_random) + derivación PBKDF2 (nunca en claro).
    uint8_t salt[pwSaltBytes];
    for (uint8_t i = 0; i < pwSaltBytes; i += 4) {
        uint32_t r = esp_random();
        memcpy(salt + i, &r, (pwSaltBytes - i >= 4) ? 4 : (pwSaltBytes - i));
    }
    uint8_t hash[pwHashBytes];
    if (!pbkdf2Sha256((const uint8_t*)pass.c_str(), pass.length(),
                      salt, pwSaltBytes, pbkdf2Iterations, hash, pwHashBytes))
        return STORAGE_ERROR;

    if (!storage::saveAuth(salt, hash, user.c_str())) return STORAGE_ERROR;

    // Marca hasRegisteredUser en la config del cultivo.
    CropConfig cfg;
    storage::loadConfig(cfg);
    cfg.flags |= FLAG_HAS_USER;
    if (!storage::saveConfig(cfg)) return STORAGE_ERROR;

    ESP_LOGI(TAG, "Usuario registrado (contraseña derivada con PBKDF2)");
    return STATUS_OK;
}

requestStatus loginUser(const std::string& body, bool fromAP) {
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, body)) return INVALID_JSON;
    if (!doc.containsKey("user") || !doc.containsKey("pass")) return MISSING_CREDENTIALS;

    std::string user = doc["user"] | "";
    std::string pass = doc["pass"] | "";
    auto trim = [](std::string& s){ size_t a=s.find_first_not_of(" \t\r\n");
        size_t b=s.find_last_not_of(" \t\r\n");
        s = (a==std::string::npos)? "" : s.substr(a, b-a+1); };
    trim(user); trim(pass);

    // Comando RESET: vía de recuperación, SOLO por el AP. Se evalúa antes del
    // bloqueo y de validar credenciales (no compara contra las guardadas).
    if (pass == "**reset**") {
        if (!fromAP) return RESET_REQUIRES_AP;
        storage::eraseAll();     // borra cfg + auth + wifi (reset de fábrica)
        session::clear();
        ESP_LOGW(TAG, "FACTORY RESET");
        return HARD_RESET;
    }

    // Bloqueo por intentos ANTES de derivar el hash (evita DoS por el coste del PBKDF2).
    if (session::loginLocked()) return TOO_MANY_ATTEMPTS;

    if (utf8Len(user) < minUsernameChars || utf8Len(user) > maxUsernameChars)
        return INVALID_USERNAME_LENGTH;
    if (utf8Len(pass) < minUserpassChars || utf8Len(pass) > maxUserpassChars)
        return INVALID_USERPASS_LENGTH;

    // Carga las credenciales guardadas.
    uint8_t salt[pwSaltBytes], hash[pwHashBytes];
    char savedUser[maxUsernameChars * utf8MaxBytesPerChar + 1] = {0};
    if (!storage::loadAuth(salt, hash, savedUser, sizeof(savedUser)))
        return MISMATCH_CREDENTIALS;   // no hay credenciales guardadas

    if (user != savedUser) {
        session::registerLoginFailure();
        return MISMATCH_CREDENTIALS;
    }

    uint8_t intento[pwHashBytes];
    if (!pbkdf2Sha256((const uint8_t*)pass.c_str(), pass.length(),
                      salt, pwSaltBytes, pbkdf2Iterations, intento, pwHashBytes))
        return STORAGE_ERROR;

    if (!constantTimeEquals(intento, hash, pwHashBytes)) {
        session::registerLoginFailure();
        return MISMATCH_CREDENTIALS;
    }

    session::resetLoginFailures();
    return STATUS_OK;
}

} // namespace auth
