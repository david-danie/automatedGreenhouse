#include <Arduino.h>
#include <EEPROM.h>
#include "Constants.h"
#include "Plant.h"

Plant::Plant() {
  ledcSetup(pwmChannel0, pwmFrequency, pwmResolution);
  ledcSetup(pwmChannel2, pwmFrequency, pwmResolution);
  ledcSetup(pwmChannel4, pwmFrequency, pwmResolution);
  ledcAttachPin(blueLedPin, pwmChannel0);
  ledcAttachPin(redLedPin, pwmChannel2);
  ledcAttachPin(complementLedPin, pwmChannel4);
  pinMode(whiteLedPin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(deviceFourPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(whiteLedPin, LOW);
  digitalWrite(waterPumpPin, LOW);
  digitalWrite(fanPin, LOW);
  digitalWrite(deviceFourPin, LOW);
}

void Plant::setParameters(uint8_t* parameters) {
  for(uint8_t i = 1; i <= 20; i++)
    _systemStatus[i] = parameters[i];
  return;
}

uint8_t* Plant::getParameters() {
  return _systemStatus;
}

bool Plant::readParametersEEPROM() {
  EEPROM.begin(eepromBytes);
  if(EEPROM.read(systemActive) == 0) {
    _systemStatus[systemActive] = 0;
    _systemStatus[cropWeek] = 0;
    _systemStatus[cropDay] = 0;
    _systemStatus[photoperiod] = 8;
    _systemStatus[blueDutyCycle] = 3;
    _systemStatus[redDutyCycle] = 5;
    _systemStatus[complementDutyCycle] = 10;
    _systemStatus[irrigationTime] = 1;
    _systemStatus[irrigationTimeMinute] = 6;
    _systemStatus[fanTime] = 2;
    _systemStatus[fanTimeMinute] = 2;

    _systemStatus[second] = 50;
    _systemStatus[minute] = 59;
    _systemStatus[hour] = 23;
    _systemStatus[dayOfWeek] = 3;
    _systemStatus[day] = 21;
    _systemStatus[month] = 5;
    _systemStatus[year] = 36;
    _systemStatus[ctrl] = 0;
    return false;
  }
  else
    for (int i = 1; i <= 12; i++) 
      _systemStatus[i] = EEPROM.read(i);
  return true;
}

void Plant::turnOffDevices() {
  if(_systemStatus[hour] < _systemStatus[photoperiod]) {
    digitalWrite(whiteLedPin, HIGH);
    analogWrite(blueLedPin, map(_systemStatus[blueDutyCycle], zero, 20, zero, maxDutyCycle));
    analogWrite(redLedPin, map(_systemStatus[redDutyCycle], zero, 20, zero, maxDutyCycle));
    analogWrite(complementLedPin, map(_systemStatus[complementDutyCycle], zero, 20, zero, maxDutyCycle));
  }
  else {
    digitalWrite(whiteLedPin, LOW);
    analogWrite(blueLedPin, map(zero, zero, 20, zero, maxDutyCycle));
    analogWrite(redLedPin, map(zero, zero, 20, zero, maxDutyCycle));
    analogWrite(complementLedPin, map(zero, zero, 20, zero, maxDutyCycle));
  }
  switch (_systemStatus[irrigationTime]) {
  case onceAday:
    if(_systemStatus[hour] == zero && _systemStatus[minute] < _systemStatus[irrigationTimeMinute])
      analogWrite(waterPumpPin, HIGH);
    else
      analogWrite(waterPumpPin, LOW);
    break;
  case eachThreeHours:
    if((_systemStatus[hour] % 3 == zero || _systemStatus[hour] == zero) && _systemStatus[minute] < _systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  case eachEightHours:
    if((_systemStatus[hour] % 8 == zero || _systemStatus[hour] == zero) && _systemStatus[minute] < _systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  case eachHour:
    if(_systemStatus[minute] < _systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  default:
    digitalWrite(waterPumpPin, LOW);
  }
  switch (_systemStatus[fanTime]) {
  case onceAday:
    if(_systemStatus[hour] == zero && _systemStatus[minute] < _systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachThreeHours:
    if((_systemStatus[hour] % 3 == zero || _systemStatus[hour] == zero) && _systemStatus[minute] < _systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachEightHours:
    if((_systemStatus[hour] % 8 == zero || _systemStatus[hour] == zero) && _systemStatus[minute] < _systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachHour:
    if(_systemStatus[minute] < _systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  default:
    digitalWrite(fanPin, LOW);
  }
  /*if (_systemStatus[deviceFour] == zero)
    digitalWrite(deviceFourPin, LOW);
  else
    digitalWrite(deviceFourPin, HIGH);*/
  return;
}

char* Plant::sendParameters() {
  sprintf(buffer, "%02d/%02d/%02d %02d:%02d:%02d Pl:%d Week:%d Day:%d Fp:%d\nIrr:%dh/%dm Fan:%dh/%dm Valve:%dh/%dm B:%d%% R:%d%% C:%d%%\n",
    _systemStatus[day], _systemStatus[month], _systemStatus[year], _systemStatus[hour], _systemStatus[minute], _systemStatus[second],
    _systemStatus[systemActive], _systemStatus[cropWeek], _systemStatus[cropDay], _systemStatus[photoperiod], _systemStatus[irrigationTime], 
    _systemStatus[irrigationTimeMinute], _systemStatus[fanTime], _systemStatus[fanTimeMinute], _systemStatus[deviceFourHour], _systemStatus[deviceFourMinute],
    _systemStatus[blueDutyCycle] * factorOf100, _systemStatus[redDutyCycle] * factorOf100, _systemStatus[complementDutyCycle] * factorOf100);
    return buffer;
}

char* Plant::updateEEPROM(uint8_t condition) {
  if(condition == 'x') {
    for (uint8_t i = 1; i <= 15; i++) {
      EEPROM.write(i, zero);
      _systemStatus[i] = zero;
    }
    EEPROM.commit();  
    setBuzzer();
    return "Parameters deleted";
  }
  else if(condition == '1') {
    for (uint8_t i = 1; i <= 12; i++)
      EEPROM.write(i, _systemStatus[i]);
    EEPROM.commit();
    setBuzzer();
    return "Parameters saved";
  }
  else {
    EEPROM.write(zero, _systemStatus[systemActive]);
    EEPROM.commit();
    digitalWrite(buzzerPin, HIGH);
    delay(BuzzerOn);
    digitalWrite(buzzerPin, LOW);
    return "Parameters updated";
  }
}

bool Plant::addDay() {
  if (_systemStatus[hour] == 23 && _systemStatus[minute] == 59 && _systemStatus[second] >= 58) {
    if (_systemStatus[cropDay] <= 6) {
      _systemStatus[cropDay] += 1;
      //EEPROM.write(cropDay, _systemStatus[cropDay]);
    } 
    else {
      _systemStatus[cropDay] = 1;
      _systemStatus[cropWeek] += 1;
      //EEPROM.write(cropDay, _systemStatus[cropDay]);
      //EEPROM.write(cropWeek, _systemStatus[cropWeek]);
    }
    delay(3000);
    return true;
    //EEPROM.commit();
  }
  return false;
}

bool Plant::addDay(unsigned long currentMillis) {
  if (_systemStatus[hour] == 23 && _systemStatus[minute] == 59 && _systemStatus[second] >= 58) {
    if (currentMillis - _previousMillisAddDay >= 3000) {
      if (_systemStatus[cropDay] <= 6) {
        _systemStatus[cropDay] += 1;
      //EEPROM.write(cropDay, _systemStatus[cropDay]);
      }
      else {
        _systemStatus[cropDay] = 1;
        _systemStatus[cropWeek] += 1;
      //EEPROM.write(cropDay, _systemStatus[cropDay]);
      //EEPROM.write(cropWeek, _systemStatus[cropWeek]);
      }
      return true;
    }
    _previousMillisAddDay = currentMillis;
  }
  else {
    _previousMillisAddDay = zero;
    return false;
  }
}

void setBuzzer() {
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
  delay(BuzzerOff);
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
  delay(BuzzerOff);
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
}