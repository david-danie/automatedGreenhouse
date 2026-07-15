#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "Constants.h"
#include "sensible.h"
#include "Plant.h"
#include "mainForm.h" 

DNSServer dnsServer;
WebServer server(80);

Plant planta;

HttpResponse response;

// index page handler
void handleRoot();
void handleNotFound();
void handleUserCredentials();
void handleGetParameters();
void handleNewParameters();
void handleAuthUserCredentials();
void handleExit();
void handleWifiScan();
void handleWifiCredentials();

void setup() {
  
  planta.begin();

  // ===== Configuración de red (AP puro o AP+STA), toda aquí =====
  // Una sola radio en el ESP32-C3: el AP y el STA comparten canal. Fijamos el
  // modo dual EXPLÍCITO para que WiFi.begin() del STA no reconfigure el modo por
  // debajo, y creamos el SoftAP antes de arrancarlo.
  WiFi.mode(WIFI_AP_STA);

  // AP con WPA2-PSK (passphrase en sensible.h): cifra el enlace para que las
  // credenciales del usuario y el token de sesión no viajen en claro. create()
  // selecciona WPA2 automáticamente al recibir una passphrase de >= 8 caracteres.
  WiFi.AP.create(apSsid, apPassword);
  WiFi.AP.begin();
  WiFi.AP.enableDhcpCaptivePortal();

  // Desactiva el modem-sleep: con STA habilitado el ESP32 duerme la radio según
  // el ciclo del STA (WIFI_PS_MIN_MODEM por defecto), lo que deja al SoftAP sin
  // responder y el portal no carga. Sin sleep el AP responde estable en AP+STA.
  WiFi.setSleep(false);

  // Conexión a la red del usuario (STA) SOLO si ya hay credenciales guardadas:
  // el AP queda siempre arriba y, con credenciales, el dispositivo pasa a AP+STA.
  // Sin ellas se queda como AP puro (caso "básico"/sin configurar). No bloquea:
  // setAutoReconnect mantiene el enlace tras cortes y el loop observa el intento.
  if (planta.getWifiCredentials() && strlen(planta.getSsid()) > 0) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(planta.getSsid(), planta.getWifiPass());
    Serial.printf("[WIFI] Conectando a %s…\n", planta.getSsid());
  }

  // by default DNSServer is started serving any "*" domain name. It will reply
  // AccessPoint's IP to all DNS request (this is required for Captive Portal detection)
  if (dnsServer.start()) {
    Serial.println("[System] Portal captivo iniciado");
  } else {
    Serial.println("[System] No se puede iniciar portal captivo");
  }

  // serve a simple root page
  server.on("/", handleRoot);
  server.on("/usercredentials", HTTP_POST, handleUserCredentials);
  server.on("/getparams", HTTP_GET, handleGetParameters);
  server.on("/newparams", HTTP_POST, handleNewParameters);
  server.on("/authusercredentials", HTTP_POST, handleAuthUserCredentials);
  server.on("/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/wificredentials", HTTP_POST, handleWifiCredentials);
  server.on("/exit", handleExit);
  server.onNotFound(handleNotFound);
  server.begin();

  xTaskCreate(
      printTask,     // Función de la tarea
      "PrintTask",   // Nombre
      2048,          // Stack
      NULL,          // Sin parámetros
      1,             // Prioridad
      NULL           // Handle
  );

}

void loop() {
  // handleClient() corre en cada iteración (sin delay bloqueante) para que el
  // portal responda al instante. El control de dispositivos no necesita esa
  // frecuencia: se throttlea con millis() a deviceUpdateInterval. 
  server.handleClient();

  static uint32_t lastDeviceUpdate = 0;
  uint32_t now = millis();
  if (now - lastDeviceUpdate >= deviceUpdateInterval) {
    lastDeviceUpdate = now;
    // Único punto de I²C periódico: refresca _currentTime desde el RTC aquí, en
    // loopTask, para no compartir el bus Wire con printTask (que solo imprime).
    planta.getCurrentTime();
    planta.turnOnDevices();
    // Persiste las credenciales Wi-Fi si un intento pendiente acaba de conectar.
    planta.updateWifi();
  }

  delay(2);  // cede CPU al IDLE task (alimenta el watchdog) sin la latencia del delay(250) previo
}

void handleRoot() {
  // Página estática única. El cliente pide /getparams y, según hasRegisteredUser,
  // pinta la bienvenida o el dashboard.
  // send_P sirve el HTML (~72 KB) directo desde flash, por trozos: evita el String
  // temporal de ~72 KB que crearía send() con un const char* (ese pico de heap
  // podía fallar con AP+STA, dejando el formulario sin cargar).
  server.send_P(200, "text/html", mainForm);
  Serial.println("*****  " + server.uri() + "  *****");
}

void handleNotFound() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "redirect to captive portal");
}

void handleUserCredentials() {

  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");
  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("%s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());

  requestStatus status = planta.validateUserCredentials(body);

  // Registro OK → el usuario nuevo queda autenticado: emitimos un token de
  // sesión y lo devolvemos para que entre a editar 30 min sin re-loguearse. El
  // usuario ya quedó en RAM/Preferences; no se reinicia.
  if (status == STATUS_OK) {
    String token = planta.issueSessionToken();
    server.send(200, "application/json",
                "{\"status\":true,\"message\":\"Usuario registrado.\",\"token\":\"" + token + "\"}");
    return;
  }

  response = buildHttpResponse(status);
  server.send(response.code, response.contentType, response.body);
}

void handleGetParameters() {
  // Devuelve el estado del dispositivo en JSON (incluye hasRegisteredUser y, si
  // se manda ?token=, sessionValid). El token viene en la query porque /getparams
  // es GET; buildParamsJson lo valida y, si sigue vigente, renueva la sesión.
  server.send(200, "application/json", planta.buildParamsJson(server.arg("token")));
  Serial.println("*****  " + server.uri() + "  *****");
}

void handleAuthUserCredentials() {
  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");

  requestStatus status = planta.authUserCredentials(body);

  // Login OK → emitimos token de sesión y lo devolvemos para desbloquear la
  // edición durante 30 min sin re-autenticar en cada carga.
  if (status == STATUS_OK) {
    String token = planta.issueSessionToken();
    server.send(200, "application/json",
                "{\"status\":true,\"message\":\"Acceso concedido.\",\"token\":\"" + token + "\"}");
    return;
  }

  response = buildHttpResponse(status);

  // Valida para desbloquear la edición sin reiniciar, EXCEPTO cuando se recibe
  // el comando de reset (**reset**): ahí hardReset() ya corrió y reiniciamos.
  server.send(response.code, response.contentType, response.body);
  if (status == HARD_RESET) {
      Serial.println("[System] Hard reset");
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP.restart();
  }
}

void handleNewParameters() {
  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");
  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("%s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());

  requestStatus status = planta.validateCropParameters(body);
  response = buildHttpResponse(status);

  server.send(response.code, response.contentType, response.body);

}

void handleExit() {
  // Logout: invalida la sesión de edición en el dispositivo (el front además
  // limpia su token en localStorage). Responde JSON {status, message} que el
  // front pinta en la pantalla de cierre.
  planta.clearSession();
  server.send(200, "application/json", "{\"status\":true,\"message\":\"Desconectado correctamente. Ya puedes cerrar esta ventana y desconectarte de la red SmartPlant.\"}");
  Serial.println("*****  " + server.uri() + "  *****");
}

void handleWifiScan() {
  // Escaneo on-demand: el dispositivo busca redes y devuelve las 5 más fuertes
  // sin nombres repetidos. No usa el sobre {status,message}: entrega la lista
  // directa (igual que /getparams). Bloquea ~2 s mientras escanea.
  server.send(200, "application/json", planta.scanNetworks());
  Serial.println("*****  " + server.uri() + "  *****");
}

void handleWifiCredentials() {
  // Recibe {ssid, pass, token}, valida y ARRANCA la conexión (sin bloquear). El
  // front confirma el resultado por polling a /getparams (wifiConnected). Exige
  // token de sesión vigente, igual que /newparams.
  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");

  requestStatus status = planta.saveWifiCredentials(body);
  response = buildHttpResponse(status);
  server.send(response.code, response.contentType, response.body);
}

void printTask(void *pvParameters) {
  while (true) {
    planta.printSystemData();
    Serial.printf("Stack libre minimo: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(5000));  // Esperar 5 segundos
  }
}