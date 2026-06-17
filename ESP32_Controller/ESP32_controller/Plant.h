#ifndef _PLANT
#define _PLANT

#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
//#include <Update.h>

class Plant {

  public:

    Plant();
    void begin();

    bool getRegisteredUser();

    requestStatus validateUserCredentials(const String& body);

    requestStatus validateCropParameters(const String& body);

    void manageDevice(int devicePin, int scheduleHour, int scheduleMinute);

    void turnOnDevices();

    String buildShowParametersForm();
    String buildUpdateParametersForm();

    // Funciones para el control del RTC (DS3231)
    void startClock();
    bool setCurrentTime();
    bool getCurrentTime();

    void hardReset();


    void printSystemData();
    //void readSystemStatus(byte* systemStatus, size_t length);

    void updateCropDay();
    

    /*String mainHTML();
    String updateHTML();
    String registerUserHTML();
    String wellcomeHTML();
    String exitHTML();*/

    bool getToken();
    void downloadOTA();
    //bool testCredentials(String SSID, String pass);

  private:

    uint8_t _systemStatus[15] = {0};  
    uint8_t _currentTime[10];

    char _plantName[24];

    char _SSID[32];
    char _SSIDpass[64];
    char _username[32];
    char _userpass[64];
    char _MAC[18];

    String firmwareVersion;
    String jwtToken;
    
    Preferences p;
    HTTPClient http;

};

struct HttpResponse {
    uint16_t code;
    String contentType;
    String body;
};

HttpResponse buildHttpResponse(requestStatus status);

uint8_t bcd2bin(uint8_t bcd);
uint8_t bin2bcd(uint8_t bin);

//bool isValidString(const char* s);

bool isValidString(const String &s);
bool isValidChar(char c);
String maskPassword(const char* pass);

bool isValidReadableString(const String& s, bool allowSpaces);
bool hasTooManyRepeatedChars(const String& s);

void setBuzzer();

#endif


