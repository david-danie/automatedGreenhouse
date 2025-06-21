#ifndef _PLANT
#define _PLANT

#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

class Plant {

  public:

    Plant();

    void begin();
    byte processPostBody(String body);
    char* getSystemStatus(char* buffer);
    //void readSystemStatus(byte* systemStatus, size_t length);

    // Funciones para el control del RTC (DS3231)
    void startClock();
    bool setCurrentTime();
    bool getCurrentTime();

    void turnOffDevices();
    void manageDevice(int devicePin, int scheduleHour, int scheduleMinute);

    void updateCropDay();
    
    bool getRegisteredUser();

    String mainHTML();
    String updateHTML();
    String registerUserHTML();
    String wellcomeHTML();
    String exitHTML();

    bool getToken();
    void downloadOTA();
    //bool testCredentials(String SSID, String pass);

  private:
    
    SemaphoreHandle_t mutex;
    byte _systemStatus[15];
    byte _currentTime[10];
    String jwtToken;
    String firmwareVersion;
    String macID;
    
    Preferences preferences;
    HTTPClient http;

};

uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);
void setBuzzer();

#endif


