#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
//#include <max6675.h>

const char* ssid = "deviceName";
const char* password = "yourPassword";

const uint8_t port80 = 80;
const uint8_t messageMaxLenght = 60;

// setting PWM properties
const int pwmFrequency = 1000;
const uint8_t pwmChannel0 = 0;
const uint8_t pwmChannel2 = 2;
const uint8_t pwmChannel4 = 4;
const uint8_t pwmResolution = 8;
const uint8_t maxDutyCycle = 255;
const uint8_t factorOf100 = 5;

const uint8_t second = 1;
const uint8_t minute = 2;
const uint8_t hour = 3;
const uint8_t dayOfWeek = 4;
const uint8_t day = 5;
const uint8_t month = 6;
const uint8_t year = 7;
const uint8_t ctrl = 8;
const uint8_t DS3231Adress = 0x68;
const uint8_t rtcReadBytes = 7;

const uint8_t eepromBytes = 12;

const uint8_t buzzerPin = 2;
const uint8_t blueLedPin = 3; // Pin asociado a el led azul
const uint8_t redLedPin = 4;
const uint8_t greenLedPin = 5;
const uint8_t whiteLedPin = 6;
const uint8_t waterPumpPin = 7;
const uint8_t fanPin = 8;
const uint8_t deviceFourPin = 9;

const uint8_t thermoCSPin = 10;
//
const uint8_t systemActive = 1; //index
const uint8_t photoperiod = 2; //index
const uint8_t whiteLedStatus = 3; //index
const uint8_t blueDutyCycle = 4; //index
const uint8_t redDutyCycle = 5; //index
const uint8_t greenDutyCycle = 6; //index
const uint8_t irrigationTime = 7;
const uint8_t irrigationTimeMinute = 8;
const uint8_t fanTime = 9;
const uint8_t fanTimeMinute = 10;
const uint8_t deviceFour = 13;
const uint8_t cropWeek = 11;
const uint8_t cropDay = 12;
const uint8_t userIrrigationHour = 14;
const uint8_t userFanHour = 15;

const uint8_t onceAday = 1; // 8 times
const uint8_t eachThreeHours = 2; // 8 times
const uint8_t eachEightHours = 3; // 3 times
const uint8_t eachHour = 4; // 24 times

const long intervalToWifiUpdate = 1000;  // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;  // will store last time LED was updated
unsigned long currentMillis = 0;

unsigned long intervalToAddDay = 3000;
unsigned long previousMillisAddDay = 0;

const uint8_t BuzzerOn = 50;  // interval at which to blink (milliseconds)
const uint8_t BuzzerOff = 80;  // interval at which to blink (milliseconds)

const uint8_t zero = 0;

char buffer[100];
uint8_t systemStatus[20];
uint8_t currentTime[10];

//uint8_t thermoDO = 12;
//uint8_t thermoCS = 11;
//uint8_t thermoCLK = 10;

bool whiteLedBool;

WiFiServer server(port80);
//MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

void setup() {

  Serial.begin(115200);
  Wire.begin();
  if (!EEPROM.begin(eepromBytes))
    Serial.println("failed to initialise EEPROM");

  ledcSetup(pwmChannel0, pwmFrequency, pwmResolution);
  ledcSetup(pwmChannel2, pwmFrequency, pwmResolution);
  ledcSetup(pwmChannel4, pwmFrequency, pwmResolution);
  ledcAttachPin(blueLedPin, pwmChannel0);
  ledcAttachPin(redLedPin, pwmChannel2);
  ledcAttachPin(greenLedPin, pwmChannel4);
  pinMode(whiteLedPin, OUTPUT);
  pinMode(waterPumpPin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(deviceFourPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
 
  /*Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("WiFi connected to IP: ");
  Serial.println(WiFi.localIP());*/

  WiFi.softAP(ssid, password);
  Serial.print("Iniciado AP ");
  Serial.print(ssid);
  Serial.print(" IP address:");
  Serial.println(WiFi.softAPIP());

  server.begin();

  systemStatus[photoperiod] = zero;
  systemStatus[blueDutyCycle] = zero;
  systemStatus[redDutyCycle] = zero;
  systemStatus[greenDutyCycle] = zero;


  systemStatus[userIrrigationHour] = zero;
  systemStatus[userFanHour] = zero;
  systemStatus[cropWeek] = zero;
  systemStatus[cropDay] = zero;

  if(EEPROM.read(systemActive) == 1)
    for (int i = 1; i <= 12; i++)
      systemStatus[i] = EEPROM.read(i);
 
  if (systemStatus[whiteLedStatus] == zero)
    whiteLedBool = false;
  else
    whiteLedBool = true;
  analogWrite(blueLedPin, systemStatus[blueDutyCycle]);
  analogWrite(redLedPin, systemStatus[redDutyCycle]);
  analogWrite(greenLedPin, systemStatus[greenDutyCycle]);
  digitalWrite(whiteLedPin, whiteLedBool);

  currentTime[second] = 55;
  currentTime[minute] = 20;
  currentTime[hour] = 4;
  currentTime[dayOfWeek] = 1;
  currentTime[day] = 21;
  currentTime[month] = 5;
  currentTime[year] = 36;
  writeRTC();
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= intervalToWifiUpdate) {
    previousMillis = currentMillis;
    getTimeRTC();
    openWifi();
    sendCurrentParameters();
  }


  if (currentTime[hour] == 23 && currentTime[minute] == 59 && currentTime[second] >= 58) {
    currentMillis = millis();
    if (currentMillis - previousMillisAddDay >= intervalToAddDay) {
      if (systemStatus[cropDay] <= 6) {
        systemStatus[cropDay] += 1;
      //EEPROM.write(cropDay, systemStatus[cropDay]);
      }
      else {
        systemStatus[cropDay] = 1;
        systemStatus[cropWeek] += 1;
      //EEPROM.write(cropDay, systemStatus[cropDay]);
      //EEPROM.write(cropWeek, systemStatus[cropWeek]);
      }
      //EEPROM.commit();
      previousMillisAddDay = currentMillis;
    }
   
  }
  setDevices();
}


void sendCurrentParameters() {
  sprintf(buffer, "Plant:%d Week:%d Day:%d Fp:%d White:%d Blue:%d%% Red:%d%% Green:%d%%\nIrr:%dh/%dm Fan:%dh/%dm %02d/%02d/%02d %02d:%02d:%02d",
    systemStatus[systemActive], systemStatus[cropWeek], systemStatus[cropDay], systemStatus[photoperiod], systemStatus[whiteLedStatus],
    systemStatus[blueDutyCycle] * factorOf100, systemStatus[redDutyCycle] * factorOf100, systemStatus[greenDutyCycle] * factorOf100,
    systemStatus[irrigationTime], systemStatus[irrigationTimeMinute], systemStatus[fanTime], systemStatus[fanTimeMinute],
    currentTime[day], currentTime[month], currentTime[year], currentTime[hour], currentTime[minute], currentTime[second]);
//  Serial.print(thermocouple.readCelsius());
//  Serial.print("°C ");
  //Serial.println(WiFi.softAPIP());
  Serial.println(buffer);
  Serial.println();
}


void openWifi() {
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
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:


          if (currentLine.indexOf("PUT /p") >= zero && currentLine.indexOf("-HDD") == 41) {
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            for (uint8_t i = 1; i <= 12; i++) {
              if (i == 3)
                systemStatus[deviceFour] = currentLine[(i + 1) * 3] - '0';
              systemStatus[i] = (currentLine[(i + 1) * 3] - '0') * 10;
              systemStatus[i] = systemStatus[i] + (currentLine[((i + 1) * 3) + 1] - '0');
              if (currentLine[7] == '1')
                EEPROM.write(i, systemStatus[i]);
              Serial.print(systemStatus[i]);
              Serial.print(".");
            }
            if (currentLine[7] == '1') {
              EEPROM.commit();
              Serial.println(" Parameters saved");
              setBuzzer();
            }
            else {
              EEPROM.write(zero, systemStatus[systemActive]);
              EEPROM.commit();
              Serial.println("Parameters updated");
              digitalWrite(buzzerPin, HIGH);
              delay(BuzzerOn);
              digitalWrite(buzzerPin, LOW);
            }
          }
          else if (currentLine.indexOf("GET /t") >= zero && currentLine.indexOf("-HDD") == 26) {
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            for (uint8_t i = 1; i <= 7; i++) {
              currentTime[i] = (currentLine[(i + 1) * 3] - '0') * 10;
              currentTime[i] = currentTime[i] + (currentLine[((i + 1) * 3) + 1] - '0');
            }
            writeRTC();
            Serial.println("RTC Updated");
            setBuzzer();
          }
          else if (currentLine.indexOf("DELETE /x") >= zero && currentLine.indexOf("-HDD") == 9) {
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            for (uint8_t i = 1; i <= 15; i++) {
              EEPROM.write(i, zero);
              systemStatus[i] = zero;
            }
            EEPROM.commit();  
            Serial.println("Data erased");
            setBuzzer();
          }
          else if (currentLine.indexOf("GET /d") >= zero){
            client.println("HTTP/1.1 200 OK"); //client.println("Content-type:text/html");
            client.println("Content-Type: text/html");
            client.println(""); //  Important.
            client.println("<!DOCTYPE HTML>");
            client.println("<html><head><meta charset=utf-8></head><body><center><font face='Arial'>");
            client.println("<h1>System: D800</h1>");
            client.println("<p>Sistema:" + String(systemStatus[systemActive]) + " Semana:" + String(systemStatus[cropWeek]) +
            " Dia:" + String(systemStatus[cropDay]) + " Fotoperiodo:" + String(systemStatus[photoperiod]) +
            " Luz:" + String(systemStatus[whiteLedStatus]) +
            " Irr:" + String(systemStatus[irrigationTime]) + "/" +  String(systemStatus[irrigationTimeMinute]) +
            " Vent:" + String(systemStatus[fanTime]) + "/" +  String(systemStatus[fanTimeMinute]) + "</p>");
            client.println("<p>" + String(currentTime[day]) + "/" + String(currentTime[month]) + "/" + String(currentTime[year]) + 
            " - " + String(currentTime[hour]) + ":" + String(currentTime[minute]) + ":" + currentTime[second] + "</p>");
            client.println("</font></center></body></html>");
            Serial.println("hay GET");//eliminar  
          }
          else
            Serial.println("Invalid input");
          // The HTTP response ends with another blank line:
          client.println();
          currentLine = "";
          // break out of the while loop:
          break;


        } else if (c != '\r')  // if you got anything else but a carriage return character,
          currentLine += c;                       // add it to the end of the currentLine
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.\n");
  }
}

void setDevices() {
  if(currentTime[hour] < systemStatus[photoperiod]) {
    if (systemStatus[whiteLedStatus] == zero)
      whiteLedBool = false;
    else
      whiteLedBool = true;
    digitalWrite(whiteLedPin, whiteLedBool);
    analogWrite(blueLedPin, map(systemStatus[blueDutyCycle], zero, 20, zero, maxDutyCycle));
    analogWrite(redLedPin, map(systemStatus[redDutyCycle], zero, 20, zero, maxDutyCycle));
    analogWrite(greenLedPin, map(systemStatus[greenDutyCycle], zero, 20, zero, maxDutyCycle));
  }
  else {
    digitalWrite(whiteLedPin, LOW);
    analogWrite(blueLedPin, map(zero, zero, 20, zero, maxDutyCycle));
    analogWrite(redLedPin, map(zero, zero, 20, zero, maxDutyCycle));
    analogWrite(greenLedPin, map(zero, zero, 20, zero, maxDutyCycle));
  }
  switch (systemStatus[irrigationTime]) {
  case onceAday:
    if(currentTime[hour] == userIrrigationHour && currentTime[minute] < systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  case eachThreeHours:
    if((currentTime[hour] % 3 == zero || currentTime[hour] == zero) && currentTime[minute] < systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  case eachEightHours:
    if((currentTime[hour] % 8 == zero || currentTime[hour] == zero) && currentTime[minute] < systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  case eachHour:
    if(currentTime[minute] < systemStatus[irrigationTimeMinute])
      digitalWrite(waterPumpPin, HIGH);
    else
      digitalWrite(waterPumpPin, LOW);
    break;
  default:
    digitalWrite(waterPumpPin, LOW);
  }
  switch (systemStatus[fanTime]) {
  case onceAday:
    if(currentTime[hour] == userFanHour && currentTime[minute] < systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachThreeHours:
    if((currentTime[hour] % 3 == zero || currentTime[hour] == zero) && currentTime[minute] < systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachEightHours:
    if((currentTime[hour] % 8 == zero || currentTime[hour] == zero) && currentTime[minute] < systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  case eachHour:
    if(currentTime[minute] < systemStatus[fanTimeMinute])
      digitalWrite(fanPin, HIGH);
    else
      digitalWrite(fanPin, LOW);
    break;
  default:
    digitalWrite(fanPin, LOW);
  }
  if (systemStatus[deviceFour] == zero)
    digitalWrite(deviceFourPin, LOW);
  else
    digitalWrite(deviceFourPin, HIGH);
}


void setBuzzer() {
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
  delay(BuzzerOff);
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
  delay(BuzzerOff);
  digitalWrite(buzzerPin, HIGH);
  delay(BuzzerOn);
  digitalWrite(buzzerPin, LOW);
}


bool getTimeRTC() {
  Wire.beginTransmission(DS3231Adress);  // Inicia el protocolo en modo lectura.
  Wire.write(0x00);                      // Si la escritura se llevo a cabo el metodo endTransmission retorna 0
  if (Wire.endTransmission() != zero)    // Terminamos la escritura y verificamos si el DS1307 respondio
    return false;                        // Escribir la dirección del segundero
  Wire.requestFrom(DS3231Adress, rtcReadBytes);     // Si el DS1307 esta presente, comenzar la lectura de 8 bytes
    for (uint8_t i = 1; i <= rtcReadBytes; i ++)
      currentTime[i] = bcd2bin(Wire.read());  
  return true;
}
// ==========================================================================================
// ============================ FUNCIÓN PARA ESCRIBIR EL DS3132 =============================
// ==========================================================================================
bool writeRTC() {
  // Iniciar el intercambio de información con el DS1307 (0x68)
  Wire.beginTransmission(DS3231Adress);  // 1° Byte = Dirección del chip ds1307
  Wire.write(0x00);                      // 2° Byte = Dirección del registro de segundos
    for (uint8_t i = 1; i <= rtcReadBytes; i ++)
      Wire.write(bin2bcd(currentTime[i]));
  if (Wire.endTransmission() != zero)  // Terminamos la escritura y verificamos si el DS1307 respondio
  return false;
}


uint8_t bcd2bin(uint8_t bcd) {
  return (bcd / 16 * 10) + (bcd % 16);
  }
// =============================================================================================
uint8_t bin2bcd(uint8_t bin) {
  return (bin / 10 * 16) + (bin % 10);
}
