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
    photoperiod,
    blueDutyCycle,
    redDutyCycle,
    whiteDutyCycle,
    irrigationFrequency,
    irrigationDuration,
    ventilationFrequency,
    ventilationDuration,
    cropWeek,
    cropDay
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

    INVALID_PHOTOPERIOD_TYPE,
    INVALID_IRRIGATION_TYPE,
    INVALID_VENTILATION_TYPE,

    INVALID_SECOND_FORMAT,
    INVALID_MINUTE_FORMAT,
    INVALID_HOUR_FORMAT,
    INVALID_WEEKDAY_FORMAT,
    INVALID_DAY_FORMAT,
    INVALID_MONTH_FORMAT,
    INVALID_YEAR_FORMAT
 
};

enum ActivationFrequency {
    FREQ_NONE,
    FREQ_ONCE_A_DAY,
    FREQ_TWICE_A_DAY,
    FREQ_4X_PER_DAY,
    FREQ_6X_PER_DAY,
    FREQ_8X_PER_DAY,
    FREQ_12X_PER_DAY,
    FREQ_EVERY_HOUR
};

const uint8_t buzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t buzzerOff = 80;  // interval at which to blink (milliseconds)

const int intervalToSend = 30000;  // 

#endif