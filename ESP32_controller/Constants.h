#ifndef _CONSTANTS
#define _CONSTANTS

// Pines ESP32 DEV
/*const byte whiteLedPin = 2; // Cambia según tus conexiones
const byte blueLedPin = 14;
const byte redLedPin = 15;
const byte buzzerPin = 4;
const byte waterPumpPin = 16;
const byte fanPin = 17;*/

// Pines ESP32-C3
const uint8_t whiteLedPin = 0;      // Cambia según tus conexiones
const uint8_t blueLedPin = 1;
const uint8_t redLedPin = 2;
const uint8_t buzzerPin = 3;
const uint8_t fanPin = 7;
const uint8_t waterPumpPin = 10;

// Canales PWM
const uint8_t whiteChannel = 0;     // Canal asignado a pwmPin1
const uint8_t blueChannel = 1;      // Canal asignado a pwmPin2
const uint8_t redChannel = 2;       // Canal asignado a pwmPin3

// Configuración PWM
const int pwmFrequency = 1000;      // Frecuencia en Hz
const uint8_t pwmResolution = 8;    // Resolución de 8 bits (0-255)
const uint8_t maxDutyCycle = 255;

const uint8_t zero = 0;

const uint8_t port80 = 80;
const uint8_t dnsPort = 53;

// ===== Sesión de edición =====
// Ventana durante la cual un usuario autenticado puede editar parámetros sin
// volver a introducir credenciales. TTL FIJO: se cuenta desde el login y la
// actividad NO lo renueva, así que la sesión caduca 30 min después de iniciar
// sesión. El token vive solo en RAM, así que un reinicio/corte de luz/reset lo
// invalida solo.
const uint32_t SESSION_TTL_MS = 30UL * 60UL * 1000UL; // 30 min

// ¡OJO CON EL ORDEN! _systemStatus se persiste en NVS con putBytes (por ÍNDICE,
// no por nombre): insertar, quitar o reordenar enumeradores reinterpreta los
// datos ya guardados en los equipos en campo. Renombrar SÍ es seguro; mover, no.
//
// Correspondencia con las claves JSON del portal:
//   ledAzul   -> blueDutyCycle    (PWM, 0-100 %)
//   ledRojo   -> redDutyCycle     (PWM, 0-100 %)
//   ledBlanco -> whiteLedOn       (ON/OFF, 0/1: salida digital, no PWM)
enum SystemStatus : uint8_t {
    hasRegisteredUser = 1,
    hasWifiCredentials,
    systemEnable,
    photoperiodOn,            // hora de prendido de las luces (0-23)
    photoperiodOff,           // hora de apagado de las luces (0-23). Se agrega al
    blueDutyCycle,
    redDutyCycle,
    whiteLedOn,               // LED blanco: salida DIGITAL (GPIO 0), no PWM. Se
                              // guarda normalizado a 0/1; cualquier valor > 0 enciende.
    irrigationFrequency,
    irrigationDuration,
    ventilationFrequency,
    ventilationDuration,
    cropWeek,                 // OBSOLETO como almacenamiento: cropWeek/cropDay ya
    cropDay,                  // no se guardan aquí; se DERIVAN del RTC + _cropStartDay
                              // (ver Plant::cropDayFromRtc). 
};

enum currentTime : uint8_t {
    second = 1,
    minute,
    hour,
    dayOfWeek,
    day,
    month,
    year,
    ctrl
};

// ===== Versión del firmware =====
// Se reporta en GET /getparams (clave "firmwareVersion") y la vista OTA del
// portal la muestra como "versión actual". Es una constante de COMPILACIÓN, no
// un valor en NVS, a propósito: la versión describe al binario que se está
// ejecutando. Si se guardara en NVS, tras un OTA el binario nuevo seguiría
// reportando la versión vieja hasta que alguien reescribiera ese registro.
// Así, cada binario dice exactamente lo que es.
// -> SUBIR ESTE VALOR en cada release que se vaya a distribuir por OTA.
// (const char* const: puntero const, para que el header pueda incluirse en
//  varias unidades de compilación sin colisión de símbolos en el enlazado.)
const char* const firmwareVersion = "1.0.0";

const uint8_t  DS3231Adress = 0x68;
const uint8_t rtcReadBytes = 7;

// Registro de estado del DS3231 y su bit OSF (Oscillator Stop Flag, bit 7).
// El chip pone OSF en 1 cuando el oscilador se detuvo en algún momento (primer
// arranque sin batería, celda agotada, pérdida total de alimentación). Mientras
// OSF valga 1 la hora NO es de fiar, aunque los registros contengan una fecha
// dentro de rango (un RTC virgen devuelve 2000-01-01, que "parece" válida). Se
// limpia al escribir la hora desde el navegador (setCurrentTime).
const uint8_t DS3231StatusReg = 0x0F;
const uint8_t DS3231OsfMask   = 0x80;

// ===== Límites de longitud de los campos (en caracteres / code points) =====
// Deben coincidir con las validaciones del formulario (String.length de JS).
const uint8_t minUsernameChars  = 4;
const uint8_t maxUsernameChars  = 32;
const uint8_t minUserpassChars  = 8;
const uint8_t maxUserpassChars  = 64;
const uint8_t minPlantNameChars = 3;
const uint8_t maxPlantNameChars = 20;

// ===== Límites de la red Wi-Fi del usuario (modo STA) =====
// SSID: 1–32 bytes (límite del estándar 802.11). Contraseña WPA2: 8–63 chars;
// una red ABIERTA no lleva contraseña (longitud 0). Coinciden con la validación
// del formulario (vista "wifi" en mainForm.html).
const uint8_t maxWifiSsidChars  = 32;
const uint8_t minWifiPassChars  = 8;
const uint8_t maxWifiPassChars  = 63;

// Peor caso UTF-8 de los caracteres permitidos: ASCII = 1 byte, vocales
// acentuadas y ñ/Ñ = 2 bytes. Los buffers se dimensionan a maxChars * 2 + 1
// (terminador nulo) para que un valor válido nunca se trunque al guardarlo.
const uint8_t utf8MaxBytesPerChar = 2;

enum requestStatus {
    STATUS_OK,
    HARD_RESET,
    INVALID_JSON,
    STORAGE_ERROR,
    MISSING_FIELDS,

    MISSING_CREDENTIALS,
    INVALID_CREDENTIALS,
    INVALID_USERNAME_LENGTH,
    INVALID_USERPASS_LENGTH,
    INVALID_USERNAME_CHARS,
    INVALID_USERPASS_CHARS,
    USERNAME_REPEATED_CHARS,
    USERPASS_REPEATED_CHARS,
    MISMATCH_CREDENTIALS,
    INVALID_SESSION,            // token de sesión ausente, inválido o expirado

    MISSING_PLANTNAME_FIELD,
    INVALID_PLANTNAME_LENGTH,
    INVALID_PLANTNAME_CHARS,
    PLANTNAME_REPEATED_CHARS,
    PLANTNAME_REPEATED_SPACES,
    PLANTNAME_ONLY_DIGITS,

    INVALID_PHOTOPERIOD_TYPE,
    INVALID_IRRIGATION_TYPE,
    INVALID_VENTILATION_TYPE,
    INVALID_LED_VALUE,          // azul/rojo fuera de 0-100 (canales PWM)
    INVALID_WHITE_LED_VALUE,    // blanco distinto de 0/1 (salida digital, ON/OFF)

    INVALID_SECOND_FORMAT,
    INVALID_MINUTE_FORMAT,
    INVALID_HOUR_FORMAT,
    INVALID_WEEKDAY_FORMAT,
    INVALID_DAY_FORMAT,
    INVALID_MONTH_FORMAT,
    INVALID_YEAR_FORMAT,

    // ===== Configuración de red Wi-Fi (POST /wificredentials) =====
    MISSING_WIFI_FIELDS,        // falta ssid en el cuerpo
    INVALID_SSID,               // SSID vacío o > 32 bytes
    INVALID_WIFI_PASS           // contraseña fuera de 8–63 (red segura)

};

// Intervalos válidos entre activaciones, EN HORAS (ya NO "veces/día"). El valor
// de irrH/ventH es directamente el intervalo: 3 = cada 3h (8 veces/día),
// 24 = diario, 48 = cada 2 días, 168 = semanal. 0 = apagado. Con este modelo el
// riego sub-diario y el espaciado usan la MISMA lógica en manageDevice() (módulo
// sobre un contador continuo de horas derivado del RTC). Tope 255 (uint8_t): un
// intervalo > ~10 días exigiría ampliar este arreglo y _systemStatus a uint16_t.
const uint8_t validFrequencies[] = {0, 1, 2, 3, 4, 6, 8, 12, 24, 48, 72, 168};

const uint8_t buzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t buzzerOff = 80;  // interval at which to blink (milliseconds)

const int intervalToSend = 30000;  //

// Cada cuánto se re-evalúa el control de luces/riego/ventilación en loop().
// turnOnDevices() solo depende del reloj (hora/minuto), así que 1 s sobra; lo
// importante es no bloquear server.handleClient() entre llamadas.
const uint32_t deviceUpdateInterval = 1000;

#endif