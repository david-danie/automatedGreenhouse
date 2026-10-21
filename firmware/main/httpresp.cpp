// ===========================================================================
//  httpresp.cpp — Mapeo requestStatus -> respuesta HTTP. Ver httpresp.h.
//  Códigos y mensajes idénticos a buildHttpResponse del firmware Arduino.
// ===========================================================================
#include "httpresp.h"

namespace httpresp {

esp_err_t send(httpd_req_t* req, requestStatus status) {
    const char* code = "400 Bad Request";
    const char* body = "{\"status\":false,\"message\":\"Error.\"}";

    switch (status) {
        case STATUS_OK:
            code = "200 OK"; body = "{\"status\":true,\"message\":\"Parámetros actualizados correctamente.\"}"; break;
        case HARD_RESET:
            code = "200 OK"; body = "{\"status\":true,\"message\":\"Factory reset ejecutado.\"}"; break;
        case NEW_CROP_DONE:
            code = "200 OK"; body = "{\"status\":true,\"message\":\"Cultivo reiniciado. Configura los parámetros del nuevo cultivo para comenzar.\"}"; break;
        case RESET_REQUIRES_AP:
            code = "403 Forbidden"; body = "{\"status\":false,\"message\":\"El restablecimiento solo puede hacerse desde la red Wi-Fi del dispositivo. Conéctate a la red SmartPlant e inténtalo de nuevo.\"}"; break;
        case INVALID_JSON:
            body = "{\"status\":false,\"message\":\"El formato de envío es inválido.\"}"; break;
        case STORAGE_ERROR:
            body = "{\"status\":false,\"message\":\"Los datos recibidos no se pudieron guardar.\"}"; break;
        case MISSING_FIELDS:
            body = "{\"status\":false,\"message\":\"Campos requeridos faltantes.\"}"; break;
        case MISSING_CREDENTIALS:
            body = "{\"status\":false,\"message\":\"Los campos de usuario y contraseña son obligatorios.\"}"; break;
        case INVALID_USERNAME_LENGTH:
            body = "{\"status\":false,\"message\":\"Longitud de usuario inválida (4-32 caracteres).\"}"; break;
        case INVALID_USERPASS_LENGTH:
            body = "{\"status\":false,\"message\":\"Longitud de contraseña inválida (8-64 caracteres).\"}"; break;
        case INVALID_USERNAME_CHARS:
            body = "{\"status\":false,\"message\":\"El nombre de usuario solo permite los caracteres (_-.@!#$%&*?+=).\"}"; break;
        case INVALID_USERPASS_CHARS:
            body = "{\"status\":false,\"message\":\"La contraseña de usuario solo permite los caracteres (_-.@!#$%&*?+=).\"}"; break;
        case USERNAME_REPEATED_CHARS:
            body = "{\"status\":false,\"message\":\"El nombre de usuario tiene un caracter repetido más de 3 veces.\"}"; break;
        case USERPASS_REPEATED_CHARS:
            body = "{\"status\":false,\"message\":\"La contraseña de usuario tiene un caracter repetido más de 3 veces.\"}"; break;
        case MISMATCH_CREDENTIALS:
            body = "{\"status\":false,\"message\":\"Las credenciales enviadas no coinciden.\"}"; break;
        case TOO_MANY_ATTEMPTS:
            code = "429 Too Many Requests"; body = "{\"status\":false,\"message\":\"Demasiados intentos fallidos. Espera un momento antes de volver a intentarlo.\"}"; break;
        case INVALID_SESSION:
            code = "401 Unauthorized"; body = "{\"status\":false,\"message\":\"Tu sesión expiró. Vuelve a iniciar sesión.\"}"; break;
        case MISSING_PLANTNAME_FIELD:
            body = "{\"status\":false,\"message\":\"El campo planta es obligatorio.\"}"; break;
        case INVALID_PLANTNAME_LENGTH:
            body = "{\"status\":false,\"message\":\"Longitud de planta inválida (3-20 caracteres).\"}"; break;
        case INVALID_PLANTNAME_CHARS:
            body = "{\"status\":false,\"message\":\"EL nombre de la planta solo permite los caracteres (_-.@!#$%&*?+=).\"}"; break;
        case PLANTNAME_REPEATED_CHARS:
            body = "{\"status\":false,\"message\":\"EL nombre de la planta tiene un caracter repetido más de 3 veces.\"}"; break;
        case PLANTNAME_REPEATED_SPACES:
            body = "{\"status\":false,\"message\":\"El nombre de la planta no puede tener espacios consecutivos.\"}"; break;
        case PLANTNAME_ONLY_DIGITS:
            body = "{\"status\":false,\"message\":\"El nombre de la planta no puede ser solo números.\"}"; break;
        case INVALID_PHOTOPERIOD_TYPE:
            body = "{\"status\":false,\"message\":\"Valor de fotoperiodo inválido (solamente enteros).\"}"; break;
        case INVALID_IRRIGATION_TYPE:
            body = "{\"status\":false,\"message\":\"Valores de irrigación inválidos (frecuencia permitida y minutos 0-59).\"}"; break;
        case INVALID_VENTILATION_TYPE:
            body = "{\"status\":false,\"message\":\"Valores de ventilación inválidos (frecuencia permitida y minutos 0-59).\"}"; break;
        case INVALID_IRRIGATION_DURATION:
            body = "{\"status\":false,\"message\":\"Con el riego activo la duración debe ser de al menos 1 minuto. Para desactivarlo, elige la frecuencia 'Apagado'.\"}"; break;
        case INVALID_VENTILATION_DURATION:
            body = "{\"status\":false,\"message\":\"Con la ventilación activa la duración debe ser de al menos 1 minuto. Para desactivarla, elige la frecuencia 'Apagado'.\"}"; break;
        case INVALID_LED_VALUE:
            body = "{\"status\":false,\"message\":\"Los espectros azul y rojo deben estar entre 0 y 100%.\"}"; break;
        case INVALID_WHITE_LED_VALUE:
            body = "{\"status\":false,\"message\":\"La luz blanca solo acepta 0 (apagada) o 1 (encendida).\"}"; break;
        case INVALID_SECOND_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo segundo debe ser un entero sin signo (0-59).\"}"; break;
        case INVALID_MINUTE_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo minuto debe ser un entero sin signo (0-59).\"}"; break;
        case INVALID_HOUR_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo hora debe ser un entero sin signo (0-23).\"}"; break;
        case INVALID_WEEKDAY_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo dia de la semana debe ser un entero sin signo (1-7).\"}"; break;
        case INVALID_DAY_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo dia debe ser un entero sin signo (1-31).\"}"; break;
        case INVALID_MONTH_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo mes debe ser un entero sin signo (1-12).\"}"; break;
        case INVALID_YEAR_FORMAT:
            body = "{\"status\":false,\"message\":\"El campo año debe ser un entero sin signo (0-99).\"}"; break;
        case MISSING_WIFI_FIELDS:
            body = "{\"status\":false,\"message\":\"Falta la red Wi-Fi a configurar.\"}"; break;
        case INVALID_SSID:
            body = "{\"status\":false,\"message\":\"El nombre de la red (SSID) es inválido (1-32 caracteres).\"}"; break;
        case INVALID_WIFI_PASS:
            body = "{\"status\":false,\"message\":\"La contraseña Wi-Fi debe tener entre 8 y 63 caracteres.\"}"; break;
        default:
            break;
    }

    httpd_resp_set_status(req, code);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

} // namespace httpresp
