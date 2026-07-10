#ifndef _PLANT
#define _PLANT

#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "Constants.h" 
//#include <Update.h>

class Plant {

  public:

    Plant();
    void begin();

    //bool getRegisteredUser();

    requestStatus validateUserCredentials(const String& body);

    requestStatus validateCropParameters(const String& body);

    // Valida credenciales (sin guardar nada) para desbloquear la edición.
    requestStatus authUserCredentials(const String& body);

    void manageDevice(int devicePin, int scheduleHour, int scheduleMinute);

    void turnOnDevices();

    // Serializa el estado actual del dispositivo a JSON para GET /getparams.
    String buildParamsJson();

    // Funciones para el control del RTC (DS3231)
    //void startClock();
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

    uint8_t _systemStatus[18] = {0};  
    uint8_t _currentTime[10];

    // Buffers dimensionados al peor caso UTF-8 (maxChars * 2 + 1) para que un
    // valor válido del formulario nunca se trunque al guardarse.
    char _plantName[maxPlantNameChars * utf8MaxBytesPerChar + 1];   // 41
    char _username[maxUsernameChars  * utf8MaxBytesPerChar + 1];    // 65
    char _userpass[maxUserpassChars  * utf8MaxBytesPerChar + 1];    // 129

    char _SSID[32];
    char _SSIDpass[64];
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




bool isValidString(const String &s);
bool isValidChar(char c);





void setBuzzer();

#endif
