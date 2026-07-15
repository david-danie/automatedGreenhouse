#ifndef _PLANT
#define _PLANT

#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "Constants.h" 

class Plant {

  public:

    Plant();
    void begin();

    bool getRegisteredUser();

    // ---- Getters para configurar la red desde setup() (.ino) ----
    // ¿Hay credenciales Wi-Fi guardadas en NVS? Decide AP puro vs AP+STA al
    // arrancar. (No se llama hasWifiCredentials: ese nombre es un enumerador de
    // Constants.h e indexa _systemStatus.)
    bool getWifiCredentials();
    // SSID / passphrase de la red del usuario para arrancar el STA (WiFi.begin()).
    const char* getSsid();
    const char* getWifiPass();

    requestStatus validateUserCredentials(const String& body);

    requestStatus validateCropParameters(const String& body);

    // Valida credenciales (sin guardar nada) para desbloquear la edición.
    requestStatus authUserCredentials(const String& body);

    void manageDevice(int devicePin, int intervalHours, int durationMinutes, uint32_t epochHours);

    void turnOnDevices();

    // Serializa el estado actual del dispositivo a JSON para GET /getparams.
    // Recibe el token de la query (?token=) solo para reportar "sessionValid";
    // no renueva la sesión (la ventana es FIJA desde el login).
    String buildParamsJson(const String& token);

    // ---- Sesión de edición (token en RAM, 128 bits, TTL fijo) ----
    // Emite un token nuevo (login/registro OK) y fija su expiración. Sobrescribe
    // el anterior: solo hay UNA sesión activa, así que un login nuevo invalida la
    // sesión previa. La ventana es FIJA: la actividad NO la renueva.
    String issueSessionToken();
    // ¿El token coincide con el vigente y no ha expirado? No renueva.
    bool   isSessionValid(const String& token);
    // Invalida la sesión (logout /exit y factory reset).
    void   clearSession();

    // ---- Conectividad Wi-Fi del usuario (modo STA, coexiste con el AP) ----
    // El arranque del STA (AP+STA) se hace en setup() del .ino usando los getters
    // getWifiCredentials()/getSsid()/getWifiPass(); aquí solo quedan las
    // operaciones en runtime (escaneo, guardado y observación del intento).
    // Escanea redes y devuelve JSON {"networks":[{ssid,rssi,secure}]} con las 5
    // más fuertes SIN nombres repetidos (descarta duplicados de repetidores,
    // quedándose con la señal más fuerte). Bloquea ~2 s: corre dentro del request.
    String scanNetworks();
    // Valida (token de sesión + ssid/pass) y ARRANCA la conexión, sin bloquear ni
    // persistir todavía: devuelve STATUS_OK = "intento iniciado". El front confirma
    // el resultado por polling a /getparams (wifiConnected). Las credenciales se
    // guardan en NVS solo cuando la conexión tiene éxito (ver updateWifi()).
    requestStatus saveWifiCredentials(const String& body);
    // Llamada periódica desde loop(): si hay un intento pendiente y el STA quedó
    // conectado, persiste las credenciales y marca hasWifiCredentials. Es el único
    // punto donde se guardan, para no dejar en NVS una contraseña que no funciona.
    void updateWifi();
    // ¿El STA está asociado a la red del usuario? (para /getparams: wifiConnected)
    bool isWifiConnected();

    // Funciones para el control del RTC (DS3231)
    bool setCurrentTime();
    bool getCurrentTime();

    void hardReset();


    void printSystemData();
    //void readSystemStatus(byte* systemStatus, size_t length);

    // Edad del cultivo en días (>=1) DERIVADA del RTC y del ancla _cropStartDay.
    // NO se incrementa en medianoche: se calcula del calendario real, así que es
    // correcta tras reboots/cortes de luz. Devuelve 0 si el cultivo no se ha
    // anclado todavía (primer /newparams) o si el RTC retrocedió.
    int cropDayFromRtc();
    

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

    // Buffers dimensionados al peor caso UTF-8 (maxChars * 2 + 1) para que un
    // valor válido del formulario nunca se trunque al guardarse.
    char _plantName[maxPlantNameChars * utf8MaxBytesPerChar + 1];   // 41
    char _username[maxUsernameChars  * utf8MaxBytesPerChar + 1];    // 65
    char _userpass[maxUserpassChars  * utf8MaxBytesPerChar + 1];    // 129

    char _SSID[maxWifiSsidChars + 1];        // 33: SSID (32) + nul
    char _SSIDpass[maxWifiPassChars + 1];    // 64: passphrase (63) + nul
    char _MAC[18];

    // true entre saveWifiCredentials() y la confirmación de conexión: marca que
    // hay credenciales en RAM aún NO persistidas (se guardan al conectar OK).
    bool _wifiPending = false;

    // Sesión de edición: token de 128 bits en hex (32 chars + nul) y su
    // vencimiento medido con millis() (uptime). Vacío = sin sesión activa.
    char _sessionToken[33] = {0};
    uint32_t _sessionExpiresAt = 0;

    // Fecha de inicio del cultivo como nº de día continuo (daysSinceEpoch). Se
    // ancla una sola vez en el primer /newparams con fecha válida y se persiste
    // en NVS (namespace "system", clave "cropStart"). 0 = sin anclar. De aquí se
    // derivan cropDay/cropWeek sin contadores ni lógica de medianoche.
    uint32_t _cropStartDay = 0;

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

// Capa HTTP (acoplada a requestStatus): se queda aquí, NO en utils. El struct lo
// usa el .ino (variable global response) y el handler de cada ruta.
HttpResponse buildHttpResponse(requestStatus status);

#endif