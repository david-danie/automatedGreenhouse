//#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "Constants.h"
#include "Plant.h"
#include "mainHTML.h"
#include "updateSystemHTML.h"
#include "wellcomeHTML.h"
#include "sensible.h"

Plant::Plant(){

  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Error: No se pudo crear el mutex.");  // Reiniciar.
  }

  //ledcSetup(blueChannel, pwmFrequency, pwmResolution);
  //ledcSetup(redChannel, pwmFrequency, pwmResolution);
  //ledcSetup(whiteChannel, pwmFrequency, pwmResolution);

  ledcAttachChannel(blueLedPin, pwmFrequency, pwmResolution, blueChannel);
  ledcAttachChannel(redLedPin, pwmFrequency, pwmResolution, redChannel);
  ledcAttachChannel(whiteLedPin, pwmFrequency, pwmResolution, whiteChannel);
  //ledcAttachPin(blueLedPin, blueChannel);
  //ledcAttachPin(redLedPin, redChannel);
  //ledcAttachPin(whiteLedPin, whiteChannel);
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

  String mac = WiFi.macAddress();           // "24:6F:28:9A:2C:40"
  mac.replace(":", "");   
  macID = mac;
  Serial.print("***** ");  
  Serial.print(macID);                  // "246F289A2C40"
  Serial.println(" *****");  

  preferences.begin("system", true);  // Abrir en modo lectura
  preferences.getBytes("systemStatus", _systemStatus, sizeof(_systemStatus));

  preferences.begin("firmware", true);
  firmwareVersion = preferences.getString("version", "1.0.1");

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

char* Plant::getSystemStatus(char* buffer, size_t bufferSize){
  getCurrentTime();    // ************** CORREGIR ESTA LINEA
  Serial.println("Dispositivo:" + macID + " v" + firmwareVersion);
  if (mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    sprintf(buffer, "Estado:%s Sem:%d/dia:%d %02d/%02d/%02d %02d:%02d:%02d \nFotoperiodo:%dhr Azul:%d%% Roja:%d%% Blanca:%d%% \nIrrigación:%02d:%02d Ventilación:%02d:%02d\n",
      _systemStatus[systemEnable] ? "Activo" : "Inactivo", _systemStatus[cropWeek], _systemStatus[cropDay], 
      _currentTime[day], _currentTime[month], _currentTime[year], _currentTime[hour], _currentTime[minute], _currentTime[second],
      _systemStatus[photoperiod], _systemStatus[blueDutyCycle], _systemStatus[redDutyCycle], _systemStatus[whiteDutyCycle],
      _systemStatus[irrigationHour], _systemStatus[irrigationMinute], _systemStatus[ventilationHour], _systemStatus[ventilationMinute]
    );

    StaticJsonDocument<512> jsonDoc; 
    jsonDoc["mac"] = macID;
    jsonDoc["enable"] = _systemStatus[systemEnable];
    jsonDoc["fp"] = _systemStatus[photoperiod];
    jsonDoc["ledA"] = _systemStatus[blueDutyCycle];
    jsonDoc["ledR"] = _systemStatus[redDutyCycle];
    jsonDoc["ledW"] = _systemStatus[whiteDutyCycle];
    jsonDoc["irrH"] = _systemStatus[irrigationHour];
    jsonDoc["irrM"] = _systemStatus[irrigationMinute];
    jsonDoc["ventH"] = _systemStatus[ventilationHour];
    jsonDoc["ventM"] = _systemStatus[ventilationMinute];
    jsonDoc["sem"] = _systemStatus[cropWeek];
    jsonDoc["dia"] = _systemStatus[cropDay];
      
    
    xSemaphoreGive(mutex);
    // Generate the minified JSON and send it to the Serial port

    if(WiFi.status() == WL_CONNECTED) {
    // Tu URL de API Gateway
      http.begin("a1uhhs3bffugu4-ats.iot.us-east-2.amazonaws.com");
      http.addHeader("Content-Type", "application/json");

      String jsonString;
      serializeJson(jsonDoc, jsonString);
      
      // Enviar POST
      int httpCode = http.POST(jsonString);
      
      if(httpCode == 200) {
        String response = http.getString();
        Serial.println("Respuesta: " + response);
      }
      
      http.end();
    } 

    Serial.println(buffer);
    serializeJsonPretty(jsonDoc, buffer, bufferSize);
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
void Plant::manageDevice(int devicePin, int scheduleHour, int scheduleMinute) {
  bool activeDevice = false;

  switch (scheduleHour) {
    case WATERING_ONCE_A_DAY:
      activeDevice = (_currentTime[hour] == 0 && _currentTime[minute] < scheduleMinute);
      break;

    case WATERING_TWICE_A_DAY:
      activeDevice = ((_currentTime[hour] == 0 || _currentTime[hour] == 12) && _currentTime[minute] < scheduleMinute);
      break;

    case WATERING_4X_PER_DAY:
      activeDevice = ((_currentTime[hour] % 6 == 0) &&   _currentTime[minute] < scheduleMinute);  // 24 / 4 = cada 6 horas
      break;

    case WATERING_6X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 4 == 0) && _currentTime[minute] < scheduleMinute);  // 24 / 6 = cada 4 horas
      break;

    case WATERING_8X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 3 == 0) &&  _currentTime[minute] < scheduleMinute);  // 24 / 8 = cada 3 horas
      break;

    case WATERING_12X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 2 == 0) &&   _currentTime[minute] < scheduleMinute);  // 24 / 12 = cada 2 horas
      break;

    case WATERING_EVERY_HOUR:
      activeDevice = (_currentTime[minute] < scheduleMinute);
      break;

    default:
      activeDevice = false;
  }

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

bool Plant::getToken() {

  char loginPath[100];
  char bodyRequest[100];

  strcpy(loginPath, SERVER_URL);
  strcat(loginPath, "device/login");
  strcpy(bodyRequest, userName);
  strcat(bodyRequest, userPass);
  
  http.begin(loginPath);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  sprintf(bodyRequest, "username=%s&password=%s&client_id=%s&client_secret=%s", userName, userPass, macID, userPass);
  
  int httpCode = http.POST(bodyRequest);
  //Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      jwtToken = doc["accessToken"].as<String>();
      Serial.printf("[AUTH] Código HTTP: %d. Token:%s \n", httpCode, jwtToken.c_str());
      return true;
    }
  }
  Serial.println("[AUTH] HTTP error code " + String(httpCode) + ": " + http.errorToString(httpCode));
  return false;
}

void Plant::downloadOTA() {

  char firmwarePath[50];
  strcpy(firmwarePath, FIRMWARE_URL);
  strcat(firmwarePath, "firmware");
  
  http.begin(firmwarePath);

  // Incluir el token en el header Authorization
  http.addHeader("Authorization", "Bearer " + jwtToken);
  http.addHeader("version", firmwareVersion);

    // Indica qué header recoger
  const char* keys[] = { "version" };
  http.collectHeaders(keys, 1);
  int httpCode = http.GET();


  
  if (httpCode == 200) {
    int contentLength = http.getSize();
    /*int headers = http.headers();
    Serial.println("[DEBUG] Headers en respuesta:");
    for (int i = 0; i < headers; i++) {
      Serial.print(http.headerName(i));
      Serial.print(": ");
      Serial.println(http.header(i));
    }*/
    String otaVersion = http.header("version");
    Serial.println("[OTA] Nueva version: v" + otaVersion);
    Serial.println("[OTA] Descargando actualización...");
    
    WiFiClient& stream = http.getStream();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      size_t written = Update.writeStream(stream);
      if (written == contentLength) 
        Serial.println("[OTA] Binario descargado completamente.");      
      else 
        Serial.printf("[OTA] Descarga incompleta (%d/%d bytes)\n", written, contentLength);
      if (Update.end() && Update.isFinished()) {
        Serial.println("[OTA] ¡Actualización exitosa!");

        Serial.print("[OTA] Guardando en preferences. v" + otaVersion + '\n');
        preferences.begin("firmware", false);
        preferences.putString("version", otaVersion);
        preferences.end();
        
        Serial.println("[OTA] Reiniciando dispositivo");
        ESP.restart();
      } 
      else 
        Serial.println("[OTA] Error al finalizar la actualización.\n");
      
    } 
    else 
      Serial.println("[OTA] No se pudo iniciar la actualización.\n");
    
  } 
  else if (httpCode == 204) 
    Serial.printf("[OTA] Código HTTP: %d. No hay actualización disponible.\n\n", httpCode);
  else
    Serial.printf("[OTA] Código HTTP: %d. No se realizó la descarga del binario.\n\n", httpCode);
  http.end();
}




//bool testCredentials(String SSID, String pass);


uint8_t bcd2bin(uint8_t bcd){
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin){
  return (bin / 10 * 16) + (bin % 10);
}

