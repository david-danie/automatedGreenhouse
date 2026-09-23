#ifndef _PLANT
#define _PLANT

#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "Constants.h" 

class Plant {

  public:

    Plant();
    void begin();

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

    // ---- Cultivo nuevo (POST /newcrop) ----
    // Cierra el cultivo actual y deja el equipo listo para empezar otro. Es una
    // operación RUTINARIA (cada cosecha), así que EXIGE token de sesión, igual que
    // editar parámetros. Borra nombre de planta, ancla de días y todos los
    // parámetros; CONSERVA la cuenta de usuario y las credenciales Wi-Fi, que no
    // tienen nada que ver con qué se está cultivando.
    // No confundir con hardReset(): ese es el reset de FÁBRICA, excepcional, sin
    // credenciales (vía de recuperación si se olvida la contraseña) y restringido a
    // la interfaz del AP.
    requestStatus startNewCrop(const String& body);

    // Valida credenciales (sin guardar nada) para desbloquear la edición.
    // `fromAP` indica si la petición entró por la interfaz del AP; lo calcula el
    // .ino (que es quien conoce el objeto server) comparando la IP local del
    // socket con WiFi.softAPIP(). Solo se usa para el comando de reset: el reset
    // no pide credenciales —es la vía de recuperación si se olvida la contraseña—
    // así que su barrera es la cercanía física, y con el STA activo el endpoint
    // también responde desde la red del usuario.
    requestStatus authUserCredentials(const String& body, bool fromAP);

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
    // quedándose con la señal más fuerte). ASÍNCRONO: arranca el escaneo y
    // responde scanning=true; el front consulta hasta recibir la lista.
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

    // ¿La última lectura del RTC dio una hora fiable? False si el DS3231 no
    // respondió, devolvió valores fuera de rango o su bit OSF indica que el
    // oscilador se detuvo (hora perdida). turnOnDevices() no acciona nada
    // mientras sea false, y se reporta en /getparams como "rtcValid".
    bool isRtcValid();

    void hardReset();


    void printSystemData();

    // Edad del cultivo en días (>=1) DERIVADA del RTC y del ancla _cropStartDay.
    // NO se incrementa en medianoche: se calcula del calendario real, así que es
    // correcta tras reboots/cortes de luz. Devuelve 0 si el cultivo no se ha
    // anclado todavía (primer /newparams) o si el RTC retrocedió.
    int cropDayFromRtc();

  private:

    // ---- Límite de intentos de login (RAM, ver Constants.h) ----
    // ¿El login está bloqueado ahora mismo? Comparación a prueba del wrap de
    // millis(), igual que la expiración de la sesión.
    bool loginLocked();
    // Registra un fallo y, pasado el umbral, fija el bloqueo con espera creciente.
    void registerLoginFailure();
    // Fallos consecutivos y momento hasta el que se rechaza (0 = sin bloqueo).
    // Viven en RAM: un reinicio los borra, que es el trade-off aceptado.
    uint8_t  _failedLogins = 0;
    uint32_t _lockoutUntil = 0;

    // Apaga TODOS los actuadores (luces, bomba y ventilador). Existe como punto
    // ÚNICO para que los dos caminos que exigen "todo apagado" —sistema
    // desactivado por el usuario y RTC sin hora fiable— se comporten igual, y
    // para que al añadir un actuador nuevo no se olvide uno de los dos.
    void allDevicesOff();

    // ---- Bit OSF del DS3231 (registro 0x0F) ----
    // ¿El oscilador se detuvo alguna vez? (true = la hora guardada no es fiable).
    // Devuelve true también si la lectura I²C falla: ante la duda, no confiar.
    bool rtcLostPower();
    // Limpia el OSF tras fijar una hora buena, para que las lecturas siguientes
    // vuelvan a considerarse fiables.
    void rtcClearLostPower();

    // Dimensionados por los centinelas de los enums (Constants.h) en vez de por
    // un número fijo: si se añade un campo, el arreglo crece solo y no queda un
    // desfase silencioso entre el enum y el tamaño reservado.
    // OJO: _systemStatus se persiste en NVS con putBytes, así que cambiar
    // systemStatusCount cambia el tamaño del blob y hay que borrar la flash antes
    // de cargar el firmware nuevo (ver la nota en Constants.h).
    uint8_t _systemStatus[systemStatusCount] = {0};
    uint8_t _currentTime[currentTimeCount] = {0};

    // true si la última lectura del RTC (getCurrentTime) fue correcta Y contiene
    // una fecha/hora plausible. Si el DS3231 no responde o devuelve basura, se
    // pone en false y turnOnDevices() NO acciona actuadores (fail-safe): agendar
    // riego/luz con una hora inválida podría regar de más o dejar el cultivo a
    // oscuras. Arranca en false hasta la primera lectura válida.
    bool _rtcValid = false;

    // Buffers dimensionados al peor caso UTF-8 (maxChars * 2 + 1) para que un
    // valor válido del formulario nunca se trunque al guardarse.
    char _plantName[maxPlantNameChars * utf8MaxBytesPerChar + 1];   // 41
    char _username[maxUsernameChars  * utf8MaxBytesPerChar + 1];    // 65

    // La contraseña NO se guarda ni se conserva en RAM: solo su salt y la clave
    // derivada con PBKDF2-HMAC-SHA256. En el login se deriva de nuevo con este
    // salt y se compara contra _pwHash en tiempo constante.
    uint8_t _pwSalt[pwSaltBytes];
    uint8_t _pwHash[pwHashBytes];

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
    // derivan el día y la semana del cultivo sin contadores ni lógica de medianoche.
    uint32_t _cropStartDay = 0;

    // La versión del firmware NO es un miembro: es la constante de compilación
    // firmwareVersion (Constants.h). Un miembro con ese nombre la sombrearía
    // dentro de los métodos de la clase y /getparams reportaría una cadena vacía.

    // Nota para la integración con el backend: aquí vivía un miembro
    // `HTTPClient http;` que nunca se usó. Se quitó a propósito para arrancar
    // limpio; cuando se implementen las peticiones, conviene instanciar el
    // cliente LOCAL a la función que hace la llamada (no mantenerlo vivo en RAM
    // entre peticiones esporádicas) y añadir el include correspondiente.
    Preferences p;

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