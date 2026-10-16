// ===========================================================================
//  utils.cpp — Implementación de los helpers libres (ver utils.h)
//  Port ESP-IDF: std::string en vez de la String de Arduino. La lógica es la
//  misma que el firmware Arduino de referencia (ESP32_controller/utils.cpp).
// ===========================================================================
#include "utils.h"
#include <cctype>     // isalnum, isdigit
#include <cstring>    // strlen, memcpy
#include "mbedtls/md.h"   // mbedtls_md_hmac: API estable en mbedTLS 2.x y 3.x

// ---- RTC DS3231: conversión binario <-> BCD ----
uint8_t bcd2bin(uint8_t bcd){
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin){
  return (bin / 10 * 16) + (bin % 10);
}

// Cuenta code points UTF-8 ignorando bytes de continuación (10xxxxxx), para que
// los límites de longitud coincidan con String.length de JS.
int utf8Len(const std::string& s) {
  int count = 0;
  for (size_t i = 0; i < s.length(); i++) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) count++;
  }
  return count;
}

int utf8CharLen(unsigned char c) {
  if (c < 0x80) return 1;            // 0xxxxxxx (ASCII)
  if ((c & 0xE0) == 0xC0) return 2;  // 110xxxxx
  if ((c & 0xF0) == 0xE0) return 3;  // 1110xxxx
  if ((c & 0xF8) == 0xF0) return 4;  // 11110xxx
  return 1;                          // byte inválido: avanza 1 para no atascarse
}

bool hasConsecutiveSpaces(const std::string& s) {
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] == ' ' && s[i - 1] == ' ') return true;
  }
  return false;
}

bool isAllDigits(const std::string& s) {
  if (s.length() == 0) return false;
  for (size_t i = 0; i < s.length(); i++) {
    if (!isdigit((unsigned char)s[i])) return false;
  }
  return true;
}

// Vocales acentuadas y ñ/Ñ del español en UTF-8 (2 bytes, lead 0xC3).
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

bool isValidReadableString(const std::string& s, bool allowSpaces) {
  int n = (int)s.length();
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

// Rechaza 4+ caracteres idénticos consecutivos, por carácter UTF-8 completo.
bool hasTooManyRepeatedChars(const std::string& s) {
  int n = (int)s.length();
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

// Intervalo en horas -> etiqueta legible (ver validFrequencies). No incluye la duración.
std::string frequencyToText(uint8_t hours) {
  if (hours == 0)         return "Apagado";
  if (hours < 24)         return "Cada " + std::to_string(hours) + "h";
  if (hours == 24)        return "Diario";
  if (hours == 168)       return "Semanal";
  if (hours % 24 == 0)    return "Cada " + std::to_string(hours / 24) + " dias";
  return "Cada " + std::to_string(hours) + "h";
}

// Días desde una referencia fija ("days from civil" de Howard Hinnant): puro y
// monótono. Contador continuo para agendar intervalos multi-día sin NVS.
uint32_t daysSinceEpoch(uint16_t y, uint8_t m, uint8_t d) {
  y -= (m <= 2);                                                   // mar..feb
  uint16_t era = y / 400;
  uint16_t yoe = y - era * 400;                                    // [0, 399]
  uint16_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   // [0, 365]
  uint32_t doe = (uint32_t)yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
  return (uint32_t)era * 146097UL + doe;
}

// ===========================================================================
//  Contraseña: PBKDF2-HMAC-SHA256 y comparación en tiempo constante
// ===========================================================================

bool pbkdf2Sha256(const uint8_t* pass, size_t passLen,
                  const uint8_t* salt, size_t saltLen,
                  uint32_t iterations,
                  uint8_t* out, size_t outLen) {
  // Solo dkLen == tamaño del hash: PBKDF2 se reduce a UN bloque.
  if (!pass || !salt || !out) return false;
  if (outLen != pwHashBytes) return false;
  if (saltLen == 0 || saltLen > pbkdf2MaxSaltBytes) return false;
  if (iterations == 0) return false;

  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info) return false;

  // U1 = HMAC(pass, salt || INT32BE(1)).
  uint8_t salted[pbkdf2MaxSaltBytes + 4];
  memcpy(salted, salt, saltLen);
  salted[saltLen + 0] = 0x00;
  salted[saltLen + 1] = 0x00;
  salted[saltLen + 2] = 0x00;
  salted[saltLen + 3] = 0x01;

  uint8_t u[pwHashBytes];
  if (mbedtls_md_hmac(info, pass, passLen, salted, saltLen + 4, u) != 0)
    return false;
  memcpy(out, u, pwHashBytes);

  // DK = U1 xor U2 xor ... xor Uc, con Ui = HMAC(pass, Ui-1).
  uint8_t tmp[pwHashBytes];
  for (uint32_t i = 1; i < iterations; i++) {
    if (mbedtls_md_hmac(info, pass, passLen, u, pwHashBytes, tmp) != 0)
      return false;
    memcpy(u, tmp, pwHashBytes);
    for (size_t k = 0; k < pwHashBytes; k++)
      out[k] ^= u[k];
  }
  return true;
}

bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len) {
  if (!a || !b) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < len; i++)
    diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

// ===========================================================================
//  Hex
// ===========================================================================

void bytesToHex(const uint8_t* in, size_t len, char* out) {
  static const char* digits = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2]     = digits[(in[i] >> 4) & 0x0F];
    out[i * 2 + 1] = digits[in[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

bool hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
  if (!hex || !out) return false;
  if (strlen(hex) != outLen * 2) return false;

  for (size_t i = 0; i < outLen; i++) {
    uint8_t nibbles[2];
    for (uint8_t k = 0; k < 2; k++) {
      char c = hex[i * 2 + k];
      if (c >= '0' && c <= '9')      nibbles[k] = (uint8_t)(c - '0');
      else if (c >= 'a' && c <= 'f') nibbles[k] = (uint8_t)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') nibbles[k] = (uint8_t)(c - 'A' + 10);
      else return false;
    }
    out[i] = (uint8_t)((nibbles[0] << 4) | nibbles[1]);
  }
  return true;
}
