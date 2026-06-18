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

/*const byte hasRegisteredUser = 1;
const byte hasWifiCredentials = 2;
const byte systemEnable = 3;      //index
const byte photoperiod = 4;       //index
const byte blueDutyCycle = 5;     //index
const byte redDutyCycle = 6;      //index
const byte whiteDutyCycle = 7;    //index
const byte irrigationHour = 8;
const byte irrigationMinute = 9;
const byte ventilationHour = 10;
const byte ventilationMinute = 11;
const byte cropWeek = 12;
const byte cropDay = 13;*/

enum SystemStatus : uint8_t {
    hasRegisteredUser = 1,
    hasWifiCredentials,
    systemEnable,
    photoperiodOn,            // hora de prendido de las luces (0-23)
    photoperiodOff,            // hora de apagado de las luces (0-23). 
    blueDutyCycle,
    redDutyCycle,
    whiteDutyCycle,
    irrigationFrequency,
    irrigationDuration,
    ventilationFrequency,
    ventilationDuration,
    cropWeek,
    cropDay,
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

const uint8_t  DS3231Adress = 0x68;
const uint8_t rtcReadBytes = 7;

// ===== Límites de longitud de los campos (en caracteres / code points) =====
// Deben coincidir con las validaciones del formulario (String.length de JS).
const uint8_t minUsernameChars  = 4;
const uint8_t maxUsernameChars  = 32;
const uint8_t minUserpassChars  = 8;
const uint8_t maxUserpassChars  = 64;
const uint8_t minPlantNameChars = 3;
const uint8_t maxPlantNameChars = 20;

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

    MISSING_PLANTNAME_FIELD,
    INVALID_PLANTNAME_LENGTH,
    INVALID_PLANTNAME_CHARS,
    PLANTNAME_REPEATED_CHARS,
    PLANTNAME_REPEATED_SPACES,
    PLANTNAME_ONLY_DIGITS,

    INVALID_PHOTOPERIOD_TYPE,
    INVALID_IRRIGATION_TYPE,
    INVALID_VENTILATION_TYPE,
    INVALID_LED_VALUE,

    INVALID_SECOND_FORMAT,
    INVALID_MINUTE_FORMAT,
    INVALID_HOUR_FORMAT,
    INVALID_WEEKDAY_FORMAT,
    INVALID_DAY_FORMAT,
    INVALID_MONTH_FORMAT,
    INVALID_YEAR_FORMAT
};

// Frecuencias válidas (veces/día). Deben dividir exactamente 24 para poder
// traducirse a un intervalo entero de horas en manageDevice().
const uint8_t validFrequencies[] = {0, 1, 2, 3, 4, 6, 8, 12, 24};

const uint8_t buzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t buzzerOff = 80;  // interval at which to blink (milliseconds)

const uint16_t intervalToSend = 30000;  // 
// Cada cuánto se re-evalúa el control de luces/riego/ventilación en loop().
// turnOnDevices() solo depende del reloj (hora/minuto), así que 1 s sobra; lo
// importante es no bloquear server.handleClient() entre llamadas.
const uint16_t deviceUpdateInterval = 1000;

#endif