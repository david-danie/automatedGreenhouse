#ifndef _UTILS
#define _UTILS

// ===========================================================================
//  utils.h — Funciones utilitarias LIBRES (no ligadas a la clase Plant)
// ===========================================================================
// Helpers genéricos y puros: conversión BCD del RTC, validación de cadenas con
// paridad exacta con el formulario (conteo por carácter UTF-8), aritmética de
// fechas para el scheduling stateless y enmascarado de contraseñas para logs.
// Se separan de Plant para mantener la clase enfocada y dejar estos helpers
// reutilizables/testeables. La capa HTTP (HttpResponse / buildHttpResponse) NO
// vive aquí: está acoplada a requestStatus y se queda en Plant.h.

#include <Arduino.h>      // String
#include "Constants.h"    // validFrequencies (isValidFrequency)

// ---- RTC DS3231: conversión binario <-> BCD ----
uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);

// ---- Validación de cadenas (misma regla que el formulario JS) ----
// Mismo set de caracteres que el formulario: letras (incl. acentos y ñ/Ñ),
// dígitos y (_-.@!#$%&*?+=). El espacio solo se admite si allowSpaces.
bool isValidReadableString(const String& s, bool allowSpaces);
// Rechaza 4+ caracteres idénticos consecutivos (por carácter UTF-8 completo).
bool hasTooManyRepeatedChars(const String& s);
// ¿Dos o más espacios consecutivos? (equivale a /\s{2,}/ del formulario)
bool hasConsecutiveSpaces(const String& s);
// ¿La cadena es solo dígitos? (equivale a /^\d+$/; vacía = false)
bool isAllDigits(const String& s);

// ---- UTF-8 ----
// Cuenta caracteres (code points), ignorando bytes de continuación.
int  utf8Len(const String& s);
// Longitud en bytes del carácter UTF-8 que empieza con el byte c.
int  utf8CharLen(unsigned char c);
// ¿(lead, cont) forman una vocal acentuada o ñ/Ñ del español (UTF-8, 2 bytes)?
bool isSpanishAccentUtf8(unsigned char lead, unsigned char cont);

// ---- Riego/ventilación ----
// ¿El intervalo (horas entre activaciones) es uno de validFrequencies?
bool isValidFrequency(uint8_t f);
// Traduce el intervalo en horas a una etiqueta legible para logs/UI:
// 0="Apagado", <24="Cada Nh", 24="Diario", 168="Semanal", múltiplos de 24=
// "Cada N dias", y cualquier otro="Cada Nh". No incluye la duración.
String frequencyToText(uint8_t hours);

// ---- Fecha ----
// Contador continuo de días desde una referencia fija (puro, monótono). Lo usa
// manageDevice() para agendar intervalos multi-día sin guardar estado.
uint32_t daysSinceEpoch(uint16_t y, uint8_t m, uint8_t d);

// ---- Logs ----
// (maskPassword se eliminó: la contraseña ya no vive en RAM, solo su hash.)

// ---- Contraseña: derivación y comparación ----
// PBKDF2-HMAC-SHA256 con dkLen = 32 (un solo bloque, ver la implementación).
// Se monta sobre mbedtls_md_hmac() a propósito: la API de `mbedtls/md.h` es estable
// entre mbedTLS 2.x y 3.x, mientras que mbedtls_pkcs5_pbkdf2_hmac() cambió de firma
// (quedó deprecada en favor de la variante _ext), lo que ataría el firmware a la
// versión del core.
// Devuelve false si los parámetros no son válidos o si mbedTLS falla.
bool pbkdf2Sha256(const uint8_t* pass, size_t passLen,
                  const uint8_t* salt, size_t saltLen,
                  uint32_t iterations,
                  uint8_t* out, size_t outLen);

// Compara len bytes en tiempo CONSTANTE: no corta en el primer byte distinto, para
// no filtrar por tiempo cuántos bytes del hash acertó un atacante.
bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len);

// ---- Hex ----
// Escribe len*2 caracteres en minúscula + terminador. `out` debe tener len*2+1.
void bytesToHex(const uint8_t* in, size_t len, char* out);
// Convierte outLen*2 caracteres hex a bytes. False si hay algún carácter inválido
// o la cadena no mide exactamente outLen*2.
bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);

#endif
