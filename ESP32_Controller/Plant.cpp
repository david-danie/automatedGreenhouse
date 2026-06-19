#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "esp_mac.h" 
#include "Constants.h"
#include "Plant.h"
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

  printSystemData();

  //startClock();

}

/*bool Plant::getRegisteredUser(){
  return _systemStatus[hasRegisteredUser];
}*/

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
      !doc.containsKey("fpOff")   ||
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
  if (utf8Len(plantNameBuff) < minPlantNameChars || utf8Len(plantNameBuff) > maxPlantNameChars)
    return INVALID_PLANTNAME_LENGTH;
  if (!isValidReadableString(plantNameBuff, true))
    return INVALID_PLANTNAME_CHARS;
  if (hasConsecutiveSpaces(plantNameBuff))   // equivale a /\s{2,}/ del form
    return PLANTNAME_REPEATED_SPACES;
  if (isAllDigits(plantNameBuff))            // equivale a /^\d+$/ del form
    return PLANTNAME_ONLY_DIGITS;
  if (hasTooManyRepeatedChars(plantNameBuff))
    return PLANTNAME_REPEATED_CHARS;
  strlcpy(_plantName, plantNameBuff.c_str(), sizeof(_plantName));

  // Fotoperiodo: hora de prendido y de apagado (0-23). El ciclo puede cruzar
  // medianoche (prendido > apagado), pero no pueden ser iguales (0h o 24h de luz).
  if (!doc["fpOn"].is<uint8_t>() || doc["fpOn"] > 23 ||
      !doc["fpOff"].is<uint8_t>() || doc["fpOff"] > 23)
    return INVALID_PHOTOPERIOD_TYPE;
  if ((uint8_t)doc["fpOn"] == (uint8_t)doc["fpOff"])
    return INVALID_PHOTOPERIOD_TYPE;
  // irrH/ventH = veces/día (cantidad real): frecuencia permitida.
  // irrM/ventM = minutos de duración del encendido (0-59, igual que el form).
  if (!doc["irrH"].is<uint8_t>() || !doc["irrM"].is<uint8_t>() ||
      !isValidFrequency(doc["irrH"]) || doc["irrM"] > 59)
    return INVALID_IRRIGATION_TYPE;
  if (!doc["ventH"].is<uint8_t>() || !doc["ventM"].is<uint8_t>() ||
      !isValidFrequency(doc["ventH"]) || doc["ventM"] > 59)
    return INVALID_VENTILATION_TYPE;

  // LEDs = duty cycle 0-100% (igual que el form).
  if (!doc["ledA"].is<uint8_t>() || doc["ledA"] > 100 ||
      !doc["ledR"].is<uint8_t>() || doc["ledR"] > 100 ||
      !doc["ledB"].is<uint8_t>() || doc["ledB"] > 100)
    return INVALID_LED_VALUE;

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
  _systemStatus[photoperiodOn] = doc["fpOn"] | 0;
  _systemStatus[photoperiodOff] = doc["fpOff"] | 0;
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

  //startClock();
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
  // Ventana de fotoperiodo [prendido, apagado). Si prendido < apagado la ventana
  // es continua; si prendido > apagado, cruza medianoche.
  uint8_t h = _currentTime[hour];
  uint8_t on = _systemStatus[photoperiodOn];
  uint8_t off = _systemStatus[photoperiodOff];
  bool luzEncendida = (on < off) ? (h >= on && h < off) : (h >= on || h < off);

  if (luzEncendida) {
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
 * @brief Controla un dispositivo según su frecuencia diaria.
 *
 * La frecuencia llega como la cantidad real de veces/día (0,1,2,4,8,12,24). Se
 * traduce a un intervalo de horas (24 / veces) y el dispositivo se enciende
 * durante los primeros `durationMinutes` de cada hora múltiplo del intervalo.
 * Ej.: 8 veces/día → cada 3 h → enciende a las 0,3,6,9,... por N minutos.
 *
 * @param devicePin         Pin del dispositivo (bomba o ventilador).
 * @param timesPerDay       Veces al día (debe dividir 24; ya validado en la entrada).
 * @param durationMinutes   Duración en minutos del encendido.
 */
void Plant::manageDevice(int devicePin, int timesPerDay, int durationMinutes) {
  bool activeDevice = false;

  if (timesPerDay > 0) {
    uint8_t intervalHours = 24 / timesPerDay;  // divisor exacto de 24
    activeDevice = (_currentTime[hour] % intervalHours == 0) &&
                   (_currentTime[minute] < durationMinutes);
  }

  digitalWrite(devicePin, activeDevice ? HIGH : LOW);
}

// Serializa el estado del dispositivo a JSON para GET /getparams. El cliente
// (dashboardForm) decide qué pintar según "hasRegisteredUser": si es false solo
// se manda ese flag; si es true se incluyen todos los parámetros con las mismas
// claves que espera el formulario (planta, fpOn, fpOff, ledA, irrH, etc.).
String Plant::buildParamsJson() {
  StaticJsonDocument<512> doc;

  doc["hasRegisteredUser"] = (bool)_systemStatus[hasRegisteredUser];

  if (_systemStatus[hasRegisteredUser]) {
    doc["planta"] = _plantName;
    doc["enable"] = (bool)_systemStatus[systemEnable];
    doc["fpOn"]   = _systemStatus[photoperiodOn];
    doc["fpOff"]  = _systemStatus[photoperiodOff];
    doc["ledA"]   = _systemStatus[blueDutyCycle];
    doc["ledR"]   = _systemStatus[redDutyCycle];
    doc["ledB"]   = _systemStatus[whiteDutyCycle];
    doc["irrH"]   = _systemStatus[irrigationFrequency];
    doc["irrM"]   = _systemStatus[irrigationDuration];
    doc["ventH"]  = _systemStatus[ventilationFrequency];
    doc["ventM"]  = _systemStatus[ventilationDuration];
    doc["semana"] = _systemStatus[cropWeek];
    doc["dia"]    = _systemStatus[cropDay];
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// Valida credenciales contra las guardadas, sin escribir nada. Sirve para
// desbloquear el modo edición del dashboard (POST /authusercredentials) antes
// de permitir el guardado real en /newparams.
requestStatus Plant::authUserCredentials(const String& body) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, body))
    return INVALID_JSON;

  if (!doc.containsKey("user") || !doc.containsKey("pass"))
    return MISSING_CREDENTIALS;

  String username = doc["user"] | "";
  username.trim();
  String userpass = doc["pass"] | "";
  userpass.trim();

  if (utf8Len(username) < minUsernameChars || utf8Len(username) > maxUsernameChars)
    return INVALID_USERNAME_LENGTH;
  if (utf8Len(userpass) < minUserpassChars || utf8Len(userpass) > maxUserpassChars)
    return INVALID_USERPASS_LENGTH;

  if (username != _username || userpass != _userpass)
    return MISMATCH_CREDENTIALS;

  return STATUS_OK;
}

/*void Plant::startClock(){
  Wire.begin();

  Wire.beginTransmission(DS3231Adress);
  Wire.write(0x00);

  for (uint8_t i = 0; i < rtcReadBytes; i++)
      Wire.write(bin2bcd(0));

  Wire.endTransmission();
}*/

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

  Serial.printf("[LUZ] Prende:%02dh | Apaga:%02dh | Azul:%d%% | Roja:%d%% | Blanca:%d%%\n",
                _systemStatus[photoperiodOn],
                _systemStatus[photoperiodOff],
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
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de usuario inválida (4-32 caracteres).\"}"};
    case INVALID_USERPASS_LENGTH:
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de contraseña inválida (8-64 caracteres).\"}"};
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
        return {400, "application/json", "{\"status\":false,\"message\":\"Longitud de planta inválida (3-20 caracteres).\"}"};
    case INVALID_PLANTNAME_CHARS: // ESPECIFICAR DE QUE CAMPO
        return {400, "application/json", "{\"status\":false,\"message\":\"EL nombre de la planta solo permite los caracteres (_-.@!#$%&*?+=).\"}"};
    case PLANTNAME_REPEATED_CHARS: //
        return {400, "application/json", "{\"status\":false,\"message\":\"EL nombre de la planta tiene un caracter repetido más de 3 veces.\"}"};
    case PLANTNAME_REPEATED_SPACES:
        return {400, "application/json", "{\"status\":false,\"message\":\"El nombre de la planta no puede tener espacios consecutivos.\"}"};
    case PLANTNAME_ONLY_DIGITS:
        return {400, "application/json", "{\"status\":false,\"message\":\"El nombre de la planta no puede ser solo números.\"}"};


    case INVALID_PHOTOPERIOD_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valor de fotoperiodo inválido (solamente enteros).\"}"};
    case INVALID_IRRIGATION_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valores de irrigación inválidos (frecuencia permitida y minutos 0-59).\"}"};
    case INVALID_VENTILATION_TYPE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Valores de ventilación inválidos (frecuencia permitida y minutos 0-59).\"}"};
    case INVALID_LED_VALUE:
        return {400, "application/json", "{\"status\":false,\"message\":\"Los valores de los LEDs deben estar entre 0 y 100%.\"}"};

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

// Cuenta caracteres (code points) en una cadena UTF-8, ignorando los bytes de
// continuación (10xxxxxx). Así "Jalapeño" cuenta 8 y no 9, y los límites de
// longitud coinciden con los del formulario (String.length de JS).
int utf8Len(const String& s) {
  int count = 0;
  for (int i = 0; i < s.length(); i++) {
    if (((unsigned char)s[i] & 0xC0) != 0x80) count++;
  }
  return count;
}

// Longitud en bytes del carácter UTF-8 que empieza con el byte c.
int utf8CharLen(unsigned char c) {
  if (c < 0x80) return 1;            // 0xxxxxxx (ASCII)
  if ((c & 0xE0) == 0xC0) return 2;  // 110xxxxx
  if ((c & 0xF0) == 0xE0) return 3;  // 1110xxxx
  if ((c & 0xF8) == 0xF0) return 4;  // 11110xxx
  return 1;                          // byte inválido: avanza 1 para no atascarse
}

// ¿Hay dos o más espacios consecutivos? Equivale a /\s{2,}/ del formulario
// (los demás caracteres de espacio ya los rechaza isValidReadableString).
bool hasConsecutiveSpaces(const String& s) {
  for (int i = 1; i < s.length(); i++) {
    if (s[i] == ' ' && s[i - 1] == ' ') return true;
  }
  return false;
}

// ¿La cadena está formada únicamente por dígitos? Equivale a /^\d+$/ del
// formulario (cadena vacía cuenta como "no solo dígitos").
bool isAllDigits(const String& s) {
  if (s.length() == 0) return false;
  for (int i = 0; i < s.length(); i++) {
    if (!isdigit((unsigned char)s[i])) return false;
  }
  return true;
}

// Vocales acentuadas y ñ/Ñ del español en UTF-8: secuencias de 2 bytes cuyo
// primer byte (lead) es 0xC3. Devuelve true si (lead, cont) forman una de
// esas letras: á é í ó ú ü Á É Í Ó Ú Ü ñ Ñ.
bool isSpanishAccentUtf8(unsigned char lead, unsigned char cont) {
  if (lead != 0xC3) return false;
  switch (cont) {
    case 0xA1: case 0xA9: case 0xAD: case 0xB3: case 0xBA: case 0xBC: // á é í ó ú ü
    case 0x81: case 0x89: case 0x8D: case 0x93: case 0x9A: case 0x9C: // Á É Í Ó Ú Ü
    case 0xB1: case 0x91:                                             // ñ Ñ
      return true;
  }
  return false;
}

// Mismo set de caracteres que el formulario: letras (incluidas vocales
// acentuadas y ñ/Ñ del español), dígitos y (_-.@!#$%&*?+=). El espacio solo se
// admite cuando allowSpaces es true (p. ej. el nombre de la planta).
bool isValidReadableString(const String& s, bool allowSpaces) {
  int n = s.length();
  for (int i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];

    if (isalnum(c)) continue; // alfanumérico ASCII

    if (c == '_' || c == '-' || c == '.' || c == '@' ||
        c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
        c == '*' || c == '?' || c == '+' || c == '=')
      continue;

    if (allowSpaces && c == ' ') continue;

    // Vocal acentuada o ñ/Ñ (UTF-8, 2 bytes): se aceptan ambos bytes.
    if (i + 1 < n && isSpanishAccentUtf8(c, (unsigned char)s[i + 1])) {
      i++; // consume el segundo byte de la secuencia
      continue;
    }

    return false;
  }
  return true;
}

// Rechaza 4 o más caracteres idénticos consecutivos, comparando por carácter
// UTF-8 completo (no byte a byte), para que repetidos acentuados como "ññññ" o
// "áááá" se detecten igual que en el formulario (count > 3).
bool hasTooManyRepeatedChars(const String& s) {
  int n = s.length();
  int count = 1;
  int prevStart = 0;
  int prevLen = 0; // 0 = aún no hay carácter previo

  for (int i = 0; i < n; ) {
    int len = utf8CharLen((unsigned char)s[i]);
    if (i + len > n) len = n - i; // secuencia truncada al final

    bool same = false;
    if (prevLen == len) {
      same = true;
      for (int k = 0; k < len; k++) {
        if (s[prevStart + k] != s[i + k]) { same = false; break; }
      }
    }

    if (same) {
      count++;
      if (count > 3) return true;
    } else {
      count = 1;
    }

    prevStart = i;
    prevLen = len;
    i += len;
  }
  return false;
}

bool isValidFrequency(uint8_t f) {
  for (uint8_t v : validFrequencies)
    if (f == v) return true;
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