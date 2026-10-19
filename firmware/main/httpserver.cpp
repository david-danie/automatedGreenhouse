// ===========================================================================
//  httpserver.cpp — Servidor HTTP del portal (esp_http_server). Ver httpserver.h.
//  Sub-paso 4.1: sirve el portal gzip en '/' y responde el portal captivo.
// ===========================================================================
#include "httpserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mainForm.h"      // mainForm_gz[], mainForm_gz_len

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

namespace httpserver {

bool begin() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;   // permite el catch-all '/*'
    cfg.lru_purge_enable = true;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start falló");
        return false;
    }

    httpd_uri_t root = { "/", HTTP_GET, rootHandler, nullptr };
    httpd_register_uri_handler(s_server, &root);

    httpd_uri_t any = { "/*", HTTP_GET, captiveHandler, nullptr };
    httpd_register_uri_handler(s_server, &any);

    ESP_LOGI(TAG, "Servidor HTTP iniciado (portal en /)");
    return true;
}

} // namespace httpserver
