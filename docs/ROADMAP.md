# Roadmap — Migración del firmware a ESP-IDF

Plan de migración del firmware de **Arduino-ESP32** a **ESP-IDF** puro, por fases entregables y verificables en hardware por separado. La meta es profesionalizar el firmware (tooling, control de RAM/flash, OTA de doble slot y TLS), no aumentar la velocidad de CPU.

Para el diseño técnico a fondo, ver [ARCHITECTURE.md](ARCHITECTURE.md). `Constants.h` sigue siendo la fuente de verdad del hardware y los límites de validación.

---

## Motivación y expectativas honestas

Arduino-ESP32 **ya corre sobre ESP-IDF**: cuando el firmware llama `ledcAttachChannel`, `esp_random()`, `mbedtls_md_hmac` o `vTaskDelay`, ya está ejecutando código de IDF. La migración **no hace el firmware "más rápido"** —misma CPU (RISC-V @160 MHz), mismo compilador (`riscv32-esp-gcc`), y el cuello de botella real (servir ~73 KB de HTML y handlers HTTP esporádicos) no está limitado por CPU.

Lo que sí se gana, y por qué se migra:

| Beneficio | De dónde sale |
|---|---|
| **Menos flash y RAM** | `menuconfig` recorta componentes que Arduino incluye por defecto; buffers de red, stacks por tarea y config de mbedTLS ajustables |
| **OTA de doble slot con rollback** | `esp_ota_ops` / `esp_https_ota` + `partitions.csv` con dos slots OTA, de primera clase en IDF |
| **TLS con pinning y firmware firmado** | Control fino de mbedTLS y `esp_https_*`; el firmware ya usa `mbedtls/md.h` directo |
| **HTTP por chunks / gzip nativo** | `esp_http_server` resuelve por diseño el pico de heap que hoy se maneja con `send_P` |
| **Control de socket/interfaz** | `esp_wifi` + `esp_netif` reemplazan los workarounds actuales (bind del portal solo a la IP del AP) |

> El rendimiento del **backend** (Postgres/Timescale, índices, ingesta MQTT, pooling) es un tema separado que no depende de esta migración. Ver [BACKEND.md](BACKEND.md).

---

## Clasificación del código actual

Inventario de los 4 archivos del firmware según el esfuerzo de migración.

### Portable sin cambios de lógica (ya es IDF o es neutral)

| Elemento | Nota |
|---|---|
| `utils.cpp` / `utils.h` | Cripto (`mbedtls/md.h`, PBKDF2-HMAC-SHA256, comparación en tiempo constante, hex), BCD del RTC, validación UTF-8, aritmética de fechas. Solo cambia el tipo `String` en las firmas |
| Lógica de `Constants.h` | Enums, límites de validación, `validFrequencies`. Solo cambian `HIGH`/`LOW`, `IPAddress` y el `#include <Arduino.h>` |
| Lógica de negocio de `Plant.cpp` | Scheduling stateless, derivación de día/semana desde el RTC, validación, manejo del bit OSF del DS3231, sesión con token. **Es el valor real del proyecto y no depende de Arduino** |
| `ArduinoJson` (v6) | Funciona en IDF puro como componente; **no se reescribe** |

### Requiere reescritura (la capa de framework)

| API actual (Arduino) | Destino (ESP-IDF) | Esfuerzo |
|---|---|---|
| `Preferences` | `nvs_flash` / API `nvs` | Bajo (casi 1:1) |
| `Wire` (I²C DS3231) | `driver/i2c_master.h` | Bajo |
| `ledcAttachChannel`, `pinMode`, `digitalWrite` | `driver/ledc.h`, `driver/gpio.h` | Bajo |
| `WiFi.*` (AP+STA, scan, eventos) | `esp_wifi` + `esp_netif` + event loop | Medio |
| `DNSServer` | Servidor DNS captivo propio (~80 líneas, patrón conocido) | Medio |
| `WebServer` (10 handlers) | `esp_http_server` | Alto |
| `String` | `std::string` / `char[]` en toda la superficie | Medio (mecánico pero extenso) |

---

## Estrategia: incremental, no big-bang

Arduino-ESP32 puede compilarse **como componente dentro de un proyecto ESP-IDF**. Eso permite adoptar el tooling profesional (`idf.py`, `menuconfig`, `partitions.csv`, OTA, TLS) **sin reescribir todavía** `WebServer`/`String`. Cada fase queda funcional y verificable en hardware por separado; no hay un periodo largo con el portal roto.

---

## Fases

### Fase 0 — Build ESP-IDF con Arduino como componente

**Objetivo:** convertir el proyecto en un proyecto ESP-IDF sin reescribir lógica. Es el paso que desbloquea OTA y TLS con el mínimo riesgo.

- [ ] Estructura de proyecto IDF: `CMakeLists.txt` raíz, `main/CMakeLists.txt`, `sdkconfig.defaults`
- [ ] Integrar `arduino-esp32` como componente (managed component o submódulo)
- [ ] Mover el sketch a `main/` y adaptar `setup()`/`loop()` al `app_main()` (o `initArduino()` + task)
- [ ] `partitions.csv` con **dos slots OTA** (`ota_0`, `ota_1`) + `otadata` + `nvs`
- [ ] Compilar con `idf.py build` y flashear con `idf.py flash`
- [ ] Recorte inicial de componentes en `menuconfig`

**Verificación:** el portal arranca y funciona idéntico al build de Arduino (registro, login, edición, Wi-Fi, RTC). Medir flash/RAM antes y después.

**Criticidad de `partitions.csv`:** OTA de doble slot exige dos particiones de app del mismo tamaño. Fijar esto ahora evita rehacer el layout más adelante. Ver [ARCHITECTURE.md § requisito de particiones](ARCHITECTURE.md#requisito-crítico-tabla-de-particiones-con-dos-slots-ota).

---

### Fase 1 — Periféricos a IDF nativo

**Objetivo:** migrar los periféricos de bajo acoplamiento. No toca la capa HTTP.

- [ ] NVS: `Preferences` → API `nvs` (namespaces `system`, `plantData`, `config`, `wifi`)
- [ ] I²C DS3231: `Wire` → `driver/i2c_master.h` (lectura de 7 bytes, registro OSF `0x0F`)
- [ ] PWM/GPIO: `ledcAttachChannel` → `driver/ledc.h`; `pinMode`/`digitalWrite` → `driver/gpio.h`

**Verificación:** luces (PWM azul/rojo + blanco ON/OFF), bomba y ventilador responden igual; la hora del RTC se lee y el modo seguro por OSF sigue activo; la config persiste tras reinicio.

> Al cambiar el esquema de persistencia, respetar la nota de `Constants.h`: `_systemStatus` se guarda por índice; borrar la flash antes de cargar el firmware nuevo si cambia el layout.

---

### Fase 2 — Wi-Fi a `esp_wifi` / `esp_netif`

**Objetivo:** reemplazar la abstracción de `WiFi` y recuperar el control fino de interfaz/socket.

- [ ] AP+STA con `esp_wifi` + `esp_netif` + event loop (`WIFI_EVENT` / `IP_EVENT`)
- [ ] SoftAP WPA2-PSK (credenciales en `sensible.h`); STA condicional si hay credenciales en NVS
- [ ] Escaneo asíncrono (5 más fuertes, sin SSID repetidos)
- [ ] Persistencia diferida de credenciales (solo al llegar a `WL_CONNECTED` equivalente)
- [ ] Desactivar modem-sleep para que el AP responda estable en AP+STA

**Verificación:** el portal responde solo por la interfaz del AP (no por STA); el escaneo devuelve la lista; conectar a la Wi-Fi del usuario y confirmar por polling; las credenciales inválidas no se persisten.

---

### Fase 3 — HTTP a `esp_http_server` + DNS captivo propio

**Objetivo:** el trabajo grande. Sincronizar con la homologación del **Portal V2**, porque esa capa se toca de todas formas.

- [ ] `esp_http_server`: los 10 endpoints (`/`, `/usercredentials`, `/getparams`, `/newparams`, `/newcrop`, `/authusercredentials`, `/wifiscan`, `/wificredentials`, `/exit`, `onNotFound`)
- [ ] Servir el portal por **chunks** desde flash (`httpd_resp_send_chunk`) — resuelve el pico de heap por diseño
- [ ] **gzip** (`Content-Encoding: gzip`) para el portal (pendiente histórico del roadmap)
- [ ] Autorización del reset por interfaz de AP (equivalente a comparar la IP local del socket con la del SoftAP)
- [ ] Servidor DNS captivo propio (responde la IP del AP a todas las consultas)
- [ ] Endpoint **`POST /otaupdate`** (subir `.bin` por el AP) usando `esp_ota_ops`

**Verificación:** todos los endpoints responden igual que hoy (contrato de [API.md](API.md)); el portal carga completo en AP+STA sin fallos de heap; el reset solo se acepta por el AP; un OTA local completa y arranca el binario nuevo.

---

### Fase 4 — Eliminar `String` (IDF puro)

**Objetivo:** cierre. Quitar la última dependencia de Arduino.

- [ ] `String` → `std::string` / `char[]` en handlers, `buildParamsJson`, `scanNetworks`, `HttpResponse`
- [ ] Firmas de `utils.*` sin `String`
- [ ] Quitar el componente `arduino-esp32` del build
- [ ] Recorte final de componentes en `menuconfig`

**Verificación:** compila sin `arduino-esp32`; suite de pruebas en hardware completa; comparar flash/RAM contra la Fase 0.

---

### Fase 5 — OTA segura sobre TLS

**Objetivo:** cerrar la seguridad del canal de actualización. Depende de las fases anteriores.

- [ ] `esp_https_ota` con **CA pinning**
- [ ] **Firmware firmado** (secure boot / verificación de firma de imagen)
- [ ] Rollback automático si la imagen nueva no valida el arranque
- [ ] Integración con el endpoint OTA del backend (ver [BACKEND.md](BACKEND.md))

**Verificación:** un OTA con imagen firmada válida completa y persiste; una imagen no firmada o con CA incorrecta se rechaza; el rollback restaura el slot anterior ante fallo de arranque.

---

## Orden recomendado y criterios de avance

1. **Fase 0 primero** — desbloquea OTA/TLS con el mínimo riesgo y valida el tooling antes de reescribir nada crítico.
2. Fases 1 y 2 a ritmo propio; son cambios acotados y verificables.
3. **Fase 3 sincronizada con el Portal V2** — no duplicar el trabajo de la capa de servido.
4. Fases 4 y 5 al final.

**Regla de avance:** no empezar una fase hasta que la anterior esté verificada en hardware y medida (flash/RAM cuando aplique). Cada fase debe dejar el dispositivo en un estado funcional y flasheable.

---

## Estado

| Fase | Descripción | Estado |
|---|---|---|
| 0 | Build IDF + Arduino como componente + `partitions.csv` doble slot | No iniciada |
| 1 | Periféricos a IDF nativo (NVS, I²C, LEDC/GPIO) | No iniciada |
| 2 | Wi-Fi a `esp_wifi` / `esp_netif` | No iniciada |
| 3 | HTTP a `esp_http_server` + DNS captivo + OTA local | No iniciada |
| 4 | Eliminar `String` (IDF puro) | No iniciada |
| 5 | OTA segura sobre TLS (pinning + firma) | No iniciada |
