#include <WiFi.h>
#include <WiFiClientSecure.h>
//#include <PubSubClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "Constants.h"
#include "Plant.h"
#include "sensible.h"

char buffer[300];

bool hasRegisteredUser;
unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long currentMillis = 0;

Plant planta;
//IPAddress apIP(8,8,4,4); // The default android DNS
WiFiClientSecure net;
//PubSubClient client(net);

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

  delay(1000);
  WiFi.begin(SSID, PASSWORD);
  delay(1000);
  //Serial.print("Conectando a la red: " + ssidU);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    if (millis() - startTime > 15000){          // Espera máximo 20 segundos
      Serial.println("\nNo se pudo conectar a la red WiFi.");
      WiFi.disconnect();
      break;
    }
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED){
    Serial.print("\nDispositivo conectado.");
    //Serial.println("IP asignada:  " + WiFi.localIP());
  }

  WiFi.softAP(deviceName);
  vTaskDelay(100);
  //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(dnsPort, "*", WiFi.softAPIP());

  server.onNotFound(handleRoot);  // Redirige todas las solicitudes a la página de inicio
  server.on("/", handleRoot);
  server.on("/registeru", handleRegisterUser);
  server.on("/userdata", HTTP_POST, handleUserData);
  server.on("/update", handleUpdate);  // Configura el servidor web
  server.on("/updatedata", HTTP_POST, handlePostData); 
  server.on("/exit", handleExit);

  //connectAWS();
  
  if(hasRegisteredUser)
    Serial.println("YA HAY usuario registrado");
  else
    Serial.println("NO HAY usuario registrado");
  server.begin();

  xTaskCreatePinnedToCore(serverTask, "serverTask", 3000, NULL, 1, NULL,0);
  xTaskCreatePinnedToCore(downloadBin, "downloadBin", 3000, NULL, 1, NULL,0);
 
}

void loop() {

  currentMillis = millis();
  if (currentMillis - previousMillis >= intervalToSend) {
    previousMillis = currentMillis;
    String payload = planta.getSystemStatus(buffer, sizeof(buffer));
    //client.publish("test/topic", payload.c_str());
    Serial.println("Publicado: " + payload + '\n');
  } 
  //client.loop();

  //dnsServer.processNextRequest();
  //server.handleClient();

}

/*void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje en [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}*/

/*void connectAWS() {
  // Configurar certificados
  net.setCACert(ROOT_CERTIFICADE);
  net.setCertificate(CLIENT_CERTIFICADE);
  net.setPrivateKey(CLIENT_KEY);

  client.setServer(URL, MQTT_PORT);
  client.setCallback(messageHandler);

  Serial.print("Conectando a AWS IoT...");
  while (!client.connect("esp32Client")) {  // clientID único
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" ✅ Conectado a AWS IoT Core!");

  // Suscribirnos a un topic
  client.subscribe("test/topic/sub");
}*/

void handleRoot() {
  if (hasRegisteredUser)
    server.send(200, "text/html", planta.mainHTML());
  else
    server.send(200, "text/html", planta.wellcomeHTML());
  Serial.println(server.uri());

  if (server.args() > 0) 
    for (int i = 0; i < server.args(); i++) 
      Serial.printf("Argumento: %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
}

void handleRegisterUser() {
  server.send(200, "text/html", planta.registerUserHTML());
  Serial.println(server.uri());
}

void handleUserData(){
  String body = server.arg("plain");
  Serial.print("JSON recibido: ");
  Serial.println(body);
  server.send(200, "application/json", "{\"status\":\"200\",\"msg\":\"Se recibió información de usuario\"}");
}

void handleUpdate() {
  server.send(200, "text/html", planta.updateHTML());
  Serial.println("update");
  Serial.println(server.uri());
}

void handlePostData() {
  String body = server.arg("plain");
  Serial.println(body);
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

void handleExit() {
  server.send(200, "text/html", "<h1>Desconectado correctamente</h1>");
  Serial.println(server.uri());
}

void serverTask(void *pvParameters) {
  while (true) {
    dnsServer.processNextRequest();
    server.handleClient();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Espera 1 segundo
  }
}

void downloadBin(void *pvParameters) {
  while (true) {
    planta.getToken();
    planta.downloadOTA();
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Espera 1 segundo
  }
}