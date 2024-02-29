#include <WiFi.h>
#include "Plant.h"
#include "Clock.h"
#include "Constants.h"


const char* ssid = "deviceName";
const char* password = "yourPassword";
unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long currentMillis = 0;

Plant p1;
Clock rtc;
WiFiServer server(port80);

void setup() {
  Serial.begin(115200); 
  rtc.startClock();
  if(p1.readParametersEEPROM());
    rtc.setCurrentTime(p1.getParameters());
  WiFi.softAP(ssid, password);
  Serial.print("Iniciado AP ");
  Serial.print(ssid);
  Serial.print(" IP address:");
  Serial.println(WiFi.softAPIP());
  server.begin();
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= intervalToUpdate) {
    previousMillis = currentMillis;
    wifiSearchData(p1.getParameters());
    rtc.getCurrentTime(p1.getParameters());
    Serial.println(p1.sendParameters());
  } 
  p1.turnOffDevices(); 
}

void wifiSearchData(uint8_t* parameters) {
  WiFiClient client = server.available();  // listen for incoming clients
  if (client) {                            // if you get a client,
    //Serial.println("");
    Serial.println("New Client.");                   // print a monthsage out the serial port
    String currentLine = "";                         // make a String to hold incoming data from the client
    while (client.connected()) {                     // loop while the client's connected
      if (client.available()) {                      // if there's bytes to read from the client,
        char c = client.read();                      // read a byte, then
        Serial.write(c);                             // print it out the serial monitor
        if (c == '\n' || currentLine.length() >= messageMaxLenght) {  // if the byte is a newline character
          if (currentLine.indexOf("PUT /p") >= zero && currentLine.indexOf("-HDD") == 41) {  // if the current line is blank, you got two newline characters in a row that's the end of the client HTTP request, so send a response:
            client.println("HTTP/1.1 200 OK"); // HTTP headers Response code (HTTP/1.1 200 OK) 
            client.println("Content-Type: text/html"); //client.println("Content-type:text/html");
            client.println(""); // and a content-type so the client knows what's coming, then a blank line: 
            for (uint8_t i = 1; i <= 12; i++) {
              parameters[i] = (currentLine[(i + 1) * 3] - '0') * 10; // Se añade decena
              parameters[i] = parameters[i] + (currentLine[((i + 1) * 3) + 1] - '0'); // Se añade unidad
              Serial.print(parameters[i]);
              Serial.print(".");
            }
            Serial.println(p1.updateEEPROM(currentLine[7]));
          }
          else if (currentLine.indexOf("GET /t") >= zero && currentLine.indexOf("-HDD") == 26) {
            client.println("HTTP/1.1 200 OK"); 
            client.println("Content-Type: text/html"); //client.println("Content-type:text/html");
            client.println("");
            for (uint8_t i = 1; i <= 7; i++) {
              parameters[17 + i] = (currentLine[(i + 1) * 3] - '0') * 10;
              parameters[17 + i] = parameters[17 + i] + (currentLine[((i + 1) * 3) + 1] - '0');
            }
            rtc.setCurrentTime(parameters);
            Serial.println("RTC Updated");
            setBuzzer();
          }
          else if (currentLine.indexOf("DELETE /x") >= zero && currentLine.indexOf("-HDD") == 9) {
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            Serial.println(p1.updateEEPROM(currentLine[8]));
          }
          else if (currentLine.indexOf("GET /d") >= zero){
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            client.println("Content-Type: text/html");
            client.println(""); //  Important.
            client.println("<!DOCTYPE HTML>");
            client.println("<html><head><meta charset=utf-8></head><body><center><font face='Arial'>");
            client.println("<h1>System: D800</h1>");
            client.println("<p>Sistema:" + String(parameters[systemActive]) + " Semana:" + String(parameters[cropWeek]) +
            " Dia:" + String(parameters[cropDay]) + " Fotoperiodo:" + String(parameters[photoperiod]) +
            " Luz:" + String(parameters[whiteLedStatus]) +
            " Irr:" + String(parameters[irrigationTime]) + "/" +  String(parameters[irrigationTimeMinute]) +
            " Vent:" + String(parameters[fanTime]) + "/" +  String(parameters[fanTimeMinute]) + "</p>");
            client.println("<p>" + String(parameters[day]) + "/" + String(parameters[month]) + "/" + String(parameters[year]) + 
            " - " + String(parameters[hour]) + ":" + String(parameters[minute]) + ":" + parameters[second] + "</p>");
            client.println("</font></center></body></html>");
            Serial.println("hay GET");//eliminar  
          }
          else
            client.println("HTTP/1.1 400 Bad Request"); 
            client.println("Content-Type: text/html"); //client.println("Content-type:text/html");
            client.println("");
            Serial.println("Invalid input");
          // The HTTP response ends with another blank line:
          client.println();
          currentLine = "";
          // break out of the while loop:
          break;
        } 
        else if (c != '\r')  // if you got anything else but a carriage return character,
          currentLine += c;                       // add it to the end of the currentLine
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.\n");
  }
}