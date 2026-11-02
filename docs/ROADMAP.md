# Roadmap — Migración del firmware a ESP-IDF

Plan de migración del firmware de **Arduino-ESP32** a **ESP-IDF** puro, por fases entregables y verificables en hardware por separado. La meta es profesionalizar el firmware (tooling, control de RAM/flash, OTA de doble slot y TLS), no aumentar la velocidad de CPU.

Para el diseño técnico a fondo, ver [ARCHITECTURE.md](ARCHITECTURE.md). `Constants.h` sigue siendo la fuente de verdad del hardware y los límites de validación.

> **Este documento es el plan** (qué se migra, cómo y cómo se verifica) y es común a ambas
> ramas. El **avance** (qué fases están hechas y qué falta probar en hardware) vive junto al
> código, en [`firmware/README.md`](https://github.com/david-danie/automatedGreenhouse/blob/esp-idf-migration/firmware/README.md)
> (solo en la rama `esp-idf-migration`).

---

## Motivación y expectativas honestas

Arduino-ESP32 **ya corre sobre ESP-IDF**: cuando el firmware llama `ledcAttachChannel`, `esp_random()`, `mbedtls_md_hmac` o `vTaskDelay`, ya está ejecutando código de IDF. La migración **no hace el firmware "más rápido"** —misma CPU (RISC-V @160 MHz), mismo compilador (`riscv32-esp-gcc`), y el cuello de botella real (servir el portal y handlers HTTP esporádicos) no está limitado por CPU.

Lo que sí se gana, y por qué se migra:

| Beneficio | De dónde sale |
|---|---|
| **Menos flash y RAM** | `menuconfig` recorta componentes que Arduino incluye por defecto; buffers de red, stacks por tarea y config de mbedTLS ajustables |
| **OTA de doble slot con rollback** | `esp_ota_ops` / `esp_https_ota` + `partitions.csv` con dos slots OTA, de primera clase en IDF |
| **TLS con pinning y firmware firmado** | Control fino de mbedTLS y `esp_https_*`; el firmware ya usa `mbedtls/md.h` directo |
| **HTTP sin picos de heap** | `esp_http_server` envía desde un puntero (p. ej. el portal en flash) sin copias intermedias; en Arduino se maneja con `send_P` |
| **Control de socket/interfaz** | `esp_wifi` + `esp_netif` + sockets lwIP reemplazan los workarounds de Arduino (bind del portal solo a la IP del AP) |

> El rendimiento del **backend** (Postgres/Timescale, índices, ingesta MQTT, pooling) es un tema separado que no depende de esta migración. Ver [BACKEND.md](BACKEND.md).

---

## Clasificación del código Arduino (punto de partida)

Inventario de los archivos del firmware Arduino (`ESP32_controller/`) según el esfuerzo de migración.

### Portable sin cambios de lógica (ya es IDF o es neutral)

| Elemento | Nota |
|---|---|
| `utils.cpp` / `utils.h` | Cripto (`mbedtls/md.h`, PBKDF2-HMAC-SHA256, comparación en tiempo constante, hex), BCD del RTC, validación UTF-8, aritmética de fechas. Solo cambia el tipo `String` en las firmas |
| Lógica de `Constants.h` | Enums, límites de validación, `validFrequencies`. Solo cambian `HIGH`/`LOW`, `IPAddress` y el `#include <Arduino.h>` |
| Lógica de negocio de `Plant.cpp` | Scheduling stateless, derivación de día/semana desde el RTC, validación, manejo del bit OSF del DS3231, sesión con token. **Es el valor real del proyecto y no depende de Arduino** |
| `ArduinoJson` (v6) | Librería JSON header-only (no es el framework Arduino); funciona en IDF puro como componente; **no se reescribe** |

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

## Estrategia: reescritura a ESP-IDF puro, por hitos

El firmware se reimplementa directamente en **ESP-IDF nativo**, módulo por módulo, en la
carpeta `firmware/`. El firmware Arduino de [`ESP32_controller/`](../ESP32_controller) se
conserva como **especificación de referencia** (contrato de la API, reglas de validación,
scheduling stateless, manejo del bit OSF del RTC, sesión con token): cada hito reimplementa un
módulo consultando ese original y se verifica en hardware antes de pasar al siguiente.

**Objetivo de paridad:** al terminar, el firmware ESP-IDF debe comportarse igual que el
Arduino frente al portal y al usuario (mismo contrato de [API.md](API.md), mismas reglas de
seguridad y validación). Las diferencias deliberadas se documentan.

El trabajo vive en la rama `esp-idf-migration`; `main` conserva el firmware Arduino funcional
como respaldo flasheable. La lógica de negocio (`Plant`, `utils`) es portable casi sin cambios
—no depende de Arduino—; el esfuerzo real está en la capa de framework (Wi-Fi, HTTP, NVS, I²C,
PWM) y en sustituir `String`.

---

## Fases

### Fase 0 — Esqueleto ESP-IDF que arranca

**Objetivo:** levantar un proyecto ESP-IDF puro mínimo que compile, arranque y registre info
del sistema por el log. Establece el tooling (`idf.py`, `menuconfig`, particiones) antes de
portar lógica.

- Estructura de proyecto IDF: `firmware/CMakeLists.txt`, `firmware/main/CMakeLists.txt`, `firmware/sdkconfig.defaults`
- `app_main()` mínimo que arranca y loguea chip/flash/heap + latido
- `partitions.csv` con **dos slots OTA** (`ota_0`, `ota_1`) + `otadata` + `nvs` (4 MB)
- Compilar con `idf.py set-target esp32c3 && idf.py build`
- Flashear con `idf.py erase-flash && idf.py flash monitor` y confirmar el arranque
- Recorte inicial de componentes en `menuconfig`

**Verificación:** el firmware compila, arranca y el monitor muestra la info del sistema y el
latido. (Primer flasheo con `erase-flash` para limpiar otadata residual de layouts previos.)

**Criticidad de `partitions.csv`:** OTA de doble slot exige dos particiones de app del mismo
tamaño. Fijar esto ahora evita rehacer el layout más adelante. Ver [ARCHITECTURE.md § requisito de particiones](ARCHITECTURE.md#requisito-crítico-tabla-de-particiones-con-dos-slots-ota).

> `firmware/partitions.csv` (4 MB): `nvs` 20 KB, `otadata`, `phy_init`, y dos slots OTA de
> ~1.9 MB (`ota_0` en `0x20000`). El offset exacto de la primera app se confirma con el primer
> `idf.py build` en hardware.

---

### Fase 1 — Periféricos a IDF nativo

**Objetivo:** migrar los periféricos de bajo acoplamiento y la lógica de control. No toca la capa HTTP.

- NVS: `Preferences` → API `nvs` (módulo `storage`). Esquema **compacto**: un solo
  namespace `smartplant`, config del cultivo en un blob versionado (`CropConfig`,
  `schemaVersion`), credenciales de usuario y Wi-Fi aparte por su distinto ciclo de vida.
- I²C DS3231: `Wire` → `driver/i2c_master.h` (módulo `rtc`; lectura de 7 bytes, enmascarado, registro OSF `0x0F`). Bus en GPIO 8 (SDA) / 9 (SCL).
- PWM/GPIO: `ledcAttachChannel` → `driver/ledc.h`; `pinMode`/`digitalWrite` → `driver/gpio.h` (módulo `actuators`; estado inicial seguro).
- Lógica de control (módulo `control`): fotoperiodo con cruce de medianoche, scheduling stateless (`epochHours % intervalo`), fail-safe por RTC e interruptor general.
- **Frecuencias de riego/ventilación a `uint16_t`**: admiten intervalos > 255 h, p. ej. **336 h = cada 2 semanas** en `validFrequencies` (la opción debe exponerse también en el portal).

**Verificación:** lógica portable (BCD, fechas, UTF-8, escalado PWM, fotoperiodo y scheduling incl. 336 h) validada con tests en el host; luces, bomba, ventilador, RTC y persistencia probados en hardware tras `idf.py build`/flash.

> El esquema NVS se reescribe desde cero (no se reutiliza el layout `_systemStatus` por índice del firmware Arduino). El blob `CropConfig` lleva `schemaVersion` para evolucionar sin corromper datos previos.

---

### Fase 2 — Wi-Fi a `esp_wifi` / `esp_netif`

**Objetivo:** reemplazar la abstracción de `WiFi` y recuperar el control fino de interfaz/socket.

- AP+STA con `esp_wifi` + `esp_netif` + event loop (`WIFI_EVENT` / `IP_EVENT`)
- SoftAP WPA2-PSK (credenciales en `sensible.h`); STA condicional si hay credenciales en NVS
- Escaneo asíncrono (5 más fuertes, sin SSID repetidos)
- Persistencia diferida de credenciales (solo al llegar a `WL_CONNECTED` equivalente)
- Desactivar modem-sleep para que el AP responda estable en AP+STA

**Verificación:** el escaneo devuelve la lista; conectar a la Wi-Fi del usuario y confirmar por polling; las credenciales inválidas no se persisten.

---

### Fase 3 — HTTP a `esp_http_server` + DNS captivo propio

**Objetivo:** el trabajo grande. Sincronizar con la homologación del **Portal V2**, porque esa capa se toca de todas formas.

- `esp_http_server`: los 11 endpoints del contrato de [API.md](API.md) (`/`, `/usercredentials`, `/getparams`, `/newparams`, `/newcrop`, `/authusercredentials`, `/wifiscan`, `/wificredentials`, `/exit`, `/otaupdate` y el catch-all del portal captivo)
- Servir el portal **directo desde flash**, sin copiarlo al heap, con **gzip**
  (`Content-Encoding: gzip`) — el mismo artefacto `mainForm.h` que genera `gen_mainform.py`
  para el firmware Arduino.
- **El portal se sirve solo por la interfaz del AP** (paridad con `WebServer server(apGatewayIp, 80)`
  de Arduino): ningún endpoint debe responder por la interfaz STA. Ver
  [ARCHITECTURE.md](ARCHITECTURE.md) (*El portal se sirve SOLO por el AP*).
- Catch-all del portal captivo: cualquier ruta desconocida → `302` a `/` (como `onNotFound`).
- Autorización del reset y del OTA por interfaz de AP (comparar la IP local del socket con la del SoftAP)
- Servidor DNS captivo propio (responde la IP del AP a todas las consultas)
- Endpoint **`POST /otaupdate`** (subir `.bin` por el AP) usando `esp_ota_ops`

**Verificación:** todos los endpoints responden igual que en Arduino (contrato de [API.md](API.md)); el portal carga completo en AP+STA sin fallos de heap; desde la red STA no responde ningún endpoint; el reset solo se acepta por el AP; un OTA local completa y arranca el binario nuevo.

---

### Fase 4 — Cierre: sin dependencias de Arduino

**Objetivo:** confirmar que no queda nada del framework Arduino y medir el resultado.

Al ser una reescritura en IDF puro (no un port con `arduino-esp32` como componente), el código
nuevo nace con `std::string` / `char[]` en vez de `String`; esta fase queda como verificación
de cierre.

- Ninguna referencia a `String`, `Arduino.h` ni al componente `arduino-esp32` en `firmware/`
  (`ArduinoJson` sí se conserva: es una librería JSON, no el framework)
- Recorte final de componentes en `menuconfig`
- Comparar flash/RAM contra el firmware Arduino

**Verificación:** compila sin `arduino-esp32`; suite de pruebas en hardware completa; tabla de flash/RAM Arduino vs IDF.

---

### Fase 5 — OTA segura sobre TLS

**Objetivo:** cerrar la seguridad del canal de actualización. Depende de las fases anteriores.

- `esp_https_ota` con **CA pinning**
- **Firmware firmado** (secure boot / verificación de firma de imagen)
- **Rollback automático si la imagen nueva no arranca** — en el firmware Arduino NO existe: el
  OTA local reinicia directo al binario nuevo sin "marcado de app válida", así que un `.bin`
  estructuralmente válido pero no arrancable puede brickear el equipo (ver
  [ARCHITECTURE.md § OTA](ARCHITECTURE.md#endpoint-post-otaupdate)). Requiere
  `esp_ota_mark_app_valid_cancel_rollback()` + `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`
  (de primera clase en IDF).
- Integración con el endpoint OTA del backend (ver [BACKEND.md](BACKEND.md))

> **Mitigación de corto plazo (no requiere IDF):** verificar el **hash** del binario en el
> OTA local — el cliente envía el SHA/MD5 y el firmware lo fija con `Update.setMD5(...)`;
> `Update.end()` falla si no coincide. Restringe a binarios aprobados y atrapa corruptos, sin
> esperar a la firma criptográfica. No sustituye al rollback, pero reduce el riesgo de subir
> un binario equivocado.

**Verificación:** un OTA con imagen firmada válida completa y persiste; una imagen no firmada o con CA incorrecta se rechaza; el rollback restaura el slot anterior ante fallo de arranque.

---

## Orden recomendado y criterios de avance

1. **Fase 0 primero** — valida el tooling (`idf.py`, particiones, flasheo) con un esqueleto que arranca, antes de portar nada crítico.
2. Fases 1 y 2 a ritmo propio; son cambios acotados y verificables.
3. **Fase 3 sincronizada con el Portal V2** — no duplicar el trabajo de la capa de servido.
4. Fases 4 y 5 al final.

**Regla de avance:** no empezar una fase hasta que la anterior esté verificada en hardware y medida (flash/RAM cuando aplique). Cada fase debe dejar el dispositivo en un estado funcional y flasheable.

---

## Estado

El avance por fase se lleva en
[`firmware/README.md` § Estado de la migración](https://github.com/david-danie/automatedGreenhouse/blob/esp-idf-migration/firmware/README.md#estado-de-la-migración)
(rama `esp-idf-migration`), para que cada avance del firmware IDF no obligue a tocar la
documentación común.
