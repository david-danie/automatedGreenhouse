// ===========================================================================
//  httpserver.cpp — Servidor HTTP del portal (esp_http_server). Ver httpserver.h.
//  Sub-paso 4.1: sirve el portal gzip en '/' y responde el portal captivo.
// ===========================================================================
#include "httpserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mainForm.h"      // mainForm_gz[], mainForm_gz_len
#include <ArduinoJson.h>
#include <string>
#include <cstring>
#include "Constants.h"
#include "storage.h"
#include "rtc.h"
#include "control.h"
#include "session.h"
#include "wifi.h"
#include "auth.h"
#include "cropparams.h"
#include "httpresp.h"
#include "actuators.h"
#include "ota.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "http";
static httpd_handle_t s_server = nullptr;

// GET '/' — sirve el portal COMPRIMIDO con gzip. El navegador lo descomprime
// solo (Content-Encoding: gzip). Igual que handleRoot del firmware Arduino.
static esp_err_t rootHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char*)mainForm_gz, mainForm_gz_len);
}

// Catch-all para la detección de portal captivo: cualquier ruta desconocida
// redirige a '/', igual que onNotFound del firmware Arduino.
static esp_err_t captiveHandler(httpd_req_t* req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// Flags de CropConfig.flags (ver storage.h).
static const uint8_t FLAG_HAS_USER = 0x01;   // bit0
static const uint8_t FLAG_SYS_EN   = 0x04;   // bit2

// Lee el valor de ?token= de la query (para reportar sessionValid).
static std::string queryToken(httpd_req_t* req) {
    size_t len = httpd_req_get_url_query_len(req) + 1;
    if (len <= 1) return "";
    std::string q(len, '\0');
    if (httpd_req_get_url_query_str(req, &q[0], len) != ESP_OK) return "";
    char val[40] = {0};
    if (httpd_query_key_value(q.c_str(), "token", val, sizeof(val)) != ESP_OK) return "";
    return std::string(val);
}

// Serializa el estado del dispositivo (GET /getparams). Port 1:1 del
// buildParamsJson del firmware Arduino, con ArduinoJson v6.
static std::string buildParamsJson(const std::string& token) {
    CropConfig cfg;
    storage::loadConfig(cfg);

    uint8_t t[currentTimeCount] = {0};
    bool rtcValid = rtc::getCurrentTime(t);

    char plant[maxPlantNameChars * utf8MaxBytesPerChar + 1] = {0};
    storage::loadPlantName(plant, sizeof(plant));

    bool hasUser = (cfg.flags & FLAG_HAS_USER) != 0;

    StaticJsonDocument<768> doc;
    doc["hasRegisteredUser"] = hasUser;
    doc["firmwareVersion"]   = firmwareVersion;
    doc["rtcValid"]          = rtcValid;
    doc["sessionValid"]      = session::isValid(token);

    bool wifiUp = wifimod::isStaConnected();
    doc["wifiConnected"] = wifiUp;
    doc["wifiSsid"]      = "";   // el SSID asociado se añadirá al exponer esa lectura

    if (hasUser) {
        doc["planta"]    = plant;
        doc["enable"]    = (cfg.flags & FLAG_SYS_EN) != 0;
        doc["fpOn"]      = cfg.photoperiodOn;
        doc["fpOff"]     = cfg.photoperiodOff;
        doc["ledAzul"]   = cfg.blueDutyCycle;
        doc["ledRojo"]   = cfg.redDutyCycle;
        doc["ledBlanco"] = cfg.whiteLedOn;
        doc["irrH"]      = cfg.irrigationFrequency;
        doc["irrM"]      = cfg.irrigationDuration;
        doc["ventH"]     = cfg.ventilationFrequency;
        doc["ventM"]     = cfg.ventilationDuration;
        int cd = control::cropDayFromRtc(cfg, t, rtcValid);
        doc["dia"]    = cd;
        doc["semana"] = cd > 0 ? (cd - 1) / 7 + 1 : 0;
    }

    std::string out;
    serializeJson(doc, out);
    return out;
}

// GET /getparams — estado del dispositivo en JSON.
static esp_err_t getParamsHandler(httpd_req_t* req) {
    std::string json = buildParamsJson(queryToken(req));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

// GET /wifiscan — escaneo asíncrono de redes (lógica en wifimod).
static esp_err_t wifiScanHandler(httpd_req_t* req) {
    std::string json = wifimod::scanNetworks();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

// Lee el cuerpo del request a un std::string (límite defensivo de tamaño).
static bool readBody(httpd_req_t* req, std::string& out) {
    int total = req->content_len;
    if (total <= 0 || total > 2048) return false;   // payloads del portal son pequeños
    out.resize(total);
    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, &out[received], total - received);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

// ¿La petición entró por la interfaz del AP? Se compara la IP local del socket
// con la IP del SoftAP (192.168.4.1). No es un dato que el cliente pueda
// falsificar (igual que el fromAP del firmware Arduino).
static bool fromAP(httpd_req_t* req) {
    int fd = httpd_req_to_sockfd(req);
    if (fd < 0) return false;
    sockaddr_in local = {};
    socklen_t len = sizeof(local);
    if (getsockname(fd, (sockaddr*)&local, &len) != 0) return false;
    return local.sin_addr.s_addr == htonl(0xC0A80401);   // 192.168.4.1
}

// Emite respuesta de login/registro OK con el token de sesión.
static esp_err_t sendAuthOk(httpd_req_t* req, const char* msg) {
    std::string token = session::issueToken();
    std::string body = std::string("{\"status\":true,\"message\":\"") + msg +
                       "\",\"token\":\"" + token + "\"}";
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body.c_str(), body.size());
}

// POST /usercredentials — registro. En éxito emite token.
static esp_err_t userCredHandler(httpd_req_t* req) {
    std::string body;
    if (!readBody(req, body)) return httpresp::send(req, INVALID_JSON);
    requestStatus st = auth::registerUser(body);
    if (st == STATUS_OK) return sendAuthOk(req, "Usuario registrado.");
    return httpresp::send(req, st);
}

// POST /authusercredentials — login (o reset con "**reset**").
static esp_err_t authHandler(httpd_req_t* req) {
    std::string body;
    if (!readBody(req, body)) return httpresp::send(req, INVALID_JSON);
    requestStatus st = auth::loginUser(body, fromAP(req));
    if (st == STATUS_OK) return sendAuthOk(req, "Acceso concedido.");
    esp_err_t r = httpresp::send(req, st);
    if (st == HARD_RESET) {                 // reset: responde y reinicia
        actuators::setBuzzer(true); vTaskDelay(pdMS_TO_TICKS(600)); actuators::setBuzzer(false);
        vTaskDelay(pdMS_TO_TICKS(400));
        esp_restart();
    } else if (st == MISMATCH_CREDENTIALS || st == TOO_MANY_ATTEMPTS) {
        // (patrón de error sonoro no bloqueante se gestiona en el bucle de control
        //  en una futura mejora; aquí basta la respuesta HTTP)
    }
    return r;
}

// POST /newparams — valida y guarda parámetros. En éxito, 3 pitidos.
static esp_err_t newParamsHandler(httpd_req_t* req) {
    std::string body;
    if (!readBody(req, body)) return httpresp::send(req, INVALID_JSON);
    requestStatus st = cropparams::validateAndSave(body);
    esp_err_t r = httpresp::send(req, st);
    return r;
}

// POST /wificredentials — valida y arranca la conexión STA.
static esp_err_t wifiCredHandler(httpd_req_t* req) {
    std::string body;
    if (!readBody(req, body)) return httpresp::send(req, INVALID_JSON);
    return httpresp::send(req, wifimod::saveCredentials(body));
}

// POST /newcrop — cierra el cultivo actual.
static esp_err_t newCropHandler(httpd_req_t* req) {
    std::string body;
    if (!readBody(req, body)) return httpresp::send(req, INVALID_JSON);
    return httpresp::send(req, cropparams::startNewCrop(body));
}

// /exit — logout: invalida la sesión.
static esp_err_t exitHandler(httpd_req_t* req) {
    session::clear();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req,
        "{\"status\":true,\"message\":\"Desconectado correctamente. Ya puedes cerrar esta ventana y desconectarte de la red SmartPlant.\"}",
        HTTPD_RESP_USE_STRLEN);
}

// POST /otaupdate — sube el .bin y lo escribe en el slot OTA inactivo.
// Autoriza ANTES de escribir un solo byte: token de sesión (header
// Authorization: Bearer) + petición por la interfaz del AP. Fail-closed.
static esp_err_t otaUpdateHandler(httpd_req_t* req) {
    // Token del header Authorization: Bearer <token>.
    char auth[64] = {0};
    httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth));
    const char* tok = (strncmp(auth, "Bearer ", 7) == 0) ? auth + 7 : "";
    if (!session::isValid(std::string(tok))) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req,
            "{\"status\":false,\"message\":\"Tu sesión expiró. Inicia sesión y reintenta.\"}",
            HTTPD_RESP_USE_STRLEN);
    }
    if (!fromAP(req)) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req,
            "{\"status\":false,\"message\":\"La actualización solo se permite desde la red del equipo (AP).\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    if (!ota::begin()) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req,
            "{\"status\":false,\"message\":\"No se pudo iniciar la actualización.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    // Escribe el cuerpo (el binario) por trozos.
    char chunk[1024];
    int remaining = req->content_len;
    bool ok = true;
    while (remaining > 0) {
        int r = httpd_req_recv(req, chunk, (remaining < (int)sizeof(chunk)) ? remaining : (int)sizeof(chunk));
        if (r <= 0) { ok = false; break; }
        if (!ota::write((const uint8_t*)chunk, r)) { ok = false; break; }
        remaining -= r;
    }

    if (!ok || !ota::finish()) {
        ota::abort();
        actuators::setBuzzer(true); vTaskDelay(pdMS_TO_TICKS(150)); actuators::setBuzzer(false);
        vTaskDelay(pdMS_TO_TICKS(150));
        actuators::setBuzzer(true); vTaskDelay(pdMS_TO_TICKS(150)); actuators::setBuzzer(false);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req,
            "{\"status\":false,\"message\":\"No se pudo instalar el firmware. El equipo conserva el anterior.\"}",
            HTTPD_RESP_USE_STRLEN);
    }

    // Éxito: responde, pita (1 largo) y reinicia al firmware nuevo.
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
        "{\"status\":true,\"message\":\"Firmware instalado. El equipo se reinicia.\"}",
        HTTPD_RESP_USE_STRLEN);
    actuators::setBuzzer(true); vTaskDelay(pdMS_TO_TICKS(600)); actuators::setBuzzer(false);
    vTaskDelay(pdMS_TO_TICKS(400));
    esp_restart();
    return ESP_OK;   // no se alcanza
}

namespace httpserver {

bool begin() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;   // permite el catch-all '/*'
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 16;                      // 11 rutas registradas (> 8 por defecto)

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start falló");
        return false;
    }

    httpd_uri_t root = { "/", HTTP_GET, rootHandler, nullptr };
    httpd_register_uri_handler(s_server, &root);

    httpd_uri_t getparams = { "/getparams", HTTP_GET, getParamsHandler, nullptr };
    httpd_register_uri_handler(s_server, &getparams);

    httpd_uri_t wifiscan = { "/wifiscan", HTTP_GET, wifiScanHandler, nullptr };
    httpd_register_uri_handler(s_server, &wifiscan);

    httpd_uri_t ucred = { "/usercredentials", HTTP_POST, userCredHandler, nullptr };
    httpd_register_uri_handler(s_server, &ucred);
    httpd_uri_t aauth = { "/authusercredentials", HTTP_POST, authHandler, nullptr };
    httpd_register_uri_handler(s_server, &aauth);
    httpd_uri_t nparams = { "/newparams", HTTP_POST, newParamsHandler, nullptr };
    httpd_register_uri_handler(s_server, &nparams);
    httpd_uri_t wcred = { "/wificredentials", HTTP_POST, wifiCredHandler, nullptr };
    httpd_register_uri_handler(s_server, &wcred);
    httpd_uri_t ncrop = { "/newcrop", HTTP_POST, newCropHandler, nullptr };
    httpd_register_uri_handler(s_server, &ncrop);
    httpd_uri_t exit = { "/exit", HTTP_GET, exitHandler, nullptr };
    httpd_register_uri_handler(s_server, &exit);

    httpd_uri_t ota = { "/otaupdate", HTTP_POST, otaUpdateHandler, nullptr };
    httpd_register_uri_handler(s_server, &ota);

    httpd_uri_t any = { "/*", HTTP_GET, captiveHandler, nullptr };
    httpd_register_uri_handler(s_server, &any);

    ESP_LOGI(TAG, "Servidor HTTP iniciado (portal en /)");
    return true;
}

} // namespace httpserver
