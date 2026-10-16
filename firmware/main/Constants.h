#ifndef _CONSTANTS
#define _CONSTANTS

#include <cstdint>
#include <cstddef>

// Niveles lógicos de salida. En IDF no existen HIGH/LOW de Arduino; se definen
// como 1/0 y se usan con gpio_set_level().
// ===== Polaridad de las salidas ON/OFF (relés/contactores y LED blanco) =====
// El nivel que ENCIENDE vive SOLO aquí. Configuración actual: LÓGICA DIRECTA
// (el pin en alto enciende). Si el hardware fuese activo en bajo, basta
// intercambiar estas dos líneas y todo el firmware queda coherente.
const uint32_t deviceOn  = 1;
const uint32_t deviceOff = 0;

// ===== Pines (ESP32-C3) =====
const int whiteLedPin = 0;
const int blueLedPin  = 1;
const int redLedPin   = 2;
const int buzzerPin   = 3;
const int fanPin      = 7;
const int waterPumpPin = 10;

// ===== PWM (LEDC) =====
// Solo azul y rojo son PWM; el blanco es salida digital (GPIO 0), sin canal.
const int blueChannel = 1;          // LEDC channel para el azul
const int redChannel  = 2;          // LEDC channel para el rojo
const int pwmFrequency = 1000;      // Hz
const uint8_t pwmResolution = 8;    // bits (0-255)
const uint8_t maxDutyCycle = 255;
const uint8_t zero = 0;

// ===== Sesión de edición =====
// TTL FIJO desde el login; la actividad NO lo renueva. Token solo en RAM.
const uint32_t SESSION_TTL_MS = 30UL * 60UL * 1000UL; // 30 min

// ¡OJO CON EL ORDEN! _systemStatus se persiste en NVS por ÍNDICE: insertar,
// quitar o reordenar enumeradores reinterpreta lo ya guardado. Renombrar es
// seguro; mover/cambiar la cantidad, no (borra la flash antes de cargar).
//   ledAzul -> blueDutyCycle (PWM 0-100), ledRojo -> redDutyCycle (PWM 0-100),
//   ledBlanco -> whiteLedOn (ON/OFF 0/1, salida digital).
enum SystemStatus : uint8_t {
    hasRegisteredUser = 1,
    hasWifiCredentials,
    systemEnable,
    photoperiodOn,            // hora de prendido de las luces (0-23)
    photoperiodOff,           // hora de apagado de las luces (0-23)
    blueDutyCycle,
    redDutyCycle,
    whiteLedOn,               // LED blanco digital (GPIO 0): 0/1, >0 enciende
    irrigationFrequency,
    irrigationDuration,
    ventilationFrequency,
    ventilationDuration,
    systemStatusCount         // centinela: marca el tamaño necesario
};

enum currentTime : uint8_t {
    second = 1,
    minute,
    hour,
    dayOfWeek,
    day,
    month,
    year,
    currentTimeCount          // centinela: marca el tamaño necesario
};

// ===== Versión del firmware =====
// Constante de compilación (no NVS): cada binario reporta lo que realmente es.
// SUBIR en cada release distribuido por OTA.
const char* const firmwareVersion = "1.0.0";

// ===== Hash de la contraseña (PBKDF2-HMAC-SHA256) =====
const uint8_t pwSaltBytes = 16;
const uint8_t pwHashBytes = 32;
const uint8_t pwSaltHexLen = pwSaltBytes * 2;
const uint8_t pwHashHexLen = pwHashBytes * 2;
const uint32_t pbkdf2Iterations = 20000;   // PENDIENTE: cronometrar en placa
const uint8_t pbkdf2MaxSaltBytes = 32;

// ===== Límite de intentos de login (contador en RAM) =====
const uint8_t  loginMaxAttempts   = 5;
const uint32_t loginLockoutBaseMs = 5000;
const uint32_t loginLockoutMaxMs  = 300000;

// ===== DS3231 (I²C) =====
const uint8_t DS3231Adress = 0x68;
const uint8_t rtcReadBytes = 7;
// Bit OSF (Oscillator Stop Flag) del registro de estado: mientras valga 1, la
// hora NO es fiable (RTC virgen o pérdida de alimentación).
const uint8_t DS3231StatusReg = 0x0F;
const uint8_t DS3231OsfMask   = 0x80;

// ===== Límites de longitud (en caracteres / code points) =====
const uint8_t minUsernameChars  = 4;
const uint8_t maxUsernameChars  = 32;
const uint8_t minUserpassChars  = 8;
const uint8_t maxUserpassChars  = 64;
const uint8_t minPlantNameChars = 3;
const uint8_t maxPlantNameChars = 20;

// ===== Wi-Fi del usuario (STA) =====
const uint8_t maxWifiSsidChars  = 32;
const uint8_t minWifiPassChars  = 8;
const uint8_t maxWifiPassChars  = 63;

// Peor caso UTF-8 de los caracteres permitidos (ASCII=1B, acentos/ñ=2B).
const uint8_t utf8MaxBytesPerChar = 2;

enum requestStatus {
    STATUS_OK,
    HARD_RESET,
    NEW_CROP_DONE,
    RESET_REQUIRES_AP,
    INVALID_JSON,
    STORAGE_ERROR,
    MISSING_FIELDS,

    MISSING_CREDENTIALS,
    INVALID_USERNAME_LENGTH,
    INVALID_USERPASS_LENGTH,
    INVALID_USERNAME_CHARS,
    INVALID_USERPASS_CHARS,
    USERNAME_REPEATED_CHARS,
    USERPASS_REPEATED_CHARS,
    MISMATCH_CREDENTIALS,
    TOO_MANY_ATTEMPTS,
    INVALID_SESSION,

    MISSING_PLANTNAME_FIELD,
    INVALID_PLANTNAME_LENGTH,
    INVALID_PLANTNAME_CHARS,
    PLANTNAME_REPEATED_CHARS,
    PLANTNAME_REPEATED_SPACES,
    PLANTNAME_ONLY_DIGITS,

    INVALID_PHOTOPERIOD_TYPE,
    INVALID_IRRIGATION_TYPE,
    INVALID_VENTILATION_TYPE,
    INVALID_IRRIGATION_DURATION,
    INVALID_VENTILATION_DURATION,
    INVALID_LED_VALUE,
    INVALID_WHITE_LED_VALUE,

    INVALID_SECOND_FORMAT,
    INVALID_MINUTE_FORMAT,
    INVALID_HOUR_FORMAT,
    INVALID_WEEKDAY_FORMAT,
    INVALID_DAY_FORMAT,
    INVALID_MONTH_FORMAT,
    INVALID_YEAR_FORMAT,

    MISSING_WIFI_FIELDS,
    INVALID_SSID,
    INVALID_WIFI_PASS
};

// Intervalos válidos entre activaciones, EN HORAS. 0=apagado, 24=diario,
// 168=semanal. Tope 255 (uint8_t).
const uint8_t validFrequencies[] = {0, 1, 2, 3, 4, 6, 8, 12, 24, 48, 72, 168};

// ===== Buzzer (activo, GPIO 3) =====
const uint16_t buzzerBeepMs = 330;   // 330 ms ON / 330 ms OFF por pitido
const uint8_t buzzerBeepsBoot    = 1;
const uint8_t buzzerBeepsParams  = 3;
const uint8_t buzzerBeepsRtcFail = 4;
const uint8_t buzzerBeepsError   = 2;

// ===== Cadencias del bucle de control =====
const uint32_t deviceUpdateInterval = 1000;   // re-evaluar actuadores cada 1 s
const uint32_t systemLogInterval = 5000;      // log de estado cada 5 s

#endif
