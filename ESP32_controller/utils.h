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

// ---- Fecha ----
// Contador continuo de días desde una referencia fija (puro, monótono). Lo usa
// manageDevice() para agendar intervalos multi-día sin guardar estado.
uint32_t daysSinceEpoch(uint16_t y, uint8_t m, uint8_t d);

// ---- Logs ----
// Enmascara una contraseña para imprimirla: primer char + **** + último.
String maskPassword(const char* pass);

#endif
