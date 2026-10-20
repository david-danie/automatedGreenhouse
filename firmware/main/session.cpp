// ===========================================================================
//  session.cpp — Sesión con token + límite de intentos. Ver session.h.
//  Port del firmware Arduino: misma semántica (TTL fijo, espera creciente).
//  millis() -> esp_timer_get_time()/1000 (ms de uptime).
// ===========================================================================
#include "session.h"
#include <cstdio>
#include <cstring>
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "Constants.h"

static const char* TAG = "session";

// Uptime en ms (equivalente a millis() de Arduino).
static uint32_t millisNow() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// Estado en RAM.
static char     s_token[33] = {0};
static uint32_t s_expiresAt = 0;
static uint8_t  s_failedLogins = 0;
static uint32_t s_lockoutUntil = 0;

namespace session {

std::string issueToken() {
    char buf[33];
    for (int i = 0; i < 4; i++)
        snprintf(buf + i * 8, 9, "%08x", (unsigned)esp_random());
    buf[32] = '\0';
    strncpy(s_token, buf, sizeof(s_token) - 1);
    s_token[sizeof(s_token) - 1] = '\0';
    s_expiresAt = millisNow() + SESSION_TTL_MS;
    return std::string(s_token);
}

bool isValid(const std::string& token) {
    if (s_token[0] == '\0') return false;                 // sin sesión activa
    if (token.length() != 32) return false;
    if (token != s_token) return false;
    return (int32_t)(millisNow() - s_expiresAt) < 0;      // aún no expira
}

void clear() {
    s_token[0] = '\0';
    s_expiresAt = 0;
}

// ---- Límite de intentos de login ----
bool loginLocked() {
    if (s_lockoutUntil == 0) return false;
    if ((int32_t)(millisNow() - s_lockoutUntil) < 0) return true;
    s_lockoutUntil = 0;                                   // ventana ya pasó
    return false;
}

void registerLoginFailure() {
    if (s_failedLogins < 255) s_failedLogins++;
    if (s_failedLogins < loginMaxAttempts) return;

    uint32_t espera = loginLockoutBaseMs;
    for (uint8_t i = loginMaxAttempts; i < s_failedLogins && espera < loginLockoutMaxMs; i++)
        espera *= 2;
    if (espera > loginLockoutMaxMs) espera = loginLockoutMaxMs;

    s_lockoutUntil = millisNow() + espera;
    ESP_LOGW(TAG, "%u intentos fallidos: login bloqueado %lu s",
             (unsigned)s_failedLogins, (unsigned long)(espera / 1000));
}

void resetLoginFailures() {
    s_failedLogins = 0;
    s_lockoutUntil = 0;
}

} // namespace session
