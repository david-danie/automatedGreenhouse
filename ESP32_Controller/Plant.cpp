//#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
//#include <ctype.h>
#include "esp_mac.h" 
#include "Constants.h"
#include "Plant.h"
#include "showParametersForm.h"
#include "updateParametersForm.h"
#include "sensible.h"


Plant::Plant(){

  ledcAttachChannel(blueLedPin, pwmFrequency, pwmResolution, blueChannel);
  ledcAttachChannel(redLedPin, pwmFrequency, pwmResolution, redChannel);
  ledcAttachChannel(whiteLedPin, pwmFrequency, pwmResolution, whiteChannel);

  //pinMode(whiteLedPin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  //pinMode(buzzerPin, OUTPUT);
  digitalWrite(whiteLedPin, LOW);
  digitalWrite(waterPumpPin, LOW);
  digitalWrite(fanPin, LOW);
  //digitalWrite(buzzerPin, LOW);
  ledcWrite(whiteChannel, zero);
  ledcWrite(blueChannel, zero);
  ledcWrite(redChannel, zero);

  uint8_t mac[6];

  if (esp_efuse_mac_get_default(mac) == ESP_OK) {
    snprintf(_MAC, sizeof(_MAC), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  
}

void Plant::begin(){

  Serial.begin(115200);
  Wire.begin();
  delay(500);

  p.begin("system", true);  // Abrir en modo lectura
  p.getBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
  p.end();

  p.begin("plantData", true);
  p.getString("plantName", _plantName, sizeof(_plantName));
  p.end();
  if (strlen(_plantName) == 0) 
    strlcpy(_plantName, "", sizeof(_plantName));


  /*preferences.begin("firmware", true);
  firmwareVersion = preferences.getString("version", "1.0.1");
  p.end();*/

  p.begin("config", true);
  p.getString("username", _username, sizeof(_username));
  p.getString("userpass", _userpass, sizeof(_userpass));

  p.end();

  Serial.printf("\n===================== ESTADO DEL SISTEMA =====================\n");

  Serial.printf("[SYS] MAC:%s | Usuario:%s | Sistema:%s\n",
                _MAC,
                _systemStatus[hasRegisteredUser] ? "REGISTRADO" : "NO REGISTRADO",
                _systemStatus[systemEnable] ? "ACTIVO" : "INACTIVO");

  Serial.printf("[AUTH] Username:%s | Pass:%s\n", _username, maskPassword(_userpass).c_str());

  /*Serial.printf("[WIFI] SSID:%s | Pass:%s | Conexion:%s\n",
                ssid,
                maskPassword(wifiPass).c_str(),
                _systemStatus[hasWifiCredentials] ? "SI" : "NO");*/

  Serial.printf("[LUZ] Fotoperiodo:%dh | Azul:%d%% | Roja:%d%% | Blanca:%d%%\n",
                _systemStatus[photoperiod],
                _systemStatus[blueDutyCycle],
                _systemStatus[redDutyCycle],
                _systemStatus[whiteDutyCycle]);

  Serial.printf("[RIEGO] Frecuencia:%02d veces al dia por %02d minutos\n",
                _systemStatus[irrigationFrequency],
                _systemStatus[irrigationDuration]);

  Serial.printf("[VENT] Frecuencia:%02d veces al dia por %02d minutos\n",
                _systemStatus[ventilationFrequency],
                _systemStatus[ventilationDuration]);

  Serial.printf("[CULTIVO] %02d/%02d/%02d %02d:%02d:%02d Semana:%d | Dia:%d\n",
                _currentTime[day], _currentTime[month], _currentTime[year], 
                _currentTime[hour], _currentTime[minute], _currentTime[second],
                _systemStatus[cropWeek],
                _systemStatus[cropDay]);
  Serial.printf("[CROP] Planta:%s\n", _plantName);

  Serial.printf("================================\n\n");

  //startClock();

}

bool Plant::getRegisteredUser(){
  return _systemStatus[hasRegisteredUser];
}

requestStatus Plant::validateUserCredentials(const String& body) {

    // ---- 1. Parseo del JSON ----
    StaticJsonDocument<100> doc;
    DeserializationError err = deserializeJson(doc, body);

    if (err) {
        return INVALID_JSON;
    }

    if (!doc.containsKey("user") || !doc.containsKey("pass")) { // Los campos no llegan
        return MISSING_CREDENTIALS;
    }

    String username = doc["user"] | "";
    username.trim();
    String userpass = doc["pass"] | "";
    userpass.trim();

    // ---- 2. Comando especial RESET ----
    if (userpass == "**reset**") {
      hardReset();
      return HARD_RESET;
    }

    // ---- 3. Validación de longitud ----
    if (username.length() < 4 || username.length() > 32) //4-31
      return INVALID_USERNAME_LENGTH; 
    if (userpass.length() < 8 || userpass.length() > 64) //8-63
      return INVALID_USERPASS_LENGTH; 

    // ---- 4. Validación de caracteres permitidos ----
    if (!isValidReadableString(username, false))
      return INVALID_USERNAME_CHARS;
    if (!isValidReadableString(userpass, false)) 
      return INVALID_USERPASS_CHARS; 

    // ---- 4. Validación de caracteres permitidos ----
    if (hasTooManyRepeatedChars(username))
      return USERNAME_REPEATED_CHARS;
    if (hasTooManyRepeatedChars(userpass))
      return USERPASS_REPEATED_CHARS;

    strlcpy(_username, username.c_str(), sizeof(_username));
    strlcpy(_userpass, userpass.c_str(), sizeof(_userpass));

    // ---- 6. Guardar en Preferences -
    if (!p.begin("config", false))
        return STORAGE_ERROR;
  
    p.putString("username", _username);
    p.putString("userpass", _userpass);
    p.end();
    Serial.printf("\nUser:%s Password:%s Guardados\n", username, userpass);

    // Leer inmediatamente después de guardar
    /*p.begin("config", true);
    String u = p.getString("user", "---");
    String pa = p.getString("pass", "---");
    p.end();*/

    if (!p.begin("system", false)) 
        return STORAGE_ERROR;
  
    _systemStatus[hasRegisteredUser]  = 1;  //*****************
    p.putBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
    p.end();
    return STATUS_OK;

}
/**
 * @brief Valida los parametros que llegan desde el formulario HTML.
 * 1. Que  contenga los campos "user" y "pass"
 * 2. Si el password es igual a "reset" se hace un reset de fabrica 
 * 
 * 
 * @param body  Es el cuerpo del request, llega en texto plano
 */
requestStatus Plant::validateCropParameters(const String& body) {

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) 
    return INVALID_JSON;
  
  if (!doc.containsKey("planta")  ||
      !doc.containsKey("enable")  ||
      !doc.containsKey("fp")      ||
      !doc.containsKey("ledA")    ||
      !doc.containsKey("ledR")    ||
      !doc.containsKey("ledB")    ||
      !doc.containsKey("irrH")    ||
      !doc.containsKey("irrM")    ||
      !doc.containsKey("ventH")   ||
      !doc.containsKey("ventM"))
    return MISSING_FIELDS;

  if (!doc.containsKey("user") || !doc.containsKey("pass")) 
    return MISSING_CREDENTIALS;
  
  String username = doc["user"] | "";
  username.trim();
  String userpass = doc["pass"] | "";
  userpass.trim();

  // ---- 2. Comando especial RESET ----
  if (userpass == "reset") {
    hardReset();
    return HARD_RESET;
  }
      // ---- 3. Validación de longitud ----
  if (username.length() < 5 || username.length() > 32) //4-31
    return INVALID_USERNAME_LENGTH; 
  if (userpass.length() < 8 || userpass.length() > 64) //8-63
    return INVALID_USERPASS_LENGTH; 

  // ---- 4. Validación de caracteres permitidos ----
  if (!isValidReadableString(username, false))
    return INVALID_USERNAME_CHARS;
  if (!isValidReadableString(userpass, false)) 
    return INVALID_USERPASS_CHARS; 

  // ---- 4. Validación de caracteres permitidos ----
  if (hasTooManyRepeatedChars(username))
    return USERNAME_REPEATED_CHARS;
  if (hasTooManyRepeatedChars(userpass))
    return USERPASS_REPEATED_CHARS;

  /*********** NO LEER username Y userpass PORQUE SE LEEN DESDE EL INICIO DEL PROGRAMA ******* REVISAR */
  /*p.begin("config", true);
  String userSaved = p.getString("username", "");
  String passSaved = p.getString("userpass", "");
  p.end();*/

  if (username != _username || userpass != _userpass) 
    return MISMATCH_CREDENTIALS;

  //Validaciones de planta
  //if (!doc.containsKey("planta")) 
  //  return MISSING_PLANTNAME_FIELD;

  String plantNameBuff = doc["planta"] | "";
  plantNameBuff.trim();
  if (plantNameBuff.length() < 3 || plantNameBuff.length() > 23)
    return INVALID_PLANTNAME_LENGTH;
  if (!isValidReadableString(plantNameBuff, true)) 
      return INVALID_PLANTNAME_CHARS;
  if (hasTooManyRepeatedChars(plantNameBuff))
    return PLANTNAME_REPEATED_CHARS;
  strlcpy(_plantName, plantNameBuff.c_str(), sizeof(_plantName));

  if (!doc["fp"].is<uint8_t>())
    return INVALID_PHOTOPERIOD_TYPE;
  if (!doc["irrH"].is<uint8_t>() || !doc["irrM"].is<uint8_t>())
    return INVALID_IRRIGATION_TYPE;
  if (!doc["ventH"].is<uint8_t>() || !doc["ventM"].is<uint8_t>())
    return INVALID_VENTILATION_TYPE;

  // Validaciones combinadas (tipo + rango en una sola línea)
  if (!doc["seg"].is<uint8_t>() || doc["seg"] > 59)           
    return INVALID_SECOND_FORMAT;
  if (!doc["min"].is<uint8_t>() || doc["min"] > 59)           
    return INVALID_MINUTE_FORMAT;
  if (!doc["hr"].is<uint8_t>() || doc["hr"] > 23)            
    return INVALID_HOUR_FORMAT;
  if (!doc["diaSem"].is<uint8_t>() || doc["diaSem"] < 1 || doc["diaSem"] > 7)         
    return INVALID_WEEKDAY_FORMAT;
  if (!doc["dia"].is<uint8_t>() || doc["dia"] < 1 || doc["dia"] > 31)           
    return INVALID_DAY_FORMAT;
  if (!doc["mes"].is<uint8_t>() || doc["mes"] < 1 || doc["mes"] > 12)           
    return INVALID_MONTH_FORMAT;
  if (!doc["anio"].is<uint8_t>() || doc["anio"] > 99)         
    return INVALID_YEAR_FORMAT;

  _systemStatus[systemEnable] = doc["enable"] | false;
  _systemStatus[photoperiod] = doc["fp"] | 0;
  _systemStatus[blueDutyCycle] = doc["ledA"] | 0;
  _systemStatus[redDutyCycle] = doc["ledR"] | 0;
  _systemStatus[whiteDutyCycle] = doc["ledB"] | 0;
  _systemStatus[irrigationFrequency] = doc["irrH"] | 0;
  _systemStatus[irrigationDuration] = doc["irrM"] | 0;
  _systemStatus[ventilationFrequency] = doc["ventH"] | 0;
  _systemStatus[ventilationDuration] = doc["ventM"] | 0;
  
  _currentTime[second] = doc["seg"];
  _currentTime[minute] = doc["min"];
  _currentTime[hour] = doc["hr"];
  _currentTime[dayOfWeek] = doc["diaSem"];
  _currentTime[day] = doc["dia"];
  _currentTime[month] = doc["mes"];
  _currentTime[year] = doc["anio"];

  startClock();
  setCurrentTime();

  if (!p.begin("system", false)) 
    return STORAGE_ERROR;
  p.putBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
  p.end();

  if (!p.begin("plantData", false))
    return STORAGE_ERROR;
  p.putString("plantName", _plantName);
  p.end();

  turnOnDevices();
  Serial.println("[System] Parámetros actualizados");

  return STATUS_OK; 
}

void Plant::turnOnDevices(){
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
  manageDevice(waterPumpPin, _systemStatus[irrigationFrequency], _systemStatus[irrigationDuration]);
  manageDevice(fanPin, _systemStatus[ventilationFrequency], _systemStatus[ventilationDuration]);

}

/**
 * @brief Controla un dispositivo de acuerdo a la frecuencia configurada.
 * 
 * @param devicePin       Pin del dispositivo (bomba o ventilador).
 * @param scheduleHour    Frecuencia de activación (onceAday, eachThreeHours, etc.).
 * @param scheduleMinute  Duración en minutos del encendido.
 */
void Plant::manageDevice(int devicePin, int scheduleHour, int scheduleMinute) {
  bool activeDevice = false;

  switch (scheduleHour) {
    case FREQ_ONCE_A_DAY:
      activeDevice = (_currentTime[hour] == 0 && _currentTime[minute] < scheduleMinute);
      break;

    case FREQ_TWICE_A_DAY:
      activeDevice = ((_currentTime[hour] == 0 || _currentTime[hour] == 12) && _currentTime[minute] < scheduleMinute);
      break;

    case FREQ_4X_PER_DAY:
      activeDevice = ((_currentTime[hour] % 6 == 0) &&   _currentTime[minute] < scheduleMinute);  // 24 / 4 = cada 6 horas
      break;

    case FREQ_6X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 4 == 0) && _currentTime[minute] < scheduleMinute);  // 24 / 6 = cada 4 horas
      break;

    case FREQ_8X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 3 == 0) &&  _currentTime[minute] < scheduleMinute);  // 24 / 8 = cada 3 horas
      break;

    case FREQ_12X_PER_DAY:
      activeDevice = (
        (_currentTime[hour] % 2 == 0) &&   _currentTime[minute] < scheduleMinute);  // 24 / 12 = cada 2 horas
      break;

    case FREQ_EVERY_HOUR:
      activeDevice = (_currentTime[minute] < scheduleMinute);
      break;

    default:
      activeDevice = false;
  }

  digitalWrite(devicePin, activeDevice ? HIGH : LOW);
}



String Plant::buildShowParametersForm() {
  String HTML;
  String dispositivoActivo = _systemStatus[systemEnable] ? "Activo" : "Inactivo";
  HTML += showParametersForm;
  HTML += "{\n";
  HTML += "dispositivoActivo: \"" + dispositivoActivo + "\",\n";
  HTML += "fotoperiodo: " + String(_systemStatus[photoperiod]) + ",\n";
  HTML += "semana: " + String(_systemStatus[cropWeek]) + ",\n";
  HTML += "dias: " + String(_systemStatus[cropDay]) + ",\n";
  HTML += "luzAzul: " + String(_systemStatus[blueDutyCycle]) + ",\n";
  HTML += "luzRoja: " + String(_systemStatus[redDutyCycle]) + ",\n";
  HTML += "luzBlanca: " + String(_systemStatus[whiteDutyCycle]) + ",\n";
  HTML += "horasIrrigacion: " + String(_systemStatus[irrigationFrequency]) + ",\n";
  HTML += "minutosIrrigacion: " + String(_systemStatus[irrigationDuration]) + ",\n";
  HTML += "horasVentilador: " + String(_systemStatus[ventilationFrequency]) + ",\n";
  HTML += "minutosVentilador: " + String(_systemStatus[ventilationDuration]) + "\n";
  HTML += "};\n";
  HTML += showParametersForm2;
  return HTML;
}

String Plant::buildUpdateParametersForm() {
  String HTML;
  Serial.printf("\nFlag %s", (_systemStatus[systemEnable] ? "true.\n" : "false.\n"));
  String dispositivoActivo = (_systemStatus[systemEnable] ? "true" : "false");
  HTML += updateParametersForm;
  HTML += "{\n";
  //HTML += "plantName: " + String(_plantName) + ",\n";
  HTML += "dispositivoActivo: " + dispositivoActivo + ",\n";
  HTML += "fotoperiodo: " + String(_systemStatus[photoperiod]) + ",\n";
  HTML += "luzAzul: " + String(_systemStatus[blueDutyCycle]) + ",\n";
  HTML += "luzRoja: " + String(_systemStatus[redDutyCycle]) + ",\n";
  HTML += "luzBlanca: " + String(_systemStatus[whiteDutyCycle]) + ",\n";
  HTML += "horasIrrigacion: " + String(_systemStatus[irrigationFrequency]) + ",\n";
  HTML += "minutosIrrigacion: " + String(_systemStatus[irrigationDuration]) + ",\n";
  HTML += "horasVentilador: " + String(_systemStatus[ventilationFrequency]) + ",\n";
  HTML += "minutosVentilador: " + String(_systemStatus[ventilationDuration]) + "\n";
  HTML += "};\n";
  HTML += updateParametersForm2;
  return HTML;
}

void Plant::startClock(){
  Wire.begin();

  Wire.beginTransmission(DS3231Adress);
  Wire.write(0x00);

  for (uint8_t i = 0; i < rtcReadBytes; i++)
      Wire.write(bin2bcd(0));

  Wire.endTransmission();
}

bool Plant::setCurrentTime(){
  //Wire.begin();
  Wire.beginTransmission(DS3231Adress);
  Wire.write(0x00);

  for (uint8_t i = second; i <= year; i++)
      Wire.write(bin2bcd(_currentTime[i]));

  return (Wire.endTransmission() == 0);
}

bool Plant::getCurrentTime(){

  Wire.beginTransmission(DS3231Adress);
  Wire.write(0x00);

  if (Wire.endTransmission() != 0)
      return false;

  uint8_t bytesReceived = Wire.requestFrom(DS3231Adress, rtcReadBytes);
  if (bytesReceived != rtcReadBytes)
      return false;

  for (uint8_t i = 1; i <= rtcReadBytes; i ++) 
    _currentTime[i] = bcd2bin(Wire.read());  
  return true; 

}

void Plant::hardReset() {
    Serial.println("------- FACTORY RESET -------");

    Serial.println("Clearing: system");
    p.begin("system", false);
    p.clear();
    p.end();

    /*Serial.println("Clearing: firmware");
    p.begin("firmware", false);
    p.clear();
    p.end();*/

    Serial.println("Clearing: config");
    p.begin("config", false);
    p.clear();
    p.end();

}

void Plant::printSystemData() { 
  getCurrentTime();
  Serial.printf("\n===================== ESTADO DEL SISTEMA =====================\n");

  Serial.printf("[SYS] MAC:%s | Usuario:%s | Sistema:%s\n",
                _MAC,
                _systemStatus[hasRegisteredUser] ? "REGISTRADO" : "NO REGISTRADO",
                _systemStatus[systemEnable] ? "ACTIVO" : "INACTIVO");

  Serial.printf("[AUTH] Username:%s | Pass:%s\n", _username, maskPassword(_userpass).c_str());

  /*Serial.printf("[WIFI] SSID:%s | Pass:%s | Conexion:%s\n",
                ssid,
                maskPassword(wifiPass).c_str(),
                _systemStatus[hasWifiCredentials] ? "SI" : "NO");*/

  Serial.printf("[LUZ] Fotoperiodo:%dh | Azul:%d%% | Roja:%d%% | Blanca:%d%%\n",
                _systemStatus[photoperiod],
                _systemStatus[blueDutyCycle],
                _systemStatus[redDutyCycle],
                _systemStatus[whiteDutyCycle]);

  Serial.printf("[RIEGO] Frecuencia:%02d veces al dia por %02d minutos\n",
                _systemStatus[irrigationFrequency],
                _systemStatus[irrigationDuration]);

  Serial.printf("[VENT] Frecuencia:%02d veces al dia por %02d minutos\n",
                _systemStatus[ventilationFrequency],
                _systemStatus[ventilationDuration]);

  Serial.printf("[CULTIVO] %02d/%02d/%02d %02d:%02d:%02d Semana:%d | Dia:%d\n",
                _currentTime[day], _currentTime[month], _currentTime[year], 
                _currentTime[hour], _currentTime[minute], _currentTime[second],
                _systemStatus[cropWeek],
                _systemStatus[cropDay]);
  Serial.printf("[CROP] Planta:%s\n", _plantName);

  Serial.printf("================================\n\n");
}

HttpResponse buildHttpResponse(requestStatus status) {
  switch (status) {
    case STATUS_OK:
        return {200, "application/json", "{\"status\":true,\"message\":\"Información recibida correctamente. El dispositivo se reiniciará.\"}"};
    case HARD_RESET:
        return {200, "application/json", "{\"status\":true,\"message\":\"Factory reset ejecutado.\"}"};
    case INVALID_JSON:
        return {400, "application/json", "{\"status\":false,\"message\":\"El formato de envío es inválido.\"}"};
    case STORAGE_ERROR:
        return {400, "application/json", "{\"status\":false,\"message\":\"Los datos recibidos no se pudieron guardar.\"}"};
    
    case MISSING_FIELDS: // ESPECIFICAR DE QUE CAMPO
        return {400, "application/json", "{\"status\":false,\"message\":\"Campos requeridos faltantes.\"}"};

    case MISSING_CREDENTIALS:
        return {400, "application/json", "{\"status\":false,\"message\":\"Los campos de usuario y contraseña son obligatorios.\"}"};
    case INVALID_USERNAME_LENGTH:
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de usuario inválida (5-16 caracteres).\"}"};
    case INVALID_USERPASS_LENGTH:
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de contraseña inválida (8-32 caracteres).\"}"};
    case INVALID_USERNAME_CHARS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"El nombre de usuario solo permite los caracteres (_-.@!#$%&*?+=).\"}"};
    case INVALID_USERPASS_CHARS: 
        return {400, "application/json", "{\"status\":false,\"message\":\"La contraseña de usuario solo permite los caracteres (_-.@!#$%&*?+=).\"}"};
    case USERNAME_REPEATED_CHARS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"El nombre de usuario tiene un caracter repetido más de 3 veces.\"}"};
    case USERPASS_REPEATED_CHARS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"La contraseña de usuario tiene un caracter repetido más de 3 veces.\"}"};
    case MISMATCH_CREDENTIALS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"Las credenciales enviadas no coinciden.\"}"};

    case MISSING_PLANTNAME_FIELD:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo planta es obligatorio.\"}"};
    case INVALID_PLANTNAME_LENGTH:
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de planta inválida (5-22 caracteres).\"}"};  
    case INVALID_PLANTNAME_CHARS: // ESPECIFICAR DE QUE CAMPO
        return {400, "application/json", "{\"status\":false,\"message\":\"EL nombre de la planta solo permite los caracteres (_-.@!#$%&*?+=).\"}"};
    case PLANTNAME_REPEATED_CHARS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"EL nombre de la planta tiene un caracter repetido más de 3 veces.\"}"};
    
    
    case INVALID_PHOTOPERIOD_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valor de fotoperiodo inválido (solamente enteros).\"}"};
    case INVALID_IRRIGATION_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valores de irrigación inválidos (solamente enteros).\"}"};
    case INVALID_VENTILATION_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valores de ventilación inválidos (solamente enteros).\"}"};

    case INVALID_SECOND_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo segundo debe ser un entero sin signo (0-59).\"}"};
    case INVALID_MINUTE_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo minuto debe ser un entero sin signo (0-59).\"}"};
    case INVALID_HOUR_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo hora debe ser un entero sin signo (0-23).\"}"};
    case INVALID_WEEKDAY_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo dia de la semana debe ser un entero sin signo (1-7).\"}"};
    case INVALID_DAY_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo dia debe ser un entero sin signo (1-31).\"}"};
    case INVALID_MONTH_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo mes debe ser un entero sin signo (1-12).\"}"};
    case INVALID_YEAR_FORMAT:
        return {400, "application/json", "{\"status\":false,\"message\":\"El campo año debe ser un entero sin signo (0-99).\"}"};

    default:
        return {500, "application/json", "{\"status\":false,\"message\":\"Error interno del sistema.\"}"};
  }
}


uint8_t bcd2bin(uint8_t bcd){
  return (bcd / 16 * 10) + (bcd % 16);
}
uint8_t bin2bcd(uint8_t bin){
  return (bin / 10 * 16) + (bin % 10);
}

/*bool isValidString(const String& s) {
  for (char c : s) {
    if (!isValidChar(c)) 
      return false; 
  }
  return true;
}*/

/*bool isValidChar(char c) {
  if (isalnum(c)) return true;
  const char* allowed = "._-@!#$%&*?";
  for (int i = 0; allowed[i] != 0; i++) {
    if (c == allowed[i]) return true;
  }
  return false;
}*/

/*bool isValidString(const char* s) {
  if (!s || s[0] == '\0') return false;

  for (int i = 0; s[i] != '\0'; i++) {
    if (!isValidChar(s[i])) {
      return false;
    }
  }
  return true;
}*/

/*bool isValidChar(char c) { // Solo caracteres permitidos para username y planta
  if (isalnum((unsigned char)c)) return true;

  switch (c) {
    case '.': case '_': case '-':
    case '@': case '!': case '#':
    case '$': case '%': case '&':
    case '*': case '?':
      return true;
    default:
      return false;
  }
}*/


bool isValidReadableString(const String& s, bool allowSpaces) {
    for (char c : s) {
      if (isalnum(c)) continue;
      if (c == '_' || c == '-' || c == '.' || c == '@' || 
          c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
          c == '*' || c == '?' || c == '+' || c == '=')
        continue;
      if (allowSpaces && c == ' ') continue;
      // Si quieres permitir más símbolos para passwords:
      return false;
    }
    return true;
}

bool hasTooManyRepeatedChars(const String& s) {
    int count = 1;

    for (int i = 1; i < s.length(); i++) {
        if (s[i] == s[i - 1]) {
            count++;
            if (count > 3) return true;
        } else {
            count = 1;
        }
    }
    return false;
}

String maskPassword(const char* pass) {
    if (!pass || pass[0] == '\0') return "";

    int len = strlen(pass);
    if (len <= 2) return "**";

    String masked;
    masked.reserve(len); // evita reallocs

    masked += pass[0];
    masked += "****";
    masked += pass[len - 1];

    return masked;
}

/*
Estas funciones se usan para validar el SSID y el SSIDpass

String ssid = doc["key"] | "";
ssid.trim();
if (ssid.length() == 0)
    return MISSING_FIELDS;

String ssidPass = doc["key"] | "";
ssidPass.trim();
if (ssidPass.length() == 0)
    return MISSING_FIELDS;

if (ssid.length() == 0 || ssid.length() > 31)
    return INVALID_SSID;

if (!isValidReadableString(ssid, true))
    return INVALID_CHARS;

if (ssidPass.length() < 8 || ssidPass.length() > 63)
    return INVALID_WIFI_PASS;

if (!isValidReadableString(ssidPass))
    return INVALID_CHARS;

*/

