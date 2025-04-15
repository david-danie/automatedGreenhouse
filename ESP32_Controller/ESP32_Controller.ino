#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "Constants.h"
#include "Plant.h"

char buffer[300];

bool hasRegisteredUser;
unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long currentMillis = 0;

Plant planta;
//IPAddress apIP(8,8,4,4); // The default android DNS
DNSServer dnsServer;
WebServer server(port80);

void handleRoot();
void handleRootNoUser();
void handleRegisterUser();
void handlePost();
void handleExit();

void setup() {
  
  Serial.begin(115200);
  planta.begin();
  hasRegisteredUser = planta.getRegisteredUser();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(deviceName);
  vTaskDelay(100);
  //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(dnsPort, "*", WiFi.softAPIP());

  server.on("/exit", handleExit);
  server.on("/", handleRoot);
  server.onNotFound(handleRoot);  // Redirige todas las solicitudes a la página de inicio

  server.on("/update", HTTP_POST, handleUpdate);  // Configura el servidor web
  server.on("/data", HTTP_POST, handlePostData); 
  
  server.on("/registeru", handleRegisterUser);
  server.on("/userdata", HTTP_POST, handleUserData); 

  if(hasRegisteredUser){
    Serial.println("YA HAY usuario registrado");
  }
  else{
    Serial.println("NO HAY usuario registrado");
    //server.on("/dataUser", HTTP_POST, handlePostDataUser); 
  }
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
  if (hasRegisteredUser)
    server.send(200, "text/html", planta.mainHTML());
  else
    server.send(200, "text/html", planta.wellcomeHTML());
  //Serial.println("root");
  Serial.println(server.uri());

  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("Argumento: %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());

}

void handleRegisterUser() {
  server.send(200, "text/html", planta.registerUserHTML());
  Serial.println(server.uri());
}

void handleExit() {
  server.send(200, "text/html", "<h1>Desconectado correctamente</h1>");
  Serial.println(server.uri());
}

void handleUpdate() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", planta.updateHTML());
  Serial.println("update");
  Serial.println(server.uri());
}

void handlePostData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String body = server.arg("plain");
  Serial.println(body);
  Serial.println("post data");
  Serial.println(server.uri());
  if(planta.processPostBody(body) == 2){
    //server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"status\":\"200\",\"msg\":\"Informacion valida\"}");
    planta.turnOffDevices();
  } else if(planta.processPostBody(body) == 0) {
    //server.sendHeader("Connection", "close");
    server.send(401, "application/json", "{\"status\":\"400\",\"msg\":\"Informacion invalida\"}");
  } else if(planta.processPostBody(body) == 1) {
    //server.sendHeader("Connection", "close");
    server.send(401, "application/json", "{\"status\":\"200\",\"msg\":\"Sistema reiniciado\"}");
  }
}

void handleUserData(){
  String body = server.arg("plain");
  Serial.print("JSON recibido: ");
  Serial.println(body);
  server.send(200, "application/json", "{\"status\":\"200\",\"msg\":\"Se recibió información de usuario\"}");
}