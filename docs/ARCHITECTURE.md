# SmartPlant — Controlador ESP32 (firmware + portal captivo)

Firmware para un **ESP32-C3** que controla un cultivo (iluminación LED por espectro,
riego y ventilación) y se configura desde el celular/PC vía un **portal captivo
Wi-Fi**. El dispositivo levanta un Access Point, sirve un formulario HTML embebido y
recibe la configuración por HTTP en JSON.

Este directorio es la variante **"HTML sin comprimir"**: contiene tanto el firmware
(`ESP32_controller/`) como una copia legible de los formularios (`HTML/`) para editar
cómodamente antes de embeberlos.

---

## Estructura

```
ESP32_controller/
  ESP32_controller.ino     # Punto de entrada: AP Wi-Fi, DNS captivo, rutas HTTP y handlers
  Plant.h / Plant.cpp # Lógica del cultivo: validación de payloads, persistencia, PWM/relés, RTC, sesión
  Constants.h         # Pines, canales PWM, límites de validación, TTL de sesión, enums de estado y de error
  sensible.h          # *** Secretos del AP (apSsid / apPassword) — NO versionar, copiar por equipo ***
  mainForm.h          # *** Artefacto GENERADO (string C R"===(...)===") sin comentarios que sirve el ESP32 — no editar a mano ***
  ci.json             # Config de build (FQBN / placa)
HTML/
  mainForm.html       # *** Fuente de verdad: HTML legible y comentado del formulario — edita AQUÍ ***
```

### Convención importante (leer antes de tocar formularios)
- **Edita siempre `HTML/mainForm.html`** (legible y comentado). Es la fuente de verdad.
- **`ESP32_controller/mainForm.h` es un artefacto generado**: el mismo HTML pero **sin
  comentarios** (los comentarios viven solo en el `.html` de respaldo) y envuelto en el
  raw-string C. Es lo que el dispositivo sirve; **no se edita a mano**.
- Quitar los comentarios del `.h` no es cosmético: reduce ~17 KB los bytes que el AP
  manda en cada carga del portal (~73 KB → ~56 KB), sin tocar la lógica.
- Generar el `.h` desde el `.html`: strip de comentarios HTML (`<!-- -->`), CSS (`/* */`)
  y JS (`//`) + colapso de líneas en blanco + wrapper `static const char mainForm[] = R"===(` … `)===";`.
  Tras generar, vale la pena un `node --check` sobre el `<script>` para confirmar que el
  strip no rompió sintaxis. Para verificar que ambos archivos coinciden: regenera a un
  temporal y haz `diff` contra el `.h` del repo (deben salir idénticos).

---

## Hardware (ESP32-C3, ver `Constants.h`)
- LED blanco → GPIO 0 (salida digital, lógica invertida), LED azul → GPIO 1 (canal 1), LED rojo → GPIO 2 (canal 2)
- Buzzer → GPIO 3, ventilador → GPIO 7, bomba de agua → GPIO 10
- PWM: 1 kHz, 8 bits (duty 0–255). Los espectros se envían como 0–100 % y se escalan internamente.
- RTC externo **DS3231** por I²C (dirección `0x68`); la hora se sincroniza desde el navegador en `/newparams`.

---

## Arranque y red
Toda la configuración de red vive en `setup()` (`ESP32_controller.ino`), no en `Plant`:

1. `planta.begin()` carga estado y credenciales desde **Preferences** (NVS), namespaces `config` y `system`.
2. Se fija el modo dual **explícito** `WiFi.mode(WIFI_AP_STA)` y se crea el AP Wi-Fi
   **`SmartPlant`** con **WPA2-PSK** vía `WiFi.AP.create(apSsid, apPassword)` **seguido de**
   `WiFi.AP.begin()` (SSID y passphrase viven en `sensible.h`), más DHCP + portal captivo.
   El enlace va **cifrado**: las credenciales del usuario y el token de sesión ya no viajan
   en claro por el aire. (`WiFi.AP.create()` selecciona WPA2 al recibir una passphrase de ≥ 8 chars.)
3. `WiFi.setSleep(false)` **desactiva el modem-sleep**: con el STA habilitado, el ESP32
   duerme la radio según el ciclo del STA (`WIFI_PS_MIN_MODEM` por defecto) y el SoftAP deja
   de responder — el portal no cargaría. Sin sleep el AP responde estable en **AP+STA**.
4. STA condicional: si `planta.getWifiCredentials()` es true (y hay SSID), `setup()` llama
   `WiFi.begin(getSsid(), getWifiPass())` → queda **AP+STA**; sin credenciales, **AP puro**.
5. `DNSServer` responde la IP del AP a todo dominio (detección de portal captivo).
6. El `WebServer` (puerto 80) registra las rutas y entra al loop `handleClient()`.
7. Cualquier ruta desconocida hace `302` a `/` (`handleNotFound`).

### Secretos del AP (`sensible.h`)
SSID y contraseña del Access Point se aíslan en `ESP32_controller/sensible.h` (`apSsid`,
`apPassword`) para mantenerlos fuera del resto del código. El archivo está marcado como
**NO versionar**: cópialo desde una plantilla y ajústalo por equipo antes de flashear.
WPA2 exige una passphrase de **8–63 caracteres**. El valor por defecto
(`"SmartPlant2026"`) es de desarrollo; en producción usa una **contraseña por dispositivo**
(p. ej. derivada de la MAC o un secreto de fábrica, impresa en una etiqueta/QR del equipo)
para que solo quien tiene el dispositivo físico pueda unirse.

---

## Flujo de la aplicación (página única)

Todo vive en **un solo HTML** (`HTML/mainForm.html`, servido como `mainForm.h`). No hay página de registro aparte;
el formulario es una máquina de estados que decide qué mostrar según el dispositivo.

Al cargar, el `#dashboard` arranca **oculto** (`display:none`): el cliente hace
`GET /getparams` (adjuntando `?token=` si guardó uno) y, según la respuesta, revela directo
el panel correcto (sin parpadear datos antes de tiempo ni transiciones falsas). El JSON
espeja las llaves del formulario (`planta`, `fpOn`, `ledA`…), así que el front las usa tal
cual (objeto `params`, sin traducir nombres). Si `/getparams` **falla** (transporte), se
pinta un `mensaje-error` rojo y se reintenta recargando. La respuesta incluye
`hasRegisteredUser` y `sessionValid`:

- **`hasRegisteredUser = false`** (dispositivo nuevo) → estado **`welcome`**: pantalla de
  bienvenida que avisa que el dispositivo es nuevo e invita a **crear usuario** o **salir**.
  "Crear usuario" pasa al estado **`register`**, que reusa el bloque de credenciales como
  alta (`POST /usercredentials` guarda usuario+contraseña sin reiniciar, **devuelve un token
  de sesión** y va directo al dashboard ya autenticado); "Volver" regresa a la bienvenida.
- **`hasRegisteredUser = true`** → estado **`view`**: dashboard de solo lectura. El botón
  "Editar parámetros" lleva al estado **`auth`** (login → `POST /authusercredentials`), y
  si las credenciales son válidas, **el backend emite un token** y se pasa al estado
  **`edit`** (formulario editable → `POST /newparams`). Si `/getparams` ya respondió
  `sessionValid = true` (token vigente en `localStorage`), el botón **salta `auth`** y entra
  directo a `edit`. Al guardar, el dispositivo **aplica los parámetros en vivo, sin
  reiniciar** (ver "Cómo se aplican los parámetros").

Estados del front: `welcome` · `register` · `view` · `auth` · `edit` · `wifi`.
El estado **`wifi`** (config de red) se entra desde un **chip discreto en el
dashboard** y está gateado por sesión igual que `edit` (ver "Conexión Wi-Fi del
usuario").

### Sesión de edición (token, ver "Endpoints" y `Constants.h`)
La autenticación es por **token de sesión en RAM**, no por carga de página:

- `POST /authusercredentials` (login) y `POST /usercredentials` (registro) devuelven, al
  validar OK, un `token` (128 bits hex, `esp_random()`) en el JSON. El front lo guarda en
  `localStorage` (`spToken`).
- El token tiene **TTL fijo de 30 min** (`SESSION_TTL_MS`): la ventana se cuenta desde el
  login y la actividad **no** la renueva, así que la sesión caduca 30 min después de iniciar
  sesión (haya o no actividad) y hay que volver a autenticarse.
- `POST /newparams` ya **no reenvía las credenciales en claro**: exige el `token` en el body
  y lo valida (`isSessionValid`). Token ausente/inválido/expirado → `INVALID_SESSION`
  (HTTP `401`); el front limpia `localStorage` y cae al login.
- Solo hay **una** sesión activa: un login nuevo sobrescribe el token anterior.
- El token vive **solo en RAM** (sin NVS, sin RTC): un corte de luz, crash, `**reset**` o
  el logout `GET /exit` (`clearSession()`) lo invalidan. Como muere con el reboot, la
  expiración se mide con `millis()` (uptime), evitando depender del RTC.

**Caveat de seguridad:** el token añade UX y cierra el hueco de reenviar credenciales en
claro a `/newparams`, pero la confidencialidad real la aporta el **WPA2 del AP** — sin él,
cualquiera en la red podría capturar el token (viaja sin TLS dentro del enlace cifrado).

### Reset de fábrica
En la pantalla de **login** (`auth`), escribir como contraseña la palabra mágica
**`**reset**`** dispara `hardReset()` en el backend (borra los namespaces `config` y
`system` de Preferences) y reinicia el ESP32. Se intercepta en `authUserCredentials()`
**antes** de comparar credenciales, así que funciona aunque se haya olvidado la
contraseña. El registro **no** acepta reset (no hay nada previo que borrar).

### Cómo se aplican los parámetros (sin reboot)
`/newparams` **ya no reinicia** el ESP32: los parámetros se aplican en vivo. Al final de
`validateCropParameters()` (`Plant.cpp`) el flujo es:

1. La config validada se vuelca en `_systemStatus[]` (RAM) — la **fuente de verdad** que
   usa el dispositivo en marcha — y el nombre de planta en `_plantName`.
2. Se persiste a NVS (namespaces `system` y `plantData`) para sobrevivir cortes de luz.
3. Se fija la hora del navegador en el RTC (`setCurrentTime()`).
4. `turnOnDevices()` aplica la config **al instante** (PWM de LEDs según fotoperiodo + duty,
   relés de bomba/ventilador según frecuencia/duración).

Luego el `loop()`, cada `deviceUpdateInterval`, refresca `_currentTime` desde el RTC
(`getCurrentTime()`) y re-aplica `turnOnDevices()`. **Todo el I²C ocurre en el `loop()`
(loopTask)** — donde también corren los handlers HTTP — para no compartir el bus `Wire`
entre tareas; `printTask` **solo imprime** la copia en RAM de `_currentTime` (ya no toca el
RTC). Tras un corte de luz, `begin()` recarga `_systemStatus` desde NVS y el loop lo
re-aplica en segundos. El reboot anterior era **redundante** para aplicar la config; solo
`**reset**` reinicia ahora.

---

## Endpoints HTTP
Ver el detalle de payloads en **`HTML/test/rutas_y_parametros.txt`**.

| Método | Ruta                  | Handler                  | Propósito |
|--------|-----------------------|--------------------------|-----------|
| GET    | `/`                   | `handleRoot`             | Sirve el HTML único (`mainForm`) vía `send_P` (directo desde flash, sin copiarlo a un `String` de ~72 KB en cada request) |
| GET    | `/getparams`          | `handleGetParameters`    | Estado del dispositivo en JSON (incluye `hasRegisteredUser`; con `?token=`, `sessionValid`; y siempre `wifiConnected`/`wifiSsid`) |
| POST   | `/usercredentials`    | `handleUserCredentials`  | Alta de usuario (primer arranque). No reinicia; **devuelve `token`** de sesión |
| POST   | `/authusercredentials`| `handleAuthUserCredentials` | Login para desbloquear edición; **devuelve `token`**; intercepta `**reset**` |
| POST   | `/newparams`          | `handleNewParameters`    | Guarda y **aplica en vivo** los parámetros del cultivo + hora (sin reiniciar). **Exige `token`** vigente |
| GET    | `/wifiscan`           | `handleWifiScan`         | Escanea redes y devuelve `{networks:[{ssid,rssi,secure}]}` (top-5 sin nombres repetidos). Sin sobre `{status,message}` |
| POST   | `/wificredentials`    | `handleWifiCredentials`  | Recibe `{ssid,pass,token}`, valida y **arranca** la conexión STA (no bloquea). **Exige `token`** vigente |
| GET    | `/exit`               | `handleExit`             | Cierra sesión (`clearSession()`): JSON `{status, message}` que el front pinta en la tarjeta (no navega a otra página) |
| *      | (cualquier otra)      | `handleNotFound`         | `302 → /` (portal captivo) |

Las rutas de **acción** (`/usercredentials`, `/authusercredentials`, `/newparams`,
`/exit`) responden JSON `{"status": <bool>, "message": "<texto>"}` — `200` en éxito,
`400` en error de validación, `401` cuando el token de sesión falta/expiró
(`INVALID_SESSION`) (mapeo estado→mensaje en `buildHttpResponse()`, `Plant.cpp`).
En éxito de login/registro el sobre lleva además un campo `token`
(`{"status":true,"message":...,"token":"<hex 32>"}`). El front las pinta con
`pintarMensaje(ok, texto)`: verde (`mensaje-ok`) o roja (`mensaje-error`) según
`res.ok && json.status`.

**Excepción:** `/getparams` **no** usa ese sobre — devuelve los parámetros directos
(siempre `200`, sin `status`/`message`). Su único fallo posible es de transporte; en ese
caso el front pinta también un `mensaje-error` rojo (no inventa un dashboard vacío).

---

## Reglas de validación (deben coincidir front ↔ firmware)
Definidas en `Constants.h` y replicadas en JS dentro de `HTML/mainForm.html`
(`isValidReadableString`, `hasTooManyRepeatedChars`, etc.).

> El `<form>` lleva `novalidate`: toda la validación la hace el JS (más completa y con
> mensajes claros), no la nativa de HTML5. Es **necesario** porque es un único `<form>`
> con varios pasos que se ocultan; la validación nativa bloquearía el envío en silencio
> al toparse con campos `required` ocultos y vacíos (p. ej. usuario/contraseña tras el
> registro). Los `required`/`min`/`max` quedan solo como ayuda visual.

- **Usuario:** 4–32 caracteres. **Contraseña:** 8–64. **Nombre de planta:** 3–20.
- **Charset permitido:** letras (incl. acentos y ñ/Ñ), dígitos y `_-.@!#$%&*?+=`.
  El nombre de planta admite espacios; usuario/contraseña no.
- No se permiten **4+ caracteres idénticos consecutivos**.
- Nombre de planta: sin espacios dobles y no puede ser solo dígitos.
- Longitudes contadas por **carácter UTF-8** (`utf8Len`), no por bytes.

---

## Compilar y flashear
- IDE: Arduino IDE / arduino-cli. Placa: ESP32-C3 (ver `ci.json`).
- Dependencias: core **ESP32 (WiFi/WebServer/DNSServer)**, **ArduinoJson** (v6, API
  `StaticJsonDocument`), `Wire`, `Preferences`.
- Tras flashear, conectarse al Wi-Fi `SmartPlant`; el portal captivo abre el formulario.
- **No hay test runner.** La validación se hace en hardware / a mano.

---

## Frecuencias de riego/ventilación (`irrH` / `ventH`)
Los `<select>` envían el **intervalo en horas** entre activaciones (no "veces/día" ni un
índice). Los valores replican `validFrequencies` de `Constants.h`
(`{0,1,2,3,4,6,8,12,24,48,72,168}`): `3` = cada 3 h (8 veces/día), `24` = diario, `48` =
cada 2 días, `168` = semanal, `0` = apagado. Una **sola lista** cubre desde riego
frecuente hasta espaciado.

`manageDevice()` (`Plant.cpp`) decide el encendido con un **contador continuo de horas**
derivado del RTC (`epochHours = daysSinceEpoch(...) * 24 + hora`): como no se reinicia a
medianoche, el mismo módulo sirve para intervalos sub-diarios y multi-día, y es
**stateless** (todo se recalcula del reloj, sobrevive cortes de luz sin derivar). Por eso
los valores **ya no tienen que dividir 24**.

El front se alinea al firmware: la constante JS `VALID_FREQUENCIES` en `HTML/mainForm.html`
debe mantenerse igual a `validFrequencies` si esta cambia. **Tope 255** (`uint8_t`): un
intervalo > ~10 días exigiría ampliar `validFrequencies` y `_systemStatus` a `uint16_t`.

> **Compatibilidad NVS:** el significado de `irrH`/`ventH` cambió (antes "veces/día", ahora
> "horas de intervalo"). Un dispositivo ya configurado con la versión anterior reinterpreta
> su valor guardado con la nueva semántica (p. ej. `8` pasa de "8 veces/día" a "cada 8 h");
> basta re-seleccionar la frecuencia una vez en el formulario para corregirlo.

## Edad del cultivo (`dia` / `semana`)
El dashboard muestra la **edad del cultivo** en días y semanas (`dia` / `semana` en
`/getparams`, solo lectura — no hay input de usuario). **No se incrementan con un contador
a medianoche**: se **derivan** del calendario real del RTC, igual que el riego. Así el
cultivo "sigue envejeciendo" aunque el equipo haya estado apagado (un contador nocturno
perdería esos días y, además, escribiría NVS cada noche).

- **Anclaje:** en el **primer `/newparams`** con fecha válida, el firmware guarda
  `cropStart = daysSinceEpoch(hoy)` en NVS (namespace `system`, clave `cropStart`). Se
  escribe **una sola vez** (guard `_cropStartDay == 0`), así editar parámetros después **no**
  reinicia la edad ni desgasta flash. El **factory reset** borra el ancla y el siguiente
  `/newparams` re-arranca en día 1.
- **Cálculo** (`Plant::cropDayFromRtc`): `dia = daysSinceEpoch(hoy) − cropStart + 1` (el día
  del ancla es el día 1); `semana = (dia − 1) / 7 + 1`. Antes de anclar (o si el RTC va hacia
  atrás) ambos valen `0`.
- Reusa `daysSinceEpoch()` del control de riego; `cropDay`/`cropWeek` siguen en el enum de
  `Constants.h` **solo** para no romper el layout NVS de `_systemStatus` (ya no se almacenan).

## Propuesta: servir el HTML comprimido (gzip) para mayor performance

Hoy el HTML se embebe como texto (`mainForm.h`, ~56 KB tras quitarle los comentarios).
Comprimirlo con gzip suele reducirlo a ~12–18 KB, lo que significa **menos flash
ocupado**, menos chunks por el AP y carga más rápida del portal. Los navegadores
descomprimen gzip de forma transparente; solo hay que declarar el encabezado
`Content-Encoding: gzip`. Esto está **pendiente de implementar** (este directorio es la
variante "sin comprimir"); se documenta aquí la vía recomendada.

### Flujo propuesto
1. **Editar siempre el HTML sin comprimir** (`mainForm.h` / `HTML/mainForm.html`) como
   fuente de verdad. El `.gz` es un artefacto generado, nunca se edita a mano.
2. **Comprimir** el HTML:
   ```bash
   gzip -9 -c HTML/mainForm.html > mainForm.html.gz
   ```
3. **Convertir a arreglo de bytes** en un header (PROGMEM):
   ```bash
   xxd -i mainForm.html.gz > ESP32_controller/mainForm_gz.h
   ```
   Genera algo como `unsigned char mainForm_html_gz[] = {...};` y
   `unsigned int mainForm_html_gz_len = NNNN;`. Conviene marcarlo `PROGMEM` y, si se quiere,
   renombrar el símbolo a `mainForm_gz`.
4. **Servir con el encabezado de codificación** en `handleRoot` (`ESP32_controller.ino`).
   Como es binario (no una cadena terminada en nulo), debe enviarse con longitud explícita
   vía `send_P`:
   ```cpp
   void handleRoot() {
     server.sendHeader("Content-Encoding", "gzip");
     server.send_P(200, "text/html", (const char*)mainForm_gz, mainForm_gz_len);
   }
   ```

### Consideraciones
- **Regenerar el `.gz` en cada cambio de UI**, idealmente como paso de build (script o
  target), para que no quede desincronizado del HTML fuente. Es el mismo riesgo de
  sincronía que ya existe entre `mainForm.h` y `HTML/mainForm.html`.
- Mantener `mainForm.h` (crudo) o sustituirlo por el `_gz.h` es decisión de tamaño vs.
  conveniencia de depurar; lo habitual es **dejar solo el `.gz` en producción** y conservar
  el crudo para desarrollo.
- No afecta a las rutas POST (JSON) ni a `/getparams`: solo cambia cómo se entrega el HTML.
- `send_P` requiere longitud explícita porque el contenido gzip contiene bytes nulos.

## Sesión persistente con token (implementada)

**Objetivo:** que un usuario ya autenticado no tenga que volver a ingresar sus credenciales
cada vez que va a editar los parámetros del cultivo. Resumen del flujo en
"Sesión de edición" (arriba); aquí van los detalles de implementación.

### Diseño: token en RAM + `millis()` (sin NVS, sin RTC)
Como `/newparams` **ya no reinicia** el dispositivo (ver "Cómo se aplican los parámetros"),
el token **no se persiste en NVS**: vive en RAM. Solo se pierde en un corte de luz, crash o
`**reset**` (factory reset) — casos en los que re-autenticar es aceptable o deseado. Como el
token muere con el reboot de todos modos, la expiración se mide con `millis()` (uptime),
evitando depender del RTC DS3231 y su caso borde de "hora no seteada".

**Firmware (`Plant`, RAM, sin NVS, sin RTC):**
- 2 miembros: `char _sessionToken[33]` (128 bits en hex = 32 chars + nul) y
  `uint32_t _sessionExpiresAt`. Vacío (`_sessionToken[0]=='\0'`) = sin sesión activa.
- `issueSessionToken()`: genera el token con `esp_random()` (RNG por hardware, 4×`%08x`) y
  fija `_sessionExpiresAt = millis() + SESSION_TTL_MS`. Es el **único** punto donde se fija la
  expiración: la ventana es **fija** (no se renueva con la actividad). **Sobrescribe** el
  token anterior (solo hay UNA sesión). Se llama al validar login OK en `authUserCredentials()`
  **y** en el registro (`validateUserCredentials`).
- `isSessionValid(token)`: hay sesión activa, el token coincide (longitud 32) **y** no
  expiró (`(int32_t)(millis() - _sessionExpiresAt) < 0`, a prueba de wrap-around de `millis()`).
  No renueva.
- `clearSession()`: invalida la sesión; se llama en `/exit` (logout) y dentro de `hardReset()`.
- `/authusercredentials` y `/usercredentials`: al validar OK, devuelven el token en el JSON.
- `/getparams`: recibe el token por query (`?token=`) y agrega `sessionValid` a la respuesta;
  **no** renueva la sesión. El header-based `server.collectHeaders()` no se usa; el token va
  en la query porque `/getparams` es GET.
- `/newparams`: valida el **token** del body (`INVALID_SESSION` → `401`) en vez de reenviar
  las credenciales.
- `**reset**`, un corte de luz y `/exit` invalidan el token solos (nada que limpiar en NVS).

**Front (`mainForm.h` / `mainForm.html`):**
- Guarda el token en `localStorage` (clave `spToken`) al loguear/registrar (`setToken`).
- En la carga lo manda en `/getparams` y lee `sessionValid`; si es válido, el botón
  "Editar parámetros" **salta el estado `auth`** y entra directo a `edit`.
- Si el backend responde inválido/`401` (p. ej. tras un corte de luz), limpia `localStorage`
  (`clearToken`) y cae al login. El botón Salir hace logout (`/exit` + `clearToken`).

### Caveats de seguridad
- El AP `SmartPlant` ahora usa **WPA2-PSK** (`sensible.h`), así que el enlace va cifrado;
  aun así **no hay TLS** dentro del enlace. El token añade UX y cierra el hueco de reenviar
  credenciales en claro a `/newparams`, pero la confidencialidad real la aporta el WPA2: con
  una passphrase compartida y conocida, alguien en la misma red podría capturar el token. El
  modelo de amenaza efectivo es la **cercanía física** + conocer la passphrase del AP (de ahí
  la recomendación de contraseña por dispositivo en `sensible.h`).
- El token se genera solo en login/registro (no por request), evitando escrituras innecesarias.

## Conexión Wi-Fi del usuario (modo STA) — implementada

Permite que el dispositivo se conecte a la red Wi-Fi del usuario (modo estación,
STA) **sin dejar de servir el portal**: cuando hay credenciales, corre en
**AP+STA** simultáneo. Esta sesión implementa **solo la conectividad**; la
comunicación con un backend (telemetría, OTA, cuentas) es trabajo futuro y se
diseña en las dos secciones siguientes.

### Arranque: AP vs AP+STA (condicional)
`setup()` **siempre** crea el AP (el portal local es el plano de control base) y
decide el STA **en línea** (la config de red vive toda en el `.ino`, no en `Plant`),
consultando los getters `planta.getWifiCredentials()` / `getSsid()` / `getWifiPass()`:

- **Con credenciales** (`getWifiCredentials()` true y SSID no vacío) → `setup()` hace
  `WiFi.begin(getSsid(), getWifiPass())`, que añade el STA sobre el AP: queda **AP+STA**
  y conecta a la red guardada. No bloquea; `setAutoReconnect(true)` mantiene el enlace
  tras cortes.
- **Sin credenciales** → se queda como **AP puro** (caso "básico"/sin configurar).

Las credenciales viven en su propio namespace NVS **`wifi`** (`ssid`/`pass`); el
flag `hasWifiCredentials` (en el namespace `system`, dentro de `_systemStatus`, expuesto
vía `getWifiCredentials()`) decide si se levanta el STA al arrancar. Las operaciones de
runtime (escaneo, guardado de credenciales, observación del intento) sí siguen en `Plant`
(`scanNetworks()`, `saveWifiCredentials()`, `updateWifi()`).

### Vista de configuración (estado `wifi` del front)
Se entra desde un **chip discreto en el dashboard** (punto verde/gris + "Conectado
a X" / "Conexión a Internet"). Cambiar de red es sensible, así que **exige sesión
vigente** igual que editar parámetros (usa `authIntent` para volver a la vista
correcta tras el login). Flujo:

1. Al entrar, el dispositivo **escanea on-demand** (`GET /wifiscan`) → spinner.
2. `scanNetworks()` devuelve las **5 redes más fuertes SIN nombres repetidos**
   (ante repetidores con el mismo SSID conserva el de mayor RSSI; oculta SSIDs
   vacíos). El front pinta un `<select>` con candado (seguridad) y barras (RSSI).
3. El usuario elige red + contraseña (el campo de contraseña **se oculta** en
   redes abiertas) y pulsa **Conectar** → `POST /wificredentials {ssid,pass,token}`.

### Conexión no bloqueante + polling
`saveWifiCredentials()` valida (token + ssid/pass) y **solo arranca** el intento
(`WiFi.begin()`), respondiendo `STATUS_OK` = "intento iniciado". **No bloquea**:
esperar la conexión dentro del handler congelaría `handleClient()` (el AP dejaría
de responder varios segundos). El front **confirma por polling** a `/getparams`
(`wifiConnected`) cada 1.5 s hasta ~15 s: éxito → toast verde y de vuelta al
dashboard; timeout → toast rojo para reintentar.

### Persistencia diferida (solo si conecta)
`updateWifi()` (llamado desde `loop()`, throttled) es el **único** punto que
guarda las credenciales: solo cuando un intento pendiente (`_wifiPending`) llega a
`WL_CONNECTED` se persiste en NVS y se marca `hasWifiCredentials`. Así una
**contraseña incorrecta nunca queda guardada** (el dispositivo no entra en un
bucle de reintentos fallidos tras reiniciar).

### Caveats
- **Radio única (ESP32-C3): AP y STA comparten canal.** Al asociar el STA con el
  router, el AP **migra al canal del router** y los clientes del portal (el
  celular) pueden **caerse un instante** justo al conectar. Reasocian solos y el
  polling se reanuda; por eso el polling tolera fallos de transporte intermedios.
- **El escaneo es bloqueante (~2 s)** y corre dentro del request `/wifiscan`:
  aceptable para una acción puntual del usuario.
- **Sin tráfico saliente todavía:** el firmware no abre ninguna conexión a un
  backend. Por construcción, un dispositivo **no hace peticiones inútiles**.

---

## Modelado de la base de datos (backend futuro)

Diseño de referencia para cuando se agregue el backend Python. Persiste la
configuración/estado de cada dispositivo para que **otros dispositivos o apps de
la misma cuenta** puedan consultarlos. Modelo **por cuenta**: una cuenta posee sus
dispositivos y sus datos.

### Identidad y autenticación (resumen)
- El dispositivo se autentica **una vez** con `user + pass + mac` sobre **TLS**.
- "Pro" **no es un nivel almacenado**: significa simplemente *"el dispositivo está
  vinculado a una cuenta válida"*. La presencia de un **token de dispositivo**
  (emitido por el backend, revocable, guardado en NVS) ES el "soy pro".
- La **MAC vincula** (identifica), el **user/pass autentica**, el **token** se usa
  en todo lo demás (no se reenvía la contraseña). Ver detalles de TLS abajo.

### Entidades

```
accounts ───1:N─── devices ───1:N─── device_configs   (config actual + histórico)
                      │
                      └──────1:N─── device_telemetry   (serie temporal de estado)

firmware_releases    (catálogo de binarios para OTA, independiente)
```

| Tabla | Campos clave | Notas |
|-------|--------------|-------|
| `accounts` | `id`, `email`/`username`, `password_hash`, `created_at` | Cuenta dueña de los dispositivos. Hash con bcrypt/argon2 |
| `devices` | `id`, `mac` (UNIQUE), `account_id` (FK), `name`, `token_hash`, `firmware_version`, `last_seen_at`, `created_at` | La MAC identifica; `token_hash` = token de dispositivo hasheado (revocable) |
| `device_configs` | `id`, `device_id` (FK), `planta`, `enable`, `fp_on`, `fp_off`, `led_a/r/b`, `irr_h/m`, `vent_h/m`, `crop_start_day`, `applied_at` | **Una fila por cambio** (histórico). La última = config vigente. Espeja las llaves de `/newparams` |
| `device_telemetry` | `id`, `device_id` (FK), `ts`, `wifi_rssi`, `uptime`, *(sensores futuros: temp, humedad…)* | **Serie temporal**: candidato a hypertable de **TimescaleDB** |
| `firmware_releases` | `version`, `url`, `sha256`, `signature`, `min_version`, `published_at` | Catálogo para OTA (ver sección TLS / OTA) |

### Flujos de información

1. **Aprovisionamiento** (acción del usuario): device → `POST /devices/provision
   {user,pass,mac}` → el backend valida la cuenta, crea/vincula la fila en
   `devices` y **emite el token**. Es el único momento en que viaja la contraseña.
2. **Reporte de config**: al cambiar parámetros, device → `POST
   /devices/{id}/config {token, …}` → inserta en `device_configs`.
3. **Telemetría**: device → `POST /devices/{id}/telemetry {token, …}` → inserta en
   `device_telemetry`. Cadencia por evento o intervalo (no polling de "¿soy pro?").
4. **Consulta por otros dispositivos/app**: `GET /devices/{id}/state` (última
   config + telemetría reciente), `GET /accounts/me/devices`. Todo gateado por el
   token y restringido a la cuenta dueña.
5. **OTA**: device → `GET /firmware/latest?current=X` → metadatos del release →
   descarga del binario firmado (ver sección TLS).

### Cómo NO se hacen peticiones inútiles
- **Sin Wi-Fi → cero tráfico** (ni puede alcanzar el backend).
- **Sin token y sin acción del usuario → silencio total.** No hay polling de
  entitlement en background; el vínculo se decide al aprovisionar (acción humana).
- **Token revocado (`401/403`) → el dispositivo borra el token y vuelve a "básico"**
  (negative caching): deja de llamar hasta que el usuario re-aprovisione.

---

## TLS en el ESP32-C3 (consideraciones de memoria/velocidad)

El backend exige **HTTPS/TLS**: sin él, `user+pass+mac` y el token viajarían en
claro por Internet. Pero el C3 tiene **RAM y CPU limitadas** (RISC-V 1 núcleo
@160 MHz, ~400 KB SRAM con heap libre mucho menor cuando ya corren AP+STA + web
server), así que TLS hay que dimensionarlo con cuidado.

### Lo que juega a favor
- **mbedTLS** viene en el core ESP32 (lo usa `WiFiClientSecure`).
- El **C3 tiene aceleradores por hardware**: AES, SHA, RSA **y ECC (P-256)**. Es
  decir, los suites modernos **ECDHE-ECDSA/RSA-AES-GCM** (TLS 1.2/1.3) corren
  acelerados; un handshake toma del orden de **cientos de ms**, viable.

### Los costos a controlar
1. **RAM del handshake (lo más caro).** Cada conexión TLS reserva buffers de
   récord (por defecto ~16 KB rx + 16 KB tx). Con AP+STA + WebServer ya corriendo,
   eso aprieta el heap. Mitigaciones:
   - **MFLN (Max Fragment Length, RFC 6066):** negociar fragmentos de 2–4 KB
     reduce los buffers a una fracción. Requiere que el **servidor lo soporte**.
   - **Una sola conexión TLS a la vez** y vigilar `ESP.getFreeHeap()` antes del
     handshake.
2. **No abrir/cerrar TLS por cada mensaje.** El handshake (asimétrico) es lo
   pesado; repetirlo fragmenta el heap y gasta CPU. Para telemetría frecuente:
   - **Mantener la conexión viva** (HTTP keep-alive / conexión persistente) o
     **MQTT sobre TLS** (un solo handshake, luego mensajes ligeros). **MQTT/TLS es
     la opción recomendada** para telemetría continua.
   - **Reanudación de sesión** (session tickets/IDs) si se reconecta seguido.
3. **Validación de certificado = requiere hora correcta.** TLS verifica
   `notBefore/notAfter`; si el reloj está mal, **el handshake falla**. Hay que
   fijar la hora (RTC DS3231 ya disponible, o NTP) **antes** de conectar.
4. **CA pinning, no `setInsecure()`.** En producción, `client.setCACert(rootCA)`
   con el **root CA del backend** embebido en flash (~1–2 KB, barato). Esto
   autentica al *servidor*; el token/credencial autentica al *dispositivo*. Sin
   pinning, un MITM podría suplantar el backend (crítico sobre todo para **OTA**).

### OTA sobre TLS
`httpUpdate.update(clientSecure, url)` descarga **en streaming** (no necesita el
binario completo en RAM) y escribe en la partición OTA. Imprescindible: **HTTPS +
CA pinning** y, si el equipo es físicamente accesible, **firmar el firmware**
(verificar `sha256`/firma del release) — es la diferencia entre "OTA" y "OTA
segura".

### Recomendación práctica
- **REST/HTTPS** (`WiFiClientSecure` + `HTTPClient`) para acciones puntuales
  (aprovisionamiento, reporte de config, OTA): simple y suficiente.
- **MQTT sobre TLS** para telemetría continua: amortiza el handshake en una
  conexión persistente.
- Siempre: **CA pinning**, **hora válida antes del handshake**, **MFLN** si el
  servidor lo permite, y **vigilar el heap** porque AP+STA + WebServer + TLS es
  lo más exigente que correrá el C3 a la vez.

---

## Caveats conocidos
- **`StaticJsonDocument` ajustado.** `/newparams` recibe ~20 claves; el buffer se subió a
  1024 B para evitar `NoMemory` con credenciales/planta largas.
- La fuente de verdad es `HTML/mainForm.html` (comentado); `mainForm.h` es el artefacto
  generado sin comentarios que sirve el dispositivo. No editar el `.h` a mano.
