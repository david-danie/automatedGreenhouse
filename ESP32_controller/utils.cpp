// ===========================================================================
//  utils.cpp — Implementación de los helpers libres (ver utils.h)
// ===========================================================================
#include "utils.h"
#include <ctype.h>    // isalnum, isdigit
#include <string.h>   // strlen

// ---- RTC DS3231: conversión binario <-> BCD ----
uint8_t bcd2bin(uint8_t bcd){
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin){
  return (bin / 10 * 16) + (bin % 10);
}

// Cuenta caracteres (code points) en una cadena UTF-8, ignorando los bytes de
// continuación (10xxxxxx). Así "Jalapeño" cuenta 8 y no 9, y los límites de
// longitud coinciden con los del formulario (String.length de JS).
int utf8Len(const String& s) {
  int count = 0;
  for (int i = 0; i < s.length(); i++) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) count++;
  }
  return count;
}

// Longitud en bytes del carácter UTF-8 que empieza con el byte c.
int utf8CharLen(unsigned char c) {
  if (c < 0x80) return 1;            // 0xxxxxxx (ASCII)
  if ((c & 0xE0) == 0xC0) return 2;  // 110xxxxx
  if ((c & 0xF0) == 0xE0) return 3;  // 1110xxxx
  if ((c & 0xF8) == 0xF0) return 4;  // 11110xxx
  return 1;                          // byte inválido: avanza 1 para no atascarse
}

// ¿Hay dos o más espacios consecutivos? Equivale a /\s{2,}/ del formulario
// (los demás caracteres de espacio ya los rechaza isValidReadableString).
bool hasConsecutiveSpaces(const String& s) {
  for (int i = 1; i < s.length(); i++) {
    if (s[i] == ' ' && s[i - 1] == ' ') return true;
  }
  return false;
}

// ¿La cadena está formada únicamente por dígitos? Equivale a /^\d+$/ del
// formulario (cadena vacía cuenta como "no solo dígitos").
bool isAllDigits(const String& s) {
  if (s.length() == 0) return false;
  for (int i = 0; i < s.length(); i++) {
    if (!isdigit((unsigned char)s[i])) return false;
  }
  return true;
}

// Vocales acentuadas y ñ/Ñ del español en UTF-8: secuencias de 2 bytes cuyo
// primer byte (lead) es 0xC3. Devuelve true si (lead, cont) forman una de
// esas letras: á é í ó ú ü Á É Í Ó Ú Ü ñ Ñ.
bool isSpanishAccentUtf8(unsigned char lead, unsigned char cont) {
  if (lead != 0xC3) return false;
  switch (cont) {
    case 0xA1: case 0xA9: case 0xAD: case 0xB3: case 0xBA: case 0xBC: // á é í ó ú ü
    case 0x81: case 0x89: case 0x8D: case 0x93: case 0x9A: case 0x9C: // Á É Í Ó Ú Ü
    case 0xB1: case 0x91:                                             // ñ Ñ
      return true;
  }
  return false;
}

// Mismo set de caracteres que el formulario: letras (incluidas vocales
// acentuadas y ñ/Ñ del español), dígitos y (_-.@!#$%&*?+=). El espacio solo se
// admite cuando allowSpaces es true (p. ej. el nombre de la planta).
bool isValidReadableString(const String& s, bool allowSpaces) {
  int n = s.length();
  for (int i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];

    if (isalnum(c)) continue; // alfanumérico ASCII

    if (c == '_' || c == '-' || c == '.' || c == '@' ||
        c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
        c == '*' || c == '?' || c == '+' || c == '=')
      continue;

    if (allowSpaces && c == ' ') continue;

    // Vocal acentuada o ñ/Ñ (UTF-8, 2 bytes): se aceptan ambos bytes.
    if (i + 1 < n && isSpanishAccentUtf8(c, (unsigned char)s[i + 1])) {
      i++; // consume el segundo byte de la secuencia
      continue;
    }

    return false;
  }
  return true;
}

// Rechaza 4 o más caracteres idénticos consecutivos, comparando por carácter
// UTF-8 completo (no byte a byte), para que repetidos acentuados como "ññññ" o
// "áááá" se detecten igual que en el formulario (count > 3).
bool hasTooManyRepeatedChars(const String& s) {
  int n = s.length();
  int count = 1;
  int prevStart = 0;
  int prevLen = 0; // 0 = aún no hay carácter previo

  for (int i = 0; i < n; ) {
    int len = utf8CharLen((unsigned char)s[i]);
    if (i + len > n) len = n - i; // secuencia truncada al final

    bool same = false;
    if (prevLen == len) {
      same = true;
      for (int k = 0; k < len; k++) {
        if (s[prevStart + k] != s[i + k]) { same = false; break; }
      }
    }

    if (same) {
      count++;
      if (count > 3) return true;
    } else {
      count = 1;
    }

    prevStart = i;
    prevLen = len;
    i += len;
  }
  return false;
}

bool isValidFrequency(uint8_t f) {
  for (uint8_t v : validFrequencies)
    if (f == v) return true;
  return false;
}

/**
 * @brief Días transcurridos desde una fecha de referencia fija.
 *
 * Algoritmo "days from civil" de Howard Hinnant: PURO (sin estado) y MONÓTONO
 * (crece de 1 en 1 cada día, sin reiniciarse). Sirve de contador continuo para
 * que manageDevice() agende intervalos multi-día sin guardar nada en NVS. Como
 * solo se usa vía módulo, el origen exacto de la cuenta es irrelevante. Válido
 * para los años del RTC (2000-2099), todos positivos.
 */
uint32_t daysSinceEpoch(uint16_t y, uint8_t m, uint8_t d) {
  y -= (m <= 2);                                                   // mar..feb
  uint16_t era = y / 400;
  uint16_t yoe = y - era * 400;                                    // [0, 399]
  uint16_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   // [0, 365]
  uint32_t doe = (uint32_t)yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
  return (uint32_t)era * 146097UL + doe;
}

String maskPassword(const char* pass) {
    if (!pass || pass[0] == '\0') return "";

    int len = strlen(pass);
    if (len <= 2) return "**";

    String masked;
    masked.reserve(len); // evita reallocs

    masked += pass[0];
    masked += "****";
    masked += pass[len - 1];

    return masked;
}
