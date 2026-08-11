#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_mac.h> 
#include <esp_random.h>   // esp_random(): RNG por hardware para el token de sesión
#include "Constants.h"
#include "Plant.h"
#include "sensible.h"
#include "utils.h"     // helpers libres (BCD, validación de strings/UTF-8, fecha, etc.)

Plant::Plant(){

  ledcAttachChannel(blueLedPin, pwmFrequency, pwmResolution, blueChannel);
  ledcAttachChannel(redLedPin, pwmFrequency, pwmResolution, redChannel);

  pinMode(whiteLedPin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  //pinMode(buzzerPin, OUTPUT);

  digitalWrite(whiteLedPin, HIGH);
  digitalWrite(waterPumpPin, HIGH);
  digitalWrite(fanPin, HIGH);
  //digitalWrite(buzzerPin, LOW);
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
  _cropStartDay = p.getULong("cropStart", 0);  // 0 = cultivo sin anclar aún
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

  // Credenciales Wi-Fi del usuario (modo STA). Viven en su propio namespace
  // "wifi"; el flag hasWifiCredentials (en _systemStatus/"system") decide si se
  // levanta el STA al arrancar. Vacío = nunca configurado = AP puro.
  p.begin("wifi", true);
  p.getString("ssid", _SSID, sizeof(_SSID));
  p.getString("pass", _SSIDpass, sizeof(_SSIDpass));
  p.end();

  getCurrentTime();   // primer refresco del RTC (aquí, en setup/loopTask, sin concurrencia)
  printSystemData();

}

bool Plant::getRegisteredUser(){
  return _systemStatus[hasRegisteredUser];
}

// Getters para que setup() (.ino) decida AP puro vs AP+STA y arranque el STA.
bool Plant::getWifiCredentials(){
  return _systemStatus[hasWifiCredentials];
}

const char* Plant::getSsid(){
  return _SSID;
}

const char* Plant::getWifiPass(){
  return _SSIDpass;
}

requestStatus Plant::validateUserCredentials(const String& body){

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

    // El registro NO acepta el comando RESET: no hay usuario/datos previos que
    // borrar. El reset vive en el login (authUserCredentials).

    // ---- 3. Validación de longitud (por carácter UTF-8, igual que el form) ----
    if (utf8Len(username) < minUsernameChars || utf8Len(username) > maxUsernameChars)
      return INVALID_USERNAME_LENGTH;
    if (utf8Len(userpass) < minUserpassChars || utf8Len(userpass) > maxUserpassChars)
      return INVALID_USERPASS_LENGTH;

    // ---- 4. Validación de caracteres permitidos ----
    if (!isValidReadableString(username, false))
      return INVALID_USERNAME_CHARS;
    if (!isValidReadableString(userpass, false)) 
      return INVALID_USERPASS_CHARS; 

    // ---- 5. Rechazo de caracteres repetidos (4+ idénticos consecutivos) ----
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
    Serial.println("[System] Credenciales de usuario guardadas");

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
requestStatus Plant::validateCropParameters(const String& body){

  // 20 claves + copias de las cadenas (planta/user/pass) al deserializar desde
  // un String: 512 se queda corto con credenciales largas y desborda (NoMemory).
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) 
    return INVALID_JSON;
  
  if (!doc.containsKey("planta")  ||
      !doc.containsKey("enable")  ||
      !doc.containsKey("fpOn")    ||
      !doc.containsKey("fpOff")   ||
      !doc.containsKey("ledA")    ||
      !doc.containsKey("ledR")    ||
      !doc.containsKey("ledB")    ||
      !doc.containsKey("irrH")    ||
      !doc.containsKey("irrM")    ||
      !doc.containsKey("ventH")   ||
      !doc.containsKey("ventM"))
    return MISSING_FIELDS;

  // ---- Autorización por TOKEN de sesión ----
  // Ya no se reenvían las credenciales en claro: el front manda el token emitido
  // en el login/registro y debe seguir vigente. Si no, la edición se rechaza.
  if (!doc.containsKey("token"))
    return INVALID_SESSION;

  String token = doc["token"] | "";
  token.trim();
  if (!isSessionValid(token))
    return INVALID_SESSION;

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
  if (!doc["fpOn"].is<uint8_t>() || doc["fpOn"] > 23 || !doc["fpOff"].is<uint8_t>() || doc["fpOff"] > 23)
    return INVALID_PHOTOPERIOD_TYPE;
  if ((uint8_t)doc["fpOn"] == (uint8_t)doc["fpOff"])
    return INVALID_PHOTOPERIOD_TYPE;
  // irrH/ventH = intervalo en HORAS entre activaciones (de validFrequencies):
  //   3=cada 3h, 24=diario, 48=cada 2 días, 168=semanal, 0=apagado.
  // irrM/ventM = minutos de duración del encendido (0-59, igual que el form).
  if (!doc["irrH"].is<uint8_t>() || !doc["irrM"].is<uint8_t>() || !isValidFrequency(doc["irrH"]) || doc["irrM"] > 59)
    return INVALID_IRRIGATION_TYPE;
  if (!doc["ventH"].is<uint8_t>() || !doc["ventM"].is<uint8_t>() || !isValidFrequency(doc["ventH"]) || doc["ventM"] > 59)
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

  setCurrentTime();

  if (!p.begin("system", false))
    return STORAGE_ERROR;
  p.putBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
  // Ancla la fecha de inicio del cultivo en el PRIMER /newparams con fecha
  // válida (el navegador acaba de fijar la hora en el RTC). Solo se escribe una
  // vez: el guard _cropStartDay == 0 evita reescribir en ediciones posteriores,
  // así editar parámetros no reinicia la edad del cultivo (y no desgasta flash).
  if (_cropStartDay == 0) {
    _cropStartDay = daysSinceEpoch(2000 + _currentTime[year], _currentTime[month], _currentTime[day]);
    p.putULong("cropStart", _cropStartDay);
  }
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
    digitalWrite(whiteLedPin, _systemStatus[whiteDutyCycle] > 0 ? LOW : HIGH);
    ledcWrite(blueChannel, map(_systemStatus[blueDutyCycle], 0, 100, 0, maxDutyCycle));
    ledcWrite(redChannel, map(_systemStatus[redDutyCycle], 0, 100, 0, maxDutyCycle));
  } else {
    // Apaga todas las luces
    digitalWrite(whiteLedPin, HIGH);
    ledcWrite(blueChannel, zero);
    ledcWrite(redChannel, zero);
  }

  // ** Control de Riego y Ventilación **
  // Contador continuo de horas derivado del RTC, calculado UNA vez y compartido
  // por bomba y ventilador.
  uint32_t epochHours = daysSinceEpoch(2000 + _currentTime[year],_currentTime[month], _currentTime[day]) * 24UL + _currentTime[hour];
  manageDevice(waterPumpPin, _systemStatus[irrigationFrequency], _systemStatus[irrigationDuration], epochHours);
  manageDevice(fanPin, _systemStatus[ventilationFrequency], _systemStatus[ventilationDuration], epochHours);

}

/**
 * @brief Controla un dispositivo según su intervalo entre activaciones.
 *
 * `intervalHours` es directamente el número de HORAS entre encendidos (ya no
 * "veces/día"). Para decidir si toca encender se usa un contador CONTINUO de
 * horas, `epochHours`, derivado de la fecha/hora del RTC: a diferencia de la
 * hora del día (0-23), este contador NO se reinicia a medianoche, así que el
 * mismo módulo sirve para intervalos sub-diarios y multi-día:
 *   - 3 h  → enciende a las 0,3,6,9,... todos los días (8 veces/día)
 *   - 24 h → una vez al día (hora 0)
 *   - 48 h → cada 2 días (hora 0)   168 h → cada semana (hora 0)
 * El dispositivo queda encendido durante los primeros `durationMinutes` de la
 * hora en que toca. Es STATELESS: todo se recalcula del RTC, sin guardar nada,
 * así que sobrevive cortes de luz sin derivar.
 *
 * @param devicePin         Pin del dispositivo (bomba o ventilador).
 * @param intervalHours     Horas entre activaciones (de validFrequencies; 0 = apagado).
 * @param durationMinutes   Duración en minutos del encendido.
 * @param epochHours        Contador continuo de horas del RTC (lo calcula
 *                          turnOnDevices una sola vez y lo comparte entre
 *                          bomba y ventilador).
 */
void Plant::manageDevice(int devicePin, int intervalHours, int durationMinutes, uint32_t epochHours) {
  bool activeDevice = false;

  if (intervalHours > 0) {
    activeDevice = (epochHours % (uint32_t)intervalHours == 0) && (_currentTime[minute] < durationMinutes);
  }

  // Lógica invertida: LOW enciende (0 lógico), HIGH apaga (1 lógico)
  digitalWrite(devicePin, activeDevice ? LOW : HIGH);
}

/**
 * @brief Edad del cultivo en días (>=1), derivada del RTC y del ancla.
 *
 * STATELESS: en vez de incrementar un contador a medianoche (que se perdería en
 * cortes de luz), calcula la diferencia entre el día de hoy y _cropStartDay. Así
 * el cultivo "sigue envejeciendo" aunque el equipo haya estado apagado. El día
 * del ancla cuenta como día 1. Usa _currentTime, que refresca printTask desde el
 * RTC cada 5 s; no hace I²C aquí para no chocar con esa tarea.
 *
 * @return día de cultivo (>=1), o 0 si aún no se ancla o el RTC retrocedió.
 */
int Plant::cropDayFromRtc() {
  if (_cropStartDay == 0) return 0;                  // cultivo sin anclar

  uint32_t today = daysSinceEpoch(2000 + _currentTime[year], _currentTime[month], _currentTime[day]);
  if (today < _cropStartDay) return 0;               // RTC retrocedió: evita negativos
  return (int)(today - _cropStartDay) + 1;           // día del ancla = día 1
}

// Serializa el estado del dispositivo a JSON para GET /getparams. El cliente
// (mainForm) decide qué pintar según "hasRegisteredUser": si es false solo
// se manda ese flag; si es true se incluyen todos los parámetros con las mismas
// claves que espera el formulario (planta, fpOn, fpOff, ledA, irrH, etc.).
String Plant::buildParamsJson(const String& token) {
  StaticJsonDocument<768> doc;

  doc["hasRegisteredUser"] = (bool)_systemStatus[hasRegisteredUser];

  // Reporta si la sesión sigue activa (el front salta el login si es true). La
  // ventana es FIJA: cargar /getparams no la extiende; expira a los SESSION_TTL_MS
  // del login, haya o no actividad.
  doc["sessionValid"] = isSessionValid(token);

  // Estado de la conexión a Internet (STA) para el chip del dashboard y el
  // polling de la vista de red. wifiSsid va vacío si el STA no está asociado.
  bool wifiUp = isWifiConnected();   // una sola lectura de WiFi.status()
  doc["wifiConnected"] = wifiUp;
  doc["wifiSsid"] = wifiUp ? WiFi.SSID() : String("");

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
    int cd = cropDayFromRtc();                  // edad derivada del RTC (0 = sin anclar)
    doc["dia"]    = cd;
    doc["semana"] = cd > 0 ? (cd - 1) / 7 + 1 : 0;
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

  // Comando especial RESET: solo tiene sentido en el login, donde ya hay
  // usuario/datos guardados que borrar. Se evalúa antes de validar credenciales
  // para que funcione aunque se haya olvidado la contraseña (no compara contra
  // las guardadas). El registro NO acepta reset: ahí no hay nada previo.
  if (userpass == "**reset**") {
    hardReset();
    return HARD_RESET;
  }

  if (utf8Len(username) < minUsernameChars || utf8Len(username) > maxUsernameChars)
    return INVALID_USERNAME_LENGTH;
  if (utf8Len(userpass) < minUserpassChars || utf8Len(userpass) > maxUserpassChars)
    return INVALID_USERPASS_LENGTH;

  if (username != _username || userpass != _userpass)
    return MISMATCH_CREDENTIALS;

  return STATUS_OK;
}

// Emite un token de sesión de 128 bits (4 x esp_random(), RNG por hardware) en
// hex (32 caracteres) y fija el vencimiento a millis() + TTL. Este es el ÚNICO
// punto donde se fija la expiración: la ventana es FIJA (no se renueva con la
// actividad), así que la sesión caduca SESSION_TTL_MS después del login. Sobrescribe
// el token anterior: solo hay UNA sesión activa, así que un login nuevo invalida la
// sesión previa. Se llama al validar login o registro OK.
String Plant::issueSessionToken() {
  char buf[33];
  for (int i = 0; i < 4; i++)
    snprintf(buf + i * 8, 9, "%08x", esp_random());
  buf[32] = '\0';
  strlcpy(_sessionToken, buf, sizeof(_sessionToken));
  _sessionExpiresAt = millis() + SESSION_TTL_MS;
  return String(_sessionToken);
}

// ¿El token coincide con el vigente y no ha expirado? La comparación de tiempo
// usa aritmética con signo para ser a prueba del wrap de millis() (~49 días);
// 30 min está muy por debajo de ese horizonte.
bool Plant::isSessionValid(const String& token) {
  if (_sessionToken[0] == '\0') return false;            // no hay sesión activa
  if (token.length() != 32) return false;
  if (token != _sessionToken) return false;
  return (int32_t)(millis() - _sessionExpiresAt) < 0;    // aún no expira
}

// Invalida la sesión actual (logout /exit y factory reset).
void Plant::clearSession() {
  _sessionToken[0] = '\0';
  _sessionExpiresAt = 0;
}

// ===========================================================================
//  Conectividad Wi-Fi del usuario (modo STA, coexiste con el AP)
// ===========================================================================

bool Plant::isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// Escanea redes y devuelve las 5 más fuertes SIN nombres repetidos. El escaneo
// es BLOQUEANTE (~2 s) y corre dentro del request /wifiscan: aceptable para una
// acción puntual del usuario. Dedup: ante dos APs con el mismo SSID (repetidores)
// se conserva el de mayor RSSI. Oculta SSIDs vacíos (redes ocultas).
String Plant::scanNetworks() {
  // Top-5 por RSSI, deduplicado por SSID. Arreglos fijos (sin heap): el ESP32-C3
  // tiene RAM limitada y 5 entradas sobran para una lista legible.
  const uint8_t MAX = 5;
  char     bestSsid[MAX][maxWifiSsidChars + 1];
  int32_t  bestRssi[MAX];
  bool     bestOpen[MAX];
  uint8_t  count = 0;

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;                 // red oculta: sin nombre
    int32_t rssi = WiFi.RSSI(i);
    bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

    // ¿Ya está ese SSID? (repetidor) → conserva la señal más fuerte.
    int found = -1;
    for (uint8_t k = 0; k < count; k++)
      if (ssid.equals(bestSsid[k])) { found = k; break; }

    if (found >= 0) {
      if (rssi > bestRssi[found]) { bestRssi[found] = rssi; bestOpen[found] = open; }
      continue;
    }

    if (count < MAX) {                                // hay lugar: agrega
      strlcpy(bestSsid[count], ssid.c_str(), sizeof(bestSsid[count]));
      bestRssi[count] = rssi;
      bestOpen[count] = open;
      count++;
    } else {                                          // lleno: reemplaza al más débil si este es más fuerte
      uint8_t weakest = 0;
      for (uint8_t k = 1; k < MAX; k++)
        if (bestRssi[k] < bestRssi[weakest]) weakest = k;
      if (rssi > bestRssi[weakest]) {
        strlcpy(bestSsid[weakest], ssid.c_str(), sizeof(bestSsid[weakest]));
        bestRssi[weakest] = rssi;
        bestOpen[weakest] = open;
      }
    }
  }
  WiFi.scanDelete();                                  // libera los resultados del escaneo

  StaticJsonDocument<768> doc;
  JsonArray nets = doc.createNestedArray("networks");
  for (uint8_t k = 0; k < count; k++) {
    JsonObject o = nets.createNestedObject();
    o["ssid"]   = bestSsid[k];
    o["rssi"]   = bestRssi[k];
    o["secure"] = !bestOpen[k];
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// Valida y ARRANCA la conexión (sin bloquear ni persistir). Igual que /newparams,
// exige un token de sesión vigente: cambiar la red es una acción sensible. En
// éxito devuelve STATUS_OK = "intento iniciado"; el front confirma por polling.
requestStatus Plant::saveWifiCredentials(const String& body) {
  // ssid(32) + pass(63) + token(32) se duplican al deserializar desde un String.
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, body))
    return INVALID_JSON;

  // ---- Autorización por token (misma puerta que la edición de parámetros) ----
  if (!doc.containsKey("token"))
    return INVALID_SESSION;
  String token = doc["token"] | "";
  token.trim();
  if (!isSessionValid(token))
    return INVALID_SESSION;

  if (!doc.containsKey("ssid"))
    return MISSING_WIFI_FIELDS;

  String ssid = doc["ssid"] | "";
  ssid.trim();
  if (ssid.length() == 0 || ssid.length() > maxWifiSsidChars)
    return INVALID_SSID;

  // Contraseña: vacía = red abierta (válido); si viene, debe cumplir WPA2 (8–63).
  String pass = doc["pass"] | "";
  if (pass.length() != 0 && (pass.length() < minWifiPassChars || pass.length() > maxWifiPassChars))
    return INVALID_WIFI_PASS;

  strlcpy(_SSID, ssid.c_str(), sizeof(_SSID));
  strlcpy(_SSIDpass, pass.c_str(), sizeof(_SSIDpass));
  _wifiPending = true;                 // aún NO persistido: se guarda al conectar

  // Reinicia el intento con las credenciales nuevas (añade/recrea el STA sobre el
  // AP). No bloquea: updateWifi() observará el resultado desde el loop.
  WiFi.disconnect();
  WiFi.begin(_SSID, _SSIDpass);
  Serial.printf("[WIFI] Nuevo intento de conexión a %s…\n", _SSID);

  return STATUS_OK;
}

// Persistencia diferida: se llama desde loop(). Solo cuando un intento pendiente
// llega a WL_CONNECTED se guardan las credenciales en NVS y se marca
// hasWifiCredentials. Así una contraseña incorrecta nunca queda guardada (el
// dispositivo no entraría en un bucle de reintentos fallidos tras reiniciar).
void Plant::updateWifi() {
  if (!_wifiPending) return;
  if (WiFi.status() != WL_CONNECTED) return;

  _wifiPending = false;

  p.begin("wifi", false);
  p.putString("ssid", _SSID);
  p.putString("pass", _SSIDpass);
  p.end();

  if (!_systemStatus[hasWifiCredentials]) {
    _systemStatus[hasWifiCredentials] = 1;
    p.begin("system", false);
    p.putBytes("systemStatus", _systemStatus, sizeof(_systemStatus));
    p.end();
  }
  Serial.printf("[WIFI] Conectado a %s (%s). Credenciales guardadas.\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

bool Plant::setCurrentTime(){
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

    clearSession();  // invalida cualquier sesión de edición activa
    _cropStartDay = 0;  // re-ancla el cultivo en el próximo /newparams (NVS se borra abajo)

    Serial.println("Clearing: system");
    p.begin("system", false);
    p.clear();
    p.end();

    Serial.println("Clearing: Plant name");
    p.begin("plantData", false);
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
  // Solo imprime la copia en RAM de _currentTime; el refresco desde el RTC (I²C)
  // lo hace loop() en un único task, para no compartir el bus Wire entre tareas.
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

  Serial.printf("[RIEGO] Intervalo:%dh por %d minutos\n",
                _systemStatus[irrigationFrequency],
                _systemStatus[irrigationDuration]);

  Serial.printf("[VENT] Intervalo:%dh por %d minutos\n",
                _systemStatus[ventilationFrequency],
                _systemStatus[ventilationDuration]);

  int cropDayAge = cropDayFromRtc();  // derivado del RTC + ancla (getCurrentTime ya corrió arriba)
  Serial.printf("[CULTIVO] %02d/%02d/%02d %02d:%02d:%02d Semana:%d | Dia:%d\n",
                _currentTime[day], _currentTime[month], _currentTime[year],
                _currentTime[hour], _currentTime[minute], _currentTime[second],
                cropDayAge > 0 ? (cropDayAge - 1) / 7 + 1 : 0,
                cropDayAge);
  Serial.printf("[CROP] Planta:%s\n", _plantName);

  Serial.printf("================================\n\n");
}

HttpResponse buildHttpResponse(requestStatus status) {
  switch (status) {
    case STATUS_OK:
        return {200, "application/json", "{\"status\":true,\"message\":\"Parámetros actualizados correctamente.\"}"};
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
    case INVALID_SESSION:
        return {401, "application/json", "{\"status\":false,\"message\":\"Tu sesión expiró. Vuelve a iniciar sesión.\"}"};

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

    case MISSING_WIFI_FIELDS:
        return {400, "application/json", "{\"status\":false,\"message\":\"Falta la red Wi-Fi a configurar.\"}"};
    case INVALID_SSID:
        return {400, "application/json", "{\"status\":false,\"message\":\"El nombre de la red (SSID) es inválido (1-32 caracteres).\"}"};
    case INVALID_WIFI_PASS:
        return {400, "application/json", "{\"status\":false,\"message\":\"La contraseña Wi-Fi debe tener entre 8 y 63 caracteres.\"}"};

    default:
        return {500, "application/json", "{\"status\":false,\"message\":\"Error interno del sistema.\"}"};
  }
}