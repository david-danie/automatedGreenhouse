#ifndef Constants_h
#define Constants_h

const uint8_t zero = 0;
const uint8_t messageMaxLenght = 60;

const uint8_t buzzerPin = 2;
const uint8_t blueLedPin = 3; // Pin asociado a el led azul
const uint8_t redLedPin = 4;
const uint8_t complementLedPin = 5;
const uint8_t whiteLedPin = 6;
const uint8_t waterPumpPin = 7;
const uint8_t fanPin = 8;
const uint8_t deviceFourPin = 9;

// setting PWM properties
const int pwmFrequency = 1000;
const uint8_t pwmChannel0 = 0;
const uint8_t pwmChannel2 = 2;
const uint8_t pwmChannel4 = 4;
const uint8_t pwmResolution = 8;
const uint8_t maxDutyCycle = 255;
const uint8_t factorOf100 = 5;

const uint8_t systemActive = 1; //index
const uint8_t photoperiod = 2; //index
const uint8_t blueDutyCycle = 3; //index
const uint8_t redDutyCycle = 4; //index
const uint8_t complementDutyCycle = 5; //index
const uint8_t irrigationTime = 6;
const uint8_t irrigationTimeMinute = 7;
const uint8_t fanTime = 8;
const uint8_t fanTimeMinute = 9;
const uint8_t deviceFourHour = 10; //index
const uint8_t deviceFourMinute = 11;
const uint8_t cropWeek = 12;
const uint8_t cropDay = 13;

const uint8_t second = 18;
const uint8_t minute = 19;
const uint8_t hour = 20;
const uint8_t dayOfWeek = 21;
const uint8_t day = 22;
const uint8_t month = 23;
const uint8_t year = 24;
const uint8_t ctrl = 25;
const uint8_t DS3231Adress = 0x68;
const uint8_t rtcReadBytes = 7;

const uint8_t eepromBytes = 12;

const uint8_t port80 = 80;

const uint8_t onceAday = 1; // 8 times
const uint8_t eachThreeHours = 2; // 8 times
const uint8_t eachEightHours = 3; // 3 times
const uint8_t eachHour = 4; // 24 times

const uint8_t BuzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t BuzzerOff = 80;  // interval at which to blink (milliseconds)

const int intervalToUpdate = 1000;  // interval at which to blink (milliseconds)
const int intervalToUpdateDay = 3000;  // interval at which to blink (milliseconds)

#endif