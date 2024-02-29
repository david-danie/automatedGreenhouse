#include <Arduino.h>
#include <Wire.h>
#include "Constants.h"
#include "Clock.h"

Clock::Clock() {}

void Clock::startClock() {
  Wire.begin();
  Wire.beginTransmission(DS3231Adress);         // 1° Byte = Dirección del chip ds1307
  Wire.write(0x00);                             // 2° Byte = Dirección del registro de segundos
  for (uint8_t i = 1; i <= rtcReadBytes; i++)
    Wire.write(bin2bcd(zero));
  Wire.endTransmission();                       // Terminamos la escritura y verificamos si el DS1307 respondio
}

bool Clock::setTime(uint8_t* currentTime) {
  Wire.beginTransmission(DS3231Adress);         // 1° Byte = Dirección del chip ds1307
  Wire.write(0x00);                             // 2° Byte = Dirección del registro de segundos
    for (uint8_t i = 1; i <= rtcReadBytes; i ++)
      Wire.write(bin2bcd(currentTime[17 + i]));
  if (Wire.endTransmission() != zero)           // Terminamos la escritura y verificamos si el DS1307 respondio
    return true;
  return false;
}

bool Clock::getTime(uint8_t* currentTime) {
  Wire.beginTransmission(DS3231Adress);         // Inicia el protocolo en modo lectura.
  Wire.write(0x00);                             // Si la escritura se llevo a cabo el metodo endTransmission retorna 0
  if (Wire.endTransmission() != zero)           // Terminamos la escritura y verificamos si el DS1307 respondio
    return false;                               // Escribir la dirección del segundero
  Wire.requestFrom(DS3231Adress, rtcReadBytes); // Si el DS1307 esta presente, comenzar la lectura de 8 bytes
    for (uint8_t i = 1; i <= rtcReadBytes; i ++) 
      currentTime[17 + i] = bcd2bin(Wire.read());  
  return true;
}

uint8_t bcd2bin(uint8_t bcd) {
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin) {
  return (bin / 10 * 16) + (bin % 10);
}