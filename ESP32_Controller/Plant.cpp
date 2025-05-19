//#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Constants.h"
#include "Plant.h"
#include "mainHTML.h"
#include "updateSystemHTML.h"
#include "wellcomeHTML.h"

Plant::Plant(){

  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Error: No se pudo crear el mutex.");  // Reiniciar.
  }

  ledcSetup(blueChannel, pwmFrequency, pwmResolution);
  ledcSetup(redChannel, pwmFrequency, pwmResolution);
  ledcSetup(whiteChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(blueLedPin, blueChannel);
  ledcAttachPin(redLedPin, redChannel);
  ledcAttachPin(whiteLedPin, whiteChannel);
  pinMode(whiteLedPin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  //pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(whiteLedPin, LOW);
  digitalWrite(waterPumpPin, LOW);
  digitalWrite(fanPin, LOW);
  ledcWrite(whiteChannel, zero);
  ledcWrite(blueChannel, zero);
  ledcWrite(redChannel, zero);

}

void Plant::begin(){

  preferences.begin("system", true);  // Abrir en modo lectura
  preferences.getBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
  preferences.end();

  startClock();
  
  _systemStatus[hasUserRegistered] = true;

}

byte Plant::processPostBody(String body){

  StaticJsonDocument<512> jsonDoc;                                // Ajusta el tamaño si tu JSON es más grande
  DeserializationError error = deserializeJson(jsonDoc, body);    // Intenta parsear el body como JSON

  
  if (error) {                                                    // Verifica si hubo errores durante el parsing
    Serial.print("Error al parsear JSON: ");
    Serial.println(error.c_str());
    return 0;
  }

  String pass = jsonDoc["pass"];
  String user = jsonDoc["user"];
  
  if (pass != "p@$$w0rd" || user != "useruser")
    return 0;
  if (pass == "dr0w$$@p" && user == "useruser") //dr0w$$@p
    return 1;
  if (mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    _systemStatus[systemEnable]  = jsonDoc["enable"];
    _systemStatus[photoperiod]  = jsonDoc["fp"];
    _systemStatus[blueDutyCycle]  = jsonDoc["ledA"];
    _systemStatus[redDutyCycle]  = jsonDoc["ledR"];
    _systemStatus[whiteDutyCycle]  = jsonDoc["ledW"];
    _systemStatus[irrigationHour]  = jsonDoc["irrH"];
    _systemStatus[irrigationMinute]  = jsonDoc["irrM"];
    _systemStatus[ventilationHour]  = jsonDoc["ventH"];
    _systemStatus[ventilationMinute]  = jsonDoc["ventM"];
    
    _currentTime[second] = jsonDoc["seg"];
    _currentTime[minute] = jsonDoc["min"];
    _currentTime[hour] = jsonDoc["hr"];
    _currentTime[dayOfWeek] = jsonDoc["diaSem"];
    _currentTime[day] = jsonDoc["dia"];
    _currentTime[month] = jsonDoc["mes"];
    _currentTime[year] = jsonDoc["anio"];

    preferences.begin("system", false);  // Abrir el espacio "system" en modo escritura
    preferences.putBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
    preferences.end();  // Cerrar el espacio
    setCurrentTime();
    xSemaphoreGive(mutex);
  }
  else {
    Serial.print("Error: no se pudo actualizar el sistema.\n");
    return 0;
  }
  return 2;

}

char* Plant::getSystemStatus(char* buffer){
  getCurrentTime();
  if (mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    sprintf(buffer, "Estado:%s Sem:%d/dia:%d %02d/%02d/%02d %02d:%02d:%02d \nFotoperiodo:%dhr Azul:%d%% Roja:%d%% Blanca:%d%% \nIrrigación:%02d:%02d Ventilación:%02d:%02d\n",
      _systemStatus[systemEnable] ? "Activo" : "Inactivo", _systemStatus[cropWeek], _systemStatus[cropDay], 
      _currentTime[day], _currentTime[month], _currentTime[year], _currentTime[hour], _currentTime[minute], _currentTime[second],
      _systemStatus[photoperiod], _systemStatus[blueDutyCycle], _systemStatus[redDutyCycle], _systemStatus[whiteDutyCycle],
      _systemStatus[irrigationHour], _systemStatus[irrigationMinute], _systemStatus[ventilationHour], _systemStatus[ventilationMinute]
    );
    xSemaphoreGive(mutex);
  }
  else {
    sprintf(buffer, "Error: no se pudo obtener el estado del sistema.\n");
  }
  return buffer;
}
//void readSystemStatus(byte* systemStatus, size_t length);

void Plant::startClock(){
  Wire.begin();
  Wire.beginTransmission(DS3231Adress);         // 1° Byte = Dirección del chip ds1307
  Wire.write(0x00);                             // 2° Byte = Dirección del registro de segundos
  for (uint8_t i = 1; i <= rtcReadBytes; i++)
    Wire.write(bin2bcd(zero));
  Wire.endTransmission();    
}
bool Plant::setCurrentTime(){
  Wire.beginTransmission(DS3231Adress);         // 1° Byte = Dirección del chip ds1307
  Wire.write(0x00);                             // 2° Byte = Dirección del registro de segundos
    for (uint8_t i = 1; i <= rtcReadBytes; i ++)
      Wire.write(bin2bcd(_currentTime[i]));
  if (Wire.endTransmission() != zero)           // Terminamos la escritura y verificamos si el DS1307 respondio
    return true;
  return false;
}
bool Plant::getCurrentTime(){
  Wire.beginTransmission(DS3231Adress);         // Inicia el protocolo en modo lectura.
  Wire.write(0x00);                             // Si la escritura se llevo a cabo el metodo endTransmission retorna 0
  if (Wire.endTransmission() != zero)           // Terminamos la escritura y verificamos si el DS1307 respondio
    return false;                               // Escribir la dirección del segundero
  Wire.requestFrom(DS3231Adress, rtcReadBytes); // Si el DS1307 esta presente, comenzar la lectura de 8 bytes
    for (uint8_t i = 1; i <= rtcReadBytes; i ++) 
      _currentTime[i] = bcd2bin(Wire.read());  
  return true;
}

void Plant::turnOffDevices(){
  // ** Control de Luces **
  if (_currentTime[hour] < _systemStatus[photoperiod]) {
    // Ajusta las luces según los duty cycles configurados
    ledcWrite(whiteChannel, map(_systemStatus[whiteDutyCycle], 0, 100, 0, maxDutyCycle));
    ledcWrite(blueChannel, map(_systemStatus[blueDutyCycle], 0, 100, 0, maxDutyCycle));
    ledcWrite(redChannel, map(_systemStatus[redDutyCycle], 0, 100, 0, maxDutyCycle));
  } else {
    // Apaga todas las luces
    ledcWrite(whiteChannel, zero);
    ledcWrite(blueChannel, zero);
    ledcWrite(redChannel, zero);
  }

  // ** Control de Riego y Ventilación **
  manageDevice(waterPumpPin, _systemStatus[irrigationHour], _systemStatus[irrigationMinute]);
  manageDevice(fanPin, _systemStatus[ventilationHour], _systemStatus[ventilationMinute]);

}
/**
 * @brief Controla un dispositivo en base a la configuración horaria y de minutos.
 * 
 * @param devicePin       Pin del dispositivo (bomba o ventilador).
 * @param scheduleHour    Frecuencia de activación (onceAday, eachThreeHours, etc.).
 * @param scheduleMinute  Duración en minutos del encendido.
 */
void Plant::manageDevice(int devicePin, int scheduleHour, int scheduleMinute){
  bool activeDevice = false; // Variable para determinar si el dispositivo debe estar encendido.
  switch (scheduleHour) {
    case onceAday:
      activeDevice = (_currentTime[hour] == 0 && _currentTime[minute] < scheduleMinute);
      break;
    case eachThreeHours:
      activeDevice = (_currentTime[hour] % 3 == 0 && _currentTime[minute] < scheduleMinute);
      break;
    case eachEightHours:
      activeDevice = (_currentTime[hour] % 8 == 0 && _currentTime[minute] < scheduleMinute);
      break;
    case eachHour:
      activeDevice = (_currentTime[minute] < scheduleMinute);
      break;
    default:
      activeDevice = false; // Si no se reconoce el caso, apaga el dispositivo.
  }
  // Actualiza el estado del dispositivo según la lógica evaluada.
  digitalWrite(devicePin, activeDevice ? HIGH : LOW);
}

bool Plant::getRegisteredUser(){
  return _systemStatus[hasUserRegistered];
}

String Plant::mainHTML(){

  String mainHTML;
  String dispositivoActivo = (_systemStatus[systemEnable] == 1) ? "Activo" : "Inactivo";
  mainHTML += controllerInfo;
  mainHTML += "{\n";
  mainHTML += "dispositivoActivo: \"" + dispositivoActivo + "\",\n";
  mainHTML += "fotoperiodo: " + String(_systemStatus[photoperiod]) + ",\n";
  mainHTML += "semana: " + String(_systemStatus[cropWeek]) + ",\n";
  mainHTML += "dias: " + String(_systemStatus[cropDay]) + ",\n";
  mainHTML += "luzAzul: " + String(_systemStatus[blueDutyCycle]) + ",\n";
  mainHTML += "luzRoja: " + String(_systemStatus[redDutyCycle]) + ",\n";
  mainHTML += "luzBlanca: " + String(_systemStatus[whiteDutyCycle]) + ",\n";
  mainHTML += "horasIrrigacion: " + String(_systemStatus[irrigationHour]) + ",\n";
  mainHTML += "minutosIrrigacion: " + String(_systemStatus[irrigationMinute]) + ",\n";
  mainHTML += "horasVentilador: " + String(_systemStatus[ventilationHour]) + ",\n";
  mainHTML += "minutosVentilador: " + String(_systemStatus[ventilationMinute]) + "\n";
  mainHTML += "};";
  mainHTML += controllerInfo2;
  return mainHTML;

}

String Plant::updateHTML(){
  String updateHTML;
  String dispositivoActivo = (_systemStatus[systemEnable] == 1) ? "Activo" : "Inactivo";
  updateHTML += updateForm;
  updateHTML += "{\n";
  updateHTML += "dispositivoActivo: \"" + dispositivoActivo + "\",\n";
  updateHTML += "fotoperiodo: " + String(_systemStatus[photoperiod]) + ",\n";
  updateHTML += "semana: " + String(_systemStatus[cropWeek]) + ",\n";
  updateHTML += "dias: " + String(_systemStatus[cropDay]) + ",\n";
  updateHTML += "luzAzul: " + String(_systemStatus[blueDutyCycle]) + ",\n";
  updateHTML += "luzRoja: " + String(_systemStatus[redDutyCycle]) + ",\n";
  updateHTML += "luzBlanca: " + String(_systemStatus[whiteDutyCycle]) + ",\n";
  updateHTML += "horasIrrigacion: " + String(_systemStatus[irrigationHour]) + ",\n";
  updateHTML += "minutosIrrigacion: " + String(_systemStatus[irrigationMinute]) + ",\n";
  updateHTML += "horasVentilador: " + String(_systemStatus[ventilationHour]) + ",\n";
  updateHTML += "minutosVentilador: " + String(_systemStatus[ventilationMinute]) + "\n";
  updateHTML += "};";
  updateHTML += updateForm2;
  return updateHTML;
}

String Plant::wellcomeHTML(){
  return welcomePage;
}

String Plant::registerUserHTML(){
  return registerPage;
}

String Plant::exitHTML(){
  return exitPage;
}

void Plant::updateCropDay(){
  static int ultimoSegundoProcesado = -1; // Guarda el último segundo procesado
  // Verifica si ya se procesó este segundo
  if (_currentTime[second] == ultimoSegundoProcesado) {
    return; // Evita procesar varias veces en el mismo segundo
  }
  // Actualiza el último segundo procesado
  ultimoSegundoProcesado = _currentTime[second];
  // Verifica si es el último segundo del día
  if (_currentTime[hour] == 23 && _currentTime[minute] == 59 && _currentTime[second] >= 58) {
    _systemStatus[cropDay]++; // Incrementa el día
    if (_systemStatus[cropDay] > 6) { // Si es el séptimo día
      _systemStatus[cropDay] = 0;    // Reinicia el día
      _systemStatus[cropWeek]++;   // Incrementa la semana
    }
  }
}

//bool testCredentials(String SSID, String pass);


uint8_t bcd2bin(uint8_t bcd){
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin){
  return (bin / 10 * 16) + (bin % 10);
}

