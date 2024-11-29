#ifndef _CONSTANTS
#define _CONSTANTS

// Pines 
const byte whiteLedPin = 2; // Cambia según tus conexiones
const byte blueLedPin = 14;
const byte redLedPin = 15;
const byte buzzerPin = 4;
const byte waterPumpPin = 16;
const byte fanPin = 17;

// Canales PWM
const int whiteChannel1 = 0; // Canal asignado a pwmPin1
const int blueChannel2 = 1; // Canal asignado a pwmPin2
const int redChannel3 = 2; // Canal asignado a pwmPin3

// Configuración PWM
const int pwmFreq = 1000; // Frecuencia en Hz
const byte pwmResolution = 8; // Resolución de 8 bits (0-255)

const byte zero = 0;

const char deviceName[] = "CropController";

const byte port80 = 80;
const byte dnsPort = 53;

const byte systemEnable = 1;      //index
const byte photoperiod = 2;       //index
const byte blueDutyCycle = 3;     //index
const byte redDutyCycle = 4;      //index
const byte whiteDutyCycle = 5;    //index
const byte irrigationHour = 6;
const byte irrigationMinute = 7;
const byte ventilationHour = 8;
const byte ventilationMinute = 9;
const byte cropWeek = 12;
const byte cropDay = 13;

const byte second = 1;
const byte minute = 2;
const byte hour = 3;
const byte dayOfWeek = 4;
const byte day = 5;
const byte month = 6;
const byte year = 7;
const byte ctrl = 8;

const uint8_t  DS3231Adress = 0x68;
const byte rtcReadBytes = 7;



const byte onceAday = 1; // 8 times
const byte eachThreeHours = 2; // 8 times
const byte eachEightHours = 3; // 3 times
const byte eachHour = 4; // 24 times

const uint8_t BuzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t BuzzerOff = 80;  // interval at which to blink (milliseconds)

const int intervalToSend = 5000;  // interval at which to blink (milliseconds)

#endif