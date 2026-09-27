#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>
#include "Constants.h"
#include "sensible.h"
#include "Plant.h"
#include "mainForm.h" 

DNSServer dnsServer;
// Ligado a la IP del SoftAP, NO al wildcard: el portal es el plano de control
// LOCAL y no debe responder por la interfaz STA. Con `WebServer server(80)` el
// socket se ligaba a 0.0.0.0 y, en cuanto el equipo se unía a la red del usuario,
// todos los endpoints quedaban accesibles desde esa LAN sin conocer la passphrase
// del AP. El constructor solo guarda la dirección; el bind() real ocurre en
// server.begin(), que corre cuando el AP ya está levantado.
WebServer server(apGatewayIp, 80);

Plant planta;

HttpResponse response;

// index page handler
void handleRoot();
void handleNotFound();
void handleUserCredentials();
void handleGetParameters();
void handleNewParameters();
void handleNewCrop();
void handleAuthUserCredentials();
void handleExit();
void handleWifiScan();
void handleWifiCredentials();
void handleOtaUpdate();
void handleOtaUpload();

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

  // El WebServer se liga a apGatewayIp (ver Constants.h). Si el SoftAP no acabara
  // en esa IP, el bind fallaría en silencio y el portal quedaría inaccesible sin
  // ningún error visible, así que se comprueba y se avisa aquí.
  if (WiFi.softAPIP() != apGatewayIp) {
    Serial.printf("[AP] AVISO: el SoftAP tiene %s pero el servidor se liga a %s. "
                  "El portal NO responderá: ajusta apGatewayIp en Constants.h.\n",
                  WiFi.softAPIP().toString().c_str(), apGatewayIp.toString().c_str());
  } else {
    Serial.printf("[AP] Portal servido solo en %s (la interfaz STA no lo expone)\n",
                  apGatewayIp.toString().c_str());
  }

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
  server.on("/newcrop", HTTP_POST, handleNewCrop);
  server.on("/authusercredentials", HTTP_POST, handleAuthUserCredentials);
  server.on("/wifiscan", HTTP_GET, handleWifiScan);
  server.on("/wificredentials", HTTP_POST, handleWifiCredentials);
  // OTA local: subida del .bin por multipart. Dos callbacks: el segundo recibe el
  // archivo por trozos (handleOtaUpload) y el primero responde al terminar
  // (handleOtaUpdate). El token va en el header Authorization: Bearer (el cuerpo
  // es multipart), y por eso hay que registrar esa cabecera con collectHeaders()
  // para que WebServer la conserve.
  const char* otaHeaders[] = { "Authorization" };
  server.collectHeaders(otaHeaders, 1);
  server.on("/otaupdate", HTTP_POST, handleOtaUpdate, handleOtaUpload);
  server.on("/exit", handleExit);
  server.onNotFound(handleNotFound);
  server.begin();

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
    // Único punto de I²C periódico: refresca _currentTime desde el RTC.
    planta.getCurrentTime();
    planta.turnOnDevices();
    // Persiste las credenciales Wi-Fi si un intento pendiente acaba de conectar.
    planta.updateWifi();
  }

  // El log del estado se imprime AQUÍ, no en una tarea aparte. Antes lo hacía un
  // printTask que leía _currentTime/_systemStatus mientras este loop los
  // reescribía: sin sincronización, podía imprimir un arreglo a medio actualizar
  // (p. ej. una hora mezclada). Con un solo task tocando ese estado la carrera
  // desaparece por construcción, sin necesidad de mutex, y se ahorran los 2 KB de
  // stack de la tarea.
  static uint32_t lastLog = 0;
  if (now - lastLog >= systemLogInterval) {
    lastLog = now;
    planta.printSystemData();
    //Serial.printf("Stack libre minimo (loop): %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
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
  // es GET; buildParamsJson lo valida para reportar sessionValid, pero NO renueva
  // la sesión: la ventana es FIJA desde el login.
  server.send(200, "application/json", planta.buildParamsJson(server.arg("token")));
  Serial.println("*****  " + server.uri() + "  *****");
}

void handleAuthUserCredentials() {
  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");

  // ¿La petición entró por la interfaz del AP? localIP() es la IP LOCAL del socket,
  // es decir la interfaz que aceptó la conexión: por el AP vale softAPIP(), por el
  // STA la IP de la red del usuario. No es un dato que el cliente envíe, así que no
  // se puede falsificar como una cabecera. Solo se usa para autorizar el comando de
  // reset (ver Plant::authUserCredentials). Fail-closed: si no hay IP de AP válida
  // se deniega, en lugar de permitir.
  IPAddress apIp = WiFi.softAPIP();
  bool fromAP = (apIp != IPAddress((uint32_t)0)) && (server.client().localIP() == apIp);

  requestStatus status = planta.authUserCredentials(body, fromAP);

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

void handleNewCrop() {
  // Cierra el cultivo actual: borra nombre, ancla de días y parámetros, y conserva
  // la cuenta y la Wi-Fi. Exige token de sesión (es una acción destructiva del
  // dueño, no una vía de recuperación). El reset de FÁBRICA es otra cosa: vive en
  // /authusercredentials como el comando **reset**, sin token y solo por el AP.
  String body = server.arg("plain");
  Serial.println("*****  " + server.uri() + "  *****");

  requestStatus status = planta.startNewCrop(body);
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
  // directa (igual que /getparams). Asíncrono: arranca el escaneo y responde
  // scanning=true; el front consulta hasta recibir la lista.
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

// ===== OTA local (POST /otaupdate) =====
// Recibe el .bin por multipart y lo escribe en el slot OTA inactivo con la
// librería Update; al terminar OK, el bootloader arrancará el firmware nuevo.
// Requiere doble slot OTA (ver partitions.csv): sin él, Update.begin() falla.
//
// Seguridad (fail-closed): se exige token de sesión vigente Y que la petición
// entre por la interfaz del AP, igual que el resto del plano de control. La
// decisión se toma en UPLOAD_FILE_START —antes de escribir un solo byte— y se
// recuerda en estas variables para que los trozos siguientes y la respuesta
// final actúen en consecuencia.
static bool     otaAuthorized = false;   // token + AP validados en START
static uint16_t otaHttpError  = 0;        // 0 = sin error de autorización

void handleOtaUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaAuthorized = false;
    otaHttpError  = 0;

    // ¿Entró por el AP? Igual que en el reset: la IP local del socket es la
    // interfaz que aceptó la conexión (no la puede falsificar el cliente).
    IPAddress apIp = WiFi.softAPIP();
    bool fromAP = (apIp != IPAddress((uint32_t)0)) && (server.client().localIP() == apIp);

    // Token en el header Authorization: Bearer <token>. Se lee de las cabeceras
    // (no del cuerpo, que es multipart), así que llega ANTES del binario y permite
    // autorizar en START, antes de escribir un solo byte en flash.
    // Requiere que "Authorization" se haya registrado con server.collectHeaders()
    // en setup(); si no, WebServer no lo conserva.
    String auth = server.header("Authorization");
    String token = "";
    if (auth.startsWith("Bearer ")) token = auth.substring(7);
    token.trim();

    if (!planta.isSessionValid(token)) {
      otaHttpError = 401;  // sesión inválida/expirada
      return;              // NO se llama a Update.begin(): no se escribe nada
    }
    if (!fromAP) {
      otaHttpError = 403;  // solo por la interfaz del AP
      return;
    }

    Serial.printf("[OTA] Inicio: %s\n", upload.filename.c_str());
    // UPDATE_SIZE_UNKNOWN: el tamaño real se conoce al terminar; Update usa el
    // slot OTA inactivo y valida al final.
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      otaHttpError = 500;  // no se pudo iniciar (¿sin doble slot OTA?)
      return;
    }
    otaAuthorized = true;

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!otaAuthorized) return;  // subida no autorizada: se ignoran los datos
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      otaHttpError = 500;
      otaAuthorized = false;
      Update.abort();
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (!otaAuthorized) return;
    if (!Update.end(true)) {   // true = fija el tamaño final y valida la imagen
      Update.printError(Serial);
      otaHttpError = 500;
      otaAuthorized = false;
    } else {
      Serial.printf("[OTA] Completado: %u bytes\n", upload.totalSize);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaAuthorized = false;
    Serial.println("[OTA] Abortado por el cliente");
  }
}

void handleOtaUpdate() {
  // Respuesta final: corre cuando la subida terminó (o falló en autorización).
  Serial.println("*****  /otaupdate  *****");

  if (otaHttpError == 401) {
    server.send(401, "application/json",
                "{\"status\":false,\"message\":\"Tu sesión expiró. Inicia sesión y reintenta.\"}");
    return;
  }
  if (otaHttpError == 403) {
    server.send(403, "application/json",
                "{\"status\":false,\"message\":\"La actualización solo se permite desde la red del equipo (AP).\"}");
    return;
  }
  if (otaHttpError != 0 || !Update.isFinished()) {
    server.send(500, "application/json",
                "{\"status\":false,\"message\":\"No se pudo instalar el firmware. El equipo conserva el anterior.\"}");
    return;
  }

  // Éxito: la imagen quedó escrita y validada en el slot inactivo. Respondemos y
  // reiniciamos; el bootloader arrancará el firmware nuevo.
  server.send(200, "application/json",
              "{\"status\":true,\"message\":\"Firmware instalado. El equipo se reinicia.\"}");
  Serial.println("[OTA] Reiniciando al firmware nuevo…");
  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP.restart();
}