#ifndef _UTILS
#define _UTILS

// ===========================================================================
//  utils.h — Funciones utilitarias LIBRES (no ligadas a la clase Plant)
// ===========================================================================
// Helpers genéricos y puros: conversión BCD del RTC, validación de cadenas con
// paridad exacta con el formulario (conteo por carácter UTF-8), aritmética de
// fechas para el scheduling stateless, derivación/compración de contraseña y hex.
// Portado a ESP-IDF: se usa std::string en vez de la String de Arduino.

#include <cstdint>
#include <cstddef>
#include <string>
#include "Constants.h"    // validFrequencies (isValidFrequency)

// ---- RTC DS3231: conversión binario <-> BCD ----
uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);

// ---- Validación de cadenas (misma regla que el formulario JS) ----
// Letras (incl. acentos y ñ/Ñ), dígitos y (_-.@!#$%&*?+=). El espacio solo si allowSpaces.
bool isValidReadableString(const std::string& s, bool allowSpaces);
// Rechaza 4+ caracteres idénticos consecutivos (por carácter UTF-8 completo).
bool hasTooManyRepeatedChars(const std::string& s);
// ¿Dos o más espacios consecutivos? (equivale a /\s{2,}/ del formulario)
bool hasConsecutiveSpaces(const std::string& s);
// ¿La cadena es solo dígitos? (equivale a /^\d+$/; vacía = false)
bool isAllDigits(const std::string& s);

// ---- UTF-8 ----
int  utf8Len(const std::string& s);
int  utf8CharLen(unsigned char c);
bool isSpanishAccentUtf8(unsigned char lead, unsigned char cont);

// ---- Riego/ventilación ----
bool isValidFrequency(uint8_t f);
std::string frequencyToText(uint8_t hours);

// ---- Fecha ----
// Contador continuo de días desde una referencia fija (puro, monótono).
uint32_t daysSinceEpoch(uint16_t y, uint8_t m, uint8_t d);

// ---- Contraseña: derivación y comparación ----
// PBKDF2-HMAC-SHA256 con dkLen = 32 (un solo bloque). Sobre mbedtls_md_hmac()
// (API estable entre mbedTLS 2.x/3.x). False si parámetros inválidos o mbedTLS falla.
bool pbkdf2Sha256(const uint8_t* pass, size_t passLen,
                  const uint8_t* salt, size_t saltLen,
                  uint32_t iterations,
                  uint8_t* out, size_t outLen);

// Compara len bytes en tiempo CONSTANTE (no corta en el primer byte distinto).
bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len);

// ---- Hex ----
void bytesToHex(const uint8_t* in, size_t len, char* out);   // out: len*2+1
bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);

#endif
