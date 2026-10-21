#ifndef _HTTPRESP
#define _HTTPRESP

// ===========================================================================
//  httpresp.h — Mapeo requestStatus -> respuesta HTTP. Port de buildHttpResponse.
// ===========================================================================
#include "esp_http_server.h"
#include "Constants.h"   // requestStatus

namespace httpresp {

// Envía la respuesta JSON correspondiente al requestStatus (código + mensaje),
// con los mismos textos y códigos que el firmware Arduino.
esp_err_t send(httpd_req_t* req, requestStatus status);

} // namespace httpresp

#endif
