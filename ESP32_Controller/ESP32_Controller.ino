#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "Constants.h"
#include "Plant.h"

//#include "updateSistemHTML.h"

char buffer[300];
byte currentTime[10];

unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long currentMillis = 0;

Plant planta;
//IPAddress apIP(8,8,4,4); // The default android DNS
DNSServer dnsServer;
WebServer server(port80);

void handleRoot();
void handleExit();
void handlePost();

void setup() {

  Serial.begin(115200);
  planta.begin();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(deviceName);
  vTaskDelay(100);
  //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(dnsPort, "*", WiFi.softAPIP());
  
  server.onNotFound(handleRoot);  // Redirige todas las solicitudes a la página de inicio
  server.on("/exit", HTTP_GET, handleExit);
  server.on("/update", HTTP_POST, handleUpdate);  // Configura el servidor web
  server.on("/data", HTTP_POST, handlePostData); 
  server.begin();
  
}

void loop() {

  currentMillis = millis();
  if (currentMillis - previousMillis >= intervalToSend) {
    previousMillis = currentMillis;
    Serial.println(planta.getSystemStatus(buffer));
  } 
  dnsServer.processNextRequest();
  server.handleClient();

}

void handleRoot() {
  server.send(200, "text/html", planta.mainHTML());
  Serial.println(server.uri());

  if (server.args() > 0) {
    for (int i = 0; i < server.args(); i++) {
      Serial.printf("Argumento: %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
    }
  }
  /*if (server.uri() == "/salir"){
    server.send(200, "text/html", "<h1>Te haz desconectado del equipo.</h1>");
    Serial.println(server.uri());
  } else {
    server.send(200, "text/html", loginPage);
    Serial.println(server.uri());
  }*/
}
void handleExit() {
  //server.sendHeader("Connection", "close");
  server.send(200, "text/html", "<h1>Te haz desconectado del dispositivo.</h1>");
  Serial.println(server.uri());
  if (server.args() > 0) {
    for (int i = 0; i < server.args(); i++) {
      Serial.printf("Argumento: %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
    }
  }
}

void handleUpdate() {
  server.send(200, "text/html", planta.updateHTML());
  Serial.println(server.uri());
}

void handlePostData() {
  String body = server.arg("plain");
  Serial.println(body);
  Serial.println(server.uri());
  if (planta.processPostBody(body)){
    //server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"status\":\"200\",\"msg\":\"Informacion valida\"}");
    planta.turnOffDevices();
  } else {
    //server.sendHeader("Connection", "close");
    server.send(401, "application/json", "{\"status\":\"400\",\"msg\":\"Informacion invalida\"}");
  }
}