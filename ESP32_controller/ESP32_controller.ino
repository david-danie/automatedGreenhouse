#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "Constants.h"
#include "Plant.h"
#include "mainForm.h"
#include "registerUserForm.h"

//uint8_t systemStatus[5] = {0}; 

DNSServer dnsServer;
WebServer server(80);

Plant planta;

HttpResponse response;

// index page handler
void handleRoot();
void handleNotFound();
void handleRegisterUser();
void handleUserCredentials();
void handleGetParameters();
void handleNewParameters();
void handleAuthUserCredentials();
void handleExit();

void setup() {

  //Serial.begin(115200);
  
  planta.begin();

  //Serial.printf("\nDispositivo %s %s tiene usuario registrado.", config.deviceID, config.hasRegisteredUser ? "si" : "no");

  WiFi.AP.begin();
  WiFi.AP.create("SmartPlant");
  WiFi.AP.enableDhcpCaptivePortal();

  // by default DNSServer is started serving any "*" domain name. It will reply
  // AccessPoint's IP to all DNS request (this is required for Captive Portal detection)
  if (dnsServer.start()) {
    Serial.println("[System] Portal captivo iniciado");
  } else {
    Serial.println("[System] No se puede iniciar portal captivo");
  }

  // serve a simple root page
  server.on("/", handleRoot);
  server.on("/registeru", handleRegisterUser);
  server.on("/usercredentials", HTTP_POST, handleUserCredentials);
  server.on("/getparams", HTTP_GET, handleGetParameters);
  server.on("/newparams", HTTP_POST, handleNewParameters);
  server.on("/authusercredentials", HTTP_POST, handleAuthUserCredentials);
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
  server.handleClient();
  planta.turnOnDevices();
  delay(250);  // give CPU some idle time
}

void handleRoot() {
  server.send(200, "text/html", dashboardForm);
  Serial.println(server.uri());  
}

void handleNotFound() {
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "redirect to captive portal");
}

void handleUserCredentials() {

  String body = server.arg("plain");
  Serial.println(server.uri());
  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("%s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());

  requestStatus status = planta.validateUserCredentials(body);
  response = buildHttpResponse(status);

  server.send(response.code, response.contentType, response.body);

  if(status == HARD_RESET){
      Serial.println("[System] Hard reset\n");
      delay(1000);
      ESP.restart();
  }
}

void handleGetParameters() {
  // Devuelve el estado del dispositivo en JSON (incluye hasRegisteredUser).
  server.send(200, "application/json", planta.buildParamsJson());
  Serial.println(server.uri());
}

void handleAuthUserCredentials() {
  String body = server.arg("plain");
  Serial.println(server.uri());

  requestStatus status = planta.authUserCredentials(body);
  response = buildHttpResponse(status);

  // Solo valida para desbloquear la edición: NO reinicia el dispositivo.
  server.send(response.code, response.contentType, response.body);
}

void handleNewParameters() {
  String body = server.arg("plain");
  Serial.println(server.uri());
  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("%s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());

  requestStatus status = planta.validateCropParameters(body);
  response = buildHttpResponse(status);

  server.send(response.code, response.contentType, response.body);
  if (status == STATUS_OK || status == HARD_RESET) {
      Serial.println("[System] Hard reset");
      delay(1000);
      ESP.restart();
  }

}

void handleRegisterUser() {
  server.send(200, "text/html", registerUserForm);
  Serial.println(server.uri());
}

void handleExit() {
  server.send(200, "text/html", "<h1>Desconectado correctamente</h1>");
  Serial.println(server.uri());
}

void printTask(void *pvParameters) {
  while (true) {
    planta.printSystemData();
    Serial.printf("Stack libre minimo: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(pdMS_TO_TICKS(5000));// Esperar 1 segundo
  }
}