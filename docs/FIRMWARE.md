# Firmware

Comportamiento del firmware del ESP32-C3: arranque y red, scheduling, RTC, sesión, Wi-Fi del usuario y OTA. Es común a las dos bases de firmware (`ESP32_controller/`, Arduino-ESP32, y `firmware/`, ESP-IDF, rama `esp-idf-migration`): describe lo que hace el dispositivo, y la última sección resume en qué difieren. Para las rutas HTTP, ver [`API-device.md`](API-device.md); para el diseño del sistema y del portal, [`ARCHITECTURE.md`](ARCHITECTURE.md); para pines y periféricos, [`HARDWARE.md`](HARDWARE.md).

---

## Hardware (ESP32-C3, ver `Constants.h`)
- Lámpara → GPIO 0 (salida digital ON/OFF, activa en bajo: nivel bajo enciende), LED azul → GPIO 1 (canal 1), LED rojo → GPIO 2 (canal 2)
- Buzzer → GPIO 3, ventilador → GPIO 7, bomba de agua → GPIO 10
- PWM: 1 kHz, 8 bits (duty 0–255) en azul y rojo; se envían como 0–100 % y se escalan internamente. La lámpara es digital: el portal manda `0`/`1` y el firmware evalúa `> 0`.
- **Jerarquía de iluminación (producto):** el hardware principal contempla **una sola lámpara con control ON/OFF** (pin *Lámpara*), lo que permite usar lámparas de cultivo comerciales; los canales PWM azul y rojo son un extra para el cultivador avanzado, no el control primario. El portal refleja esto en la vista `edit`: la **Lámpara** va destacada al frente (el control principal) y **Azul/Rojo** viven en un bloque colapsable **"Espectros avanzados (opcional)"**, ambos **dentro de un mismo recuadro** (la sección *Iluminación*). La jerarquía la da la **tipografía** (tamaño/peso/color), no recuadros distintos por bloque. El colapsable se auto-expande si el cultivo ya tiene azul o rojo > 0. Es solo presentación: los tres campos (`ledAzul`/`ledRojo` sliders 0–100, `lampara` toggle 0/1) conservan su contrato con el firmware.
- RTC externo **DS3231** por I²C (dirección `0x68`); la hora se sincroniza desde el navegador en `/newparams`.

---

## Arranque y red
Toda la configuración de red vive en `setup()` (`ESP32_controller.ino`), no en `Plant`:

1. `planta.begin()` inicializa el hardware (GPIO/PWM y todo apagado) y carga el estado
   desde **Preferences** (NVS): namespaces `system`, `plantData`, `config` y `wifi`
   (ver [Persistencia en NVS](#persistencia-en-nvs-preferences)).
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

## Sesión, cultivo y persistencia

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

### Cultivo nuevo vs reset de fábrica

Son **dos operaciones distintas** a propósito: una acción que se hace cada cosecha no
puede exigir las mismas garantías que una escotilla de emergencia, ni borrar las mismas
cosas.

| | `POST /newcrop` | `**reset**` en `/authusercredentials` |
|---|---|---|
| Para qué | Terminar una cosecha y empezar otra | Recuperar el equipo (p. ej. contraseña olvidada) |
| Frecuencia | Habitual | Excepcional |
| Autenticación | **Token obligatorio** | **Ninguna**, a propósito |
| Borra cultivo (nombre, ancla, parámetros) | Sí | Sí |
| Borra cuenta de usuario | **No** | Sí |
| Borra credenciales Wi-Fi | **No** | Sí |
| Interfaz aceptada | Cualquiera | **Solo el AP** |
| UI | Botón explícito con confirmación | Palabra mágica en el campo de contraseña |

**Por qué `/newcrop` exige token y el reset no.** Quien empieza un cultivo nuevo es el dueño,
que por definición puede iniciar sesión: pedirle token no le cierra ninguna puerta. El reset,
en cambio, existe precisamente para cuando **no** se puede iniciar sesión; exigirle token lo
convertiría en un candado cuya única llave está dentro. Son requisitos opuestos porque
resuelven problemas opuestos.

**Por qué el reset se restringe por interfaz.** Al no pedir credenciales, su única barrera es
la cercanía física, así que tiene que llegar por el AP. Hoy esa comprobación es **redundante**
—el servidor ya solo escucha en la IP del SoftAP, así que toda petición viene del AP por
construcción— y se mantiene a propósito como defensa en profundidad: si alguien volviera a
ligar el servidor al wildcard, el reset seguiría protegido. El handler compara
`server.client().localIP()` con `WiFi.softAPIP()` —la interfaz que aceptó la conexión, no un
dato que el cliente envíe— y responde `RESET_REQUIRES_AP` (403) si no coincide. Es
**fail-closed**: sin IP de AP válida, deniega.

**Por qué la palabra mágica sigue siendo aceptable para el reset.** Un comando oculto en el
campo de contraseña es un patrón pobre para algo rutinario —de ahí el botón de `/newcrop`—
pero razonable para una escotilla que debe existir sin ocupar espacio en la interfaz y sin
invitar a pulsarla por curiosidad.

### Reset de fábrica (detalle)
En la pantalla de **login** (`auth`), escribir como contraseña la palabra mágica
**`**reset**`** dispara `hardReset()`, que borra **todos** los namespaces de Preferences
(`system`, `plantData`, `config` y `wifi`), y reinicia el ESP32. Se intercepta en
`authUserCredentials()` **antes** de comparar credenciales. El registro **no** acepta reset
(no hay nada previo que borrar).

> **Incluye la red Wi-Fi.** El namespace `wifi` se borra junto al resto, para no dejar la
> passphrase de la red del usuario almacenada tras un reset — importa si el equipo cambia de
> manos. `hardReset()` también limpia las copias en RAM (`_SSID`, `_SSIDpass`,
> `_wifiPending`) y la sesión activa. Por eso **tras un reset hay que reconfigurar la Wi-Fi**,
> algo que `/newcrop` no requiere.

### Persistencia en NVS (Preferences)

Todo lo que sobrevive a un corte de luz vive aquí. Es el mapa completo:

| Namespace | Clave | Tipo | Contenido | `/newcrop` | Reset fábrica |
|---|---|---|---|---|---|
| `system` | `systemStatus` | blob (13 B) | Flags y parámetros del cultivo (índices del enum `SystemStatus`) | **parcial**: reinicia parámetros y `systemEnable`; conserva `hasRegisteredUser` y `hasWifiCredentials` | sí, entero |
| `system` | `cropStart` | `uint32` | Ancla de la edad del cultivo (`daysSinceEpoch`). Se escribe en el primer `/newparams` con fecha válida | sí | sí |
| `plantData` | `plantName` | string | Nombre de la planta | sí | sí |
| `config` | `username` | string | Nombre de usuario | **no** | sí |
| `config` | `pwSalt` | string | Salt de 16 B en hex (32 chars), aleatorio por registro | **no** | sí |
| `config` | `pwHash` | string | PBKDF2-HMAC-SHA256 de la contraseña, 32 B en hex (64 chars) | **no** | sí |
| `wifi` | `ssid` | string | SSID de la red del usuario | **no** | sí |
| `wifi` | `pass` | string | Passphrase de esa red, en texto plano | **no** | sí |

**Lo que NO se persiste, a propósito:**

- **Token de sesión** (`_sessionToken`) y su expiración: solo RAM, así que un reinicio o
  corte de luz invalida la sesión sin nada que limpiar.
- **Edad del cultivo** (`dia`/`semana`): se deriva de `cropStart` + RTC, no se guarda un
  contador (así el cultivo "sigue envejeciendo" con el equipo apagado y no se desgasta
  flash cada noche).
- **Estado de los actuadores** y validez del RTC: se recalculan en cada `turnOnDevices()`.
- **Versión del firmware**: es una constante de compilación (`Constants.h`), no un valor
  guardado, para que cada binario reporte lo que realmente es tras un OTA.
- **Hora**: vive en el RTC DS3231 (con su propia pila), no en NVS.

**Cuidado al editar el enum `SystemStatus`:** el blob se guarda por **índice**, no por
nombre. Renombrar es seguro; insertar, quitar o reordenar reinterpreta los datos ya
guardados, y cambiar la **cantidad** de campos cambia el tamaño del blob —
`Preferences::getBytes` no copia nada si lo guardado es más grande que el buffer, así que
la configuración se leería en ceros sin ningún error. Por eso, al cambiar campos, se borra
la flash antes de cargar el firmware.

### Cómo se aplican los parámetros (sin reboot)
`/newparams` **ya no reinicia** el ESP32: los parámetros se aplican en vivo. Al final de
`validateCropParameters()` (`Plant.cpp`) el flujo es:

1. La config validada se vuelca en `_systemStatus[]` (RAM) — la **fuente de verdad** que
   usa el dispositivo en marcha — y el nombre de planta en `_plantName`.
2. Se fija la hora del navegador en el RTC (`setCurrentTime()`).
3. Se persiste a NVS (namespaces `system` y `plantData`) para sobrevivir cortes de luz.
4. `turnOnDevices()` aplica la config **al instante** (PWM de LEDs según fotoperiodo + duty,
   relés de bomba/ventilador según frecuencia/duración).

### Interruptor general del cultivo (`enable` → `systemEnable`)

`turnOnDevices()` arranca con dos guardas que cortan todo antes de evaluar horarios:

1. **`systemEnable == 0`** → `allDevicesOff()` y retorna. Es el "off" explícito del usuario
   desde el portal: no se acciona **nada** (ni luces, ni riego, ni ventilación) aunque el
   fotoperiodo o el intervalo digan que toca. Tiene prioridad sobre el resto.
2. **`_rtcValid == false`** → mismo apagado total (ver *Validez del RTC y modo seguro*).

Ambas comparten `allDevicesOff()` para que no puedan divergir si se añade un actuador
nuevo. Como `/newparams` llama a `turnOnDevices()` al final, desmarcar la casilla apaga
todo **de inmediato**, sin reiniciar. La edad del cultivo (`dia`/`semana`) **sí** sigue
avanzando con el sistema desactivado, porque se deriva del calendario.

Luego el `loop()`, cada `deviceUpdateInterval`, refresca `_currentTime` desde el RTC
(`getCurrentTime()`) y re-aplica `turnOnDevices()`. **Todo corre en un único task
(`loopTask`)** — I²C, handlers HTTP y el log del estado cada `systemLogInterval`. Un solo
task tocando `_currentTime`/`_systemStatus` evita por construcción la carrera de leer un
arreglo a medio actualizar, sin necesidad de mutex ni de una tarea de log aparte. Tras un
corte de luz, `begin()` recarga `_systemStatus` desde NVS y el loop lo re-aplica en
segundos; la config se aplica en vivo, así que solo `**reset**` reinicia el equipo.

---

## Endpoints HTTP
Ver el detalle de payloads, validaciones y catálogo de errores en **[`API-device.md`](API-device.md)**.

| Método | Ruta                  | Handler                  | Propósito |
|--------|-----------------------|--------------------------|-----------|
| GET    | `/`                   | `handleRoot`             | Sirve el portal (`mainForm_gz`, HTML gzip) con `Content-Encoding: gzip` vía `send_P` directo desde flash (sin `String` temporal); el navegador lo descomprime |
| GET    | `/getparams`          | `handleGetParameters`    | Estado del dispositivo en JSON (incluye `hasRegisteredUser`; con `?token=`, `sessionValid`; y siempre `wifiConnected`/`wifiSsid`) |
| POST   | `/usercredentials`    | `handleUserCredentials`  | Alta de usuario (primer arranque). No reinicia; **devuelve `token`** de sesión |
| POST   | `/authusercredentials`| `handleAuthUserCredentials` | Login para desbloquear edición; **devuelve `token`**; intercepta `**reset**` |
| POST   | `/newparams`          | `handleNewParameters`    | Guarda y **aplica en vivo** los parámetros del cultivo + hora (sin reiniciar). **Exige `token`** vigente |
| POST   | `/newcrop`            | `handleNewCrop`          | Cierra el cultivo y deja listo otro: borra nombre, ancla y parámetros; **conserva cuenta y Wi-Fi**. **Exige `token`** vigente |
| GET    | `/wifiscan`           | `handleWifiScan`         | Escaneo **asíncrono**: arranca y responde `{scanning:true,networks:[]}`; al terminar `{scanning:false,networks:[{ssid,rssi,secure}]}` (top-5 sin nombres repetidos). Sin sobre `{status,message}` |
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

## Compilar y flashear
- IDE: Arduino IDE / arduino-cli. Placa: ESP32-C3 (ver `ci.json`).
- Dependencias: core **ESP32 (WiFi/WebServer/DNSServer)**, **ArduinoJson** (v6, API
  `StaticJsonDocument`), `Wire`, `Preferences`.
- Antes de compilar, crear `ESP32_controller/sensible.h` con las credenciales del AP (ver [Secretos del AP](#secretos-del-ap-sensibleh)).
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
**stateless** (todo se recalcula del reloj, sobrevive cortes de luz sin derivar). Los
valores son el intervalo directo en horas.

El front se alinea al firmware: la constante JS `VALID_FREQUENCIES` en `HTML/mainForm.html`
debe mantenerse igual a `validFrequencies` si esta cambia. **Tope 255** (`uint8_t`): un
intervalo > ~10 días exigiría ampliar `validFrequencies` y `_systemStatus` a `uint16_t`.

---

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
- Reusa `daysSinceEpoch()` del control de riego. El día/semana se derivan del RTC, no de
  `SystemStatus`. `_systemStatus` mide 13 bytes, dimensionado por el centinela
  `systemStatusCount` para que el arreglo siga al enum automáticamente. Como el blob de
  NVS cambia de tamaño si el enum cambia, al cargar un firmware con distinto layout hay que
  **borrar la flash** (es el procedimiento habitual del proyecto): `Preferences::getBytes`
  no copia nada si lo guardado es más grande que el buffer, así que un blob viejo se leería
  en ceros.

---

## Validez del RTC y modo seguro (implementado)

Todo el scheduling —fotoperiodo, riego, ventilación y edad del cultivo— se deriva del
DS3231. Si esa hora es basura, el sistema no se equivoca "un poco": puede regar de más
o dejar el cultivo a oscuras. Por eso `Plant` mantiene `_rtcValid` y **no acciona nada**
mientras la hora no sea de fiar.

**Tres condiciones para considerar válida una lectura** (`Plant::getCurrentTime`):

1. El chip responde por I²C y entrega los 7 registros de tiempo.
2. Los valores caen en rango (`seg/min ≤ 59`, `hora ≤ 23`, `diaSem 1–7`, `dia 1–31`,
   `mes 1–12`, `anio ≤ 99`).
3. El bit **OSF** (*Oscillator Stop Flag*, registro `0x0F` bit 7) está en 0.

**Por qué no basta el chequeo de rangos:** un DS3231 sin batería arranca en
`2000-01-01 00:00:00`, que **cumple todos los rangos** y sin embargo no es la hora real.
El OSF es la única señal que distingue "hora fijada por el usuario" de "el reloj se
reinició y esto es un default". El chip lo levanta cuando el oscilador se detuvo
(primer arranque, pila agotada, pérdida total de alimentación) y el firmware lo baja en
`setCurrentTime()`, al escribir la hora que manda el navegador.

**Enmascarado de bits de control:** al convertir de BCD se limpian los bits que no son
parte del número — segundos `& 0x7F` (bit 7 = CH), horas `& 0x3F` (bit 6 = modo 12/24 h,
bit 5 = AM/PM) y mes `& 0x1F` (bit 7 = *century*) — para que un bit alto no corrompa la
conversión.

**Comportamiento con `_rtcValid == false`:**

- `turnOnDevices()` llama a `allDevicesOff()` y retorna: luces, bomba y ventilador
  apagados. Es el mismo apagado total que `enable: false`, y ambos comparten ese helper
  para que no puedan divergir si se añade un actuador.
- `cropDayFromRtc()` devuelve `0`, así que `dia` y `semana` de `/getparams` van en `0`.
- El log serie **no** lo dice de forma explícita: solo se deduce de la fecha del
  encabezado (un RTC sin hora imprime algo como `01/01/00`).
- `/getparams` lo reporta como `rtcValid`, y el portal lo trata con **discreción
  deliberada**: el indicador de estado del dashboard pasa a un tercer valor,
  **"En espera"** (habilitado pero sin accionar), y `dia`/`semana` se pintan `—`.
  Sin ese tercer estado la UI diría "Activo" mientras nada funciona.

**Recuperación:** ocurre sola en el uso normal. Guardar parámetros (`POST /newparams`)
implica que el navegador manda su fecha/hora, el firmware la escribe y limpia el OSF.
No hace falta ninguna acción especial ni un endpoint aparte.

> **Nota de diagnóstico:** este modo seguro es la causa más probable de un equipo que
> "no hace nada" recién montado o con la pila del RTC agotada. El portal lo distingue
> del apagado voluntario mediante el estado **"En espera"** (ver arriba).
>
> **Criterio de UX aplicado:** en un equipo nuevo `rtcValid == false` es el estado
> **normal** —el RTC no se ha puesto en hora hasta el primer `/newparams`—, así que
> alarmar sobre él enseñaría a ignorar los avisos. Por eso no hay banner ni color de
> error: se corrige un elemento que ya existía para que deje de mentir, sin añadir
> mobiliario visual. Se descartó un tooltip (`title`) porque el portal se usa desde el
> celular, donde es inaccesible.
>
> **Mejora pendiente:** distinguir *configuración pendiente* de *hardware degradado*.
> El mismo flag significa dos cosas: si el cultivo **nunca** se ancló, es setup normal;
> si **ya estaba anclado** y la hora se perdió, la pila del RTC está agotada y eso sí
> es accionable ("cambia la pila"). El firmware ya tiene los dos datos
> (`_rtcValid` y `_cropStartDay`); faltaría exponer ese segundo bit.

---

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
- El AP `SmartPlant` usa **WPA2-PSK** (`sensible.h`), así que el enlace va cifrado; aun así
  **no hay TLS** dentro del enlace. El token añade UX y cierra el hueco de reenviar
  credenciales en claro a `/newparams`, pero la confidencialidad real la aporta el WPA2: con
  una passphrase compartida y conocida, alguien en la misma red podría capturar el token
  (de ahí la recomendación de contraseña por dispositivo en `sensible.h`).
- **El portal se sirve SOLO por el AP.** `WebServer server(apGatewayIp, 80)` liga el socket
  a la IP del SoftAP (`192.168.4.1`) en vez de al wildcard `0.0.0.0`, así que **ningún
  endpoint responde por la interfaz STA** (la red del usuario). Sin esto, con AP+STA activo
  todo el portal —login, edición, configuración de red y el comando de reset— quedaría
  alcanzable desde esa LAN con solo escanear el puerto 80, sin la passphrase del AP. Es la
  decisión de fondo: el portal es el plano de control *local*, y el enlace con el backend
  será **saliente**, iniciado por el dispositivo.

  **Precio aceptado:** no se puede abrir el dashboard desde el celular estando en la Wi-Fi de
  casa; hay que cambiarse a la red `SmartPlant`. A cambio, el modelo de amenaza es
  únicamente **cercanía física + passphrase del AP**.

  `setup()` comprueba en runtime que `WiFi.softAPIP()` coincide con `apGatewayIp` y avisa por
  serie si no: un desajuste haría fallar el `bind()` en silencio y el portal quedaría muerto
  sin ningún error visible.
- **El DNS del portal captivo sí sigue escuchando en todas las interfaces.** `DNSServer::start()`
  termina en `_udp.listen(_port)`, sin dirección, y responde **cualquier** dominio con la IP
  del SoftAP. En la red del usuario eso deja un resolvedor DNS abierto que contesta
  direcciones falsas. El impacto práctico es bajo (nada en la LAN lo usa como DNS por
  defecto) y **la librería no permite ligarlo a una interfaz**, así que es una limitación
  aceptada: eliminarla exigiría no levantar el DNS con el STA activo, lo que rompería el
  portal captivo en modo AP+STA.

  > **Firmware ESP-IDF (`firmware/`, rama `esp-idf-migration`):** el DNS captivo es código
  > propio (`captivedns.cpp`), así que **sí** se liga solo a la IP del AP (`192.168.4.1`) y
  > no responde por la interfaz STA. Es una diferencia deliberada: IDF es más estricto que
  > Arduino en este punto. Detalle en `firmware/README.md`.
- El token se genera solo en login/registro (no por request), evitando escrituras innecesarias.

### Huecos de seguridad identificados y su resolución

Los tres huecos identificados están **resueltos**. Eran **interdependientes**: el límite de
intentos podía dejar al usuario fuera del equipo si se implementaba sin resolver antes el
reset, y por eso se hicieron en ese orden.

#### 1. Contraseña de usuario (resuelto)

La contraseña ya **no se guarda ni se conserva en RAM**: en su lugar viven un salt aleatorio
y la clave derivada con **PBKDF2-HMAC-SHA256** (`config/pwSalt` y `config/pwHash`, en hex).
La contraseña no vive en RAM: solo se conservan el salt y el hash, y el log serie no
imprime nada de la contraseña.

**Decisiones de implementación:**

- **`mbedtls_md_hmac()` de `mbedtls/md.h`**, no `mbedtls_pkcs5_pbkdf2_hmac()`. Es deliberado:
  la API de PKCS#5 cambió de firma entre mbedTLS 2.x y 3.x (quedó deprecada en favor de la
  variante `_ext`), lo que ataría el firmware a la versión del core; la de `md.h` es estable
  en ambas.
- **`dkLen` = 32 = tamaño de SHA-256**, así que PBKDF2 se reduce a **un solo bloque** y no
  hace falta el bucle externo del RFC: `U1 = HMAC(pass, salt‖INT32BE(1))`,
  `Ui = HMAC(pass, Ui-1)`, `DK = U1 ⊕ … ⊕ Uc`.
- **Salt de 16 B por registro** desde `esp_random()` (RNG por hardware): dos equipos con la
  misma contraseña producen hashes distintos, así que una tabla precalculada no sirve.
- **Comparación en tiempo constante** (`constantTimeEquals`): acumula las diferencias en vez
  de cortar en el primer byte distinto, para no filtrar por tiempo cuántos bytes acertó un
  atacante.
- **Fail-closed al cargar:** si `pwSalt`/`pwHash` faltan o están corruptos, los buffers quedan
  en cero y ningún login coincide. El equipo queda accesible solo por reset de fábrica, que es
  preferible a aceptar cualquier contraseña.

**Pendiente de calibrar:** `pbkdf2Iterations` está en 20 000 como punto de partida. Hay que
**cronometrarlo en la placa**: el login bloquea `handleClient()` mientras deriva (1 núcleo
@160 MHz), así que es un compromiso directo entre coste para el atacante y latencia del
login. La constante vive en `Constants.h`.

#### 2. Reset de fábrica sin autenticación (resuelto por interfaz, no por token)

El reset **no exige autenticación** a propósito: existe para cuando no se puede iniciar
sesión, así que pedirle token lo inutilizaría.

En su lugar se hicieron tres cosas:

1. **Separar operaciones.** Lo que el usuario hace cada cosecha se movió a
   [`POST /newcrop`](#cultivo-nuevo-vs-reset-de-fábrica), que **sí** exige token y conserva
   cuenta y Wi-Fi. El reset quedó reducido a su papel real: escotilla de recuperación.
2. **Ligar el servidor a la interfaz del AP**, que cierra la clase entera de exposición: ya
   no es solo el reset, es que *ningún* endpoint responde desde la red del usuario.
3. **Restringir el reset por interfaz** en el handler, hoy redundante por lo anterior pero
   conservado como defensa en profundidad.

Lo que **no** resuelve: quien conozca la passphrase del AP sigue pudiendo resetear sin
credenciales. Eso es deliberado —es la vía de recuperación— y su endurecimiento requeriría
hardware: un botón físico o conteo de arranques, que permitiría entonces exigir token también
al reset. Queda como opción si el PCB se revisa.

#### 3. Login sin límite de intentos (resuelto)

`POST /authusercredentials` ya no se puede martillear sin coste. Estado en RAM
(`_failedLogins`, `_lockoutUntil`) y política en `Constants.h`.

**Política:** los primeros `loginMaxAttempts` (5) fallos consecutivos no se castigan —los
errores de tecleo son normales—. Del quinto en adelante la espera arranca en
`loginLockoutBaseMs` (5 s) y se **duplica** con cada fallo adicional hasta
`loginLockoutMaxMs` (5 min): 5, 10, 20, 40, 80, 160, 300. Un login correcto limpia el
contador; expirar la ventana **no** lo limpia, así que quien insista espera cada vez más.

**El orden de las comprobaciones es la parte que importa,** y no es arbitrario:

1. **El comando `**reset**` se evalúa antes del bloqueo.** Es la vía de recuperación: si el
   bloqueo lo tapara, quien olvide su contraseña e insista unas cuantas veces se quedaría
   sin ninguna salida. Ésta era la interdependencia que hacía necesario resolver el punto 2
   antes que este.
2. **El bloqueo se evalúa antes de derivar el hash.** Cada PBKDF2 cuesta cientos de ms
   bloqueando `handleClient()`; si el bloqueo se comprobara después, un atacante obtendría
   exactamente lo que busca un DoS —consumir el único núcleo— aun estando bloqueado.

**En RAM y no en NVS, a propósito:** evita desgastar flash en cada intento fallido y evita
un bloqueo *persistente*, que sería un vector de denegación de servicio peor que el ataque
que previene. El precio es que un corte de energía reinicia el contador, así que esto frena
scripts que insisten, no a alguien con acceso físico —que ya está fuera del modelo de
amenaza—.

**Comparación de tiempos a prueba del wrap de `millis()`** (~49 días), con la misma resta
con signo que usa `isSessionValid()`.

---

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
- **El escaneo es asíncrono:** `/wifiscan` arranca `WiFi.scanNetworks(true)` y responde
  `scanning: true` al instante; el front consulta el mismo endpoint hasta recibir la
  lista, sin bloquear `handleClient()`. Queda el límite físico de una sola antena: durante
  el escaneo el AP puede perder algún paquete.
- **Sin tráfico saliente todavía:** el firmware no abre ninguna conexión a un
  backend. Por construcción, un dispositivo **no hace peticiones inútiles**.

---

## Modelado de la base de datos (backend futuro)
El modelo de datos completo —tablas, columnas, payloads y flujos— vive en
[`BACKEND.md`](BACKEND.md), que es la **referencia única**. Aquí solo queda lo específico
del *lado firmware*: qué manda el dispositivo y por qué.

**Identidad (resumen):** el dispositivo se aprovisiona **una vez** con `email + pass + mac`
sobre TLS y recibe un **token de dispositivo** (revocable, guardado en NVS). La MAC
identifica, el user/pass autentica y el token se usa en todo lo demás — la contraseña solo
viaja en el aprovisionamiento.

#### Desfase de nombres entre portal y backend

Los campos de LED **no se llaman igual** en los dos contratos:

| Portal ↔ firmware (`/newparams`, `/getparams`) | Firmware → backend (REST, snake_case) |
|---|---|
| `ledAzul` (0–100) | `led_a` |
| `ledRojo` (0–100) | `led_r` |
| `lampara` (0/1) | `led_b` |

El backend define `led_a/led_r/led_b` en `models.py`, `schema.sql`, la migración
`001_initial_schema.py` y los routers (`devices.py` valida `led_b` como `ge=0, le=1`, igual
que el firmware). **Cuando se implemente el envío de configuración al backend habrá que
mapear explícitamente** `ledAzul→led_a`, `ledRojo→led_r`, `lampara→led_b`, o renombrar las
columnas con una migración. Hoy no hay incompatibilidad activa: el firmware todavía no envía
configuración al backend.

### Cómo el firmware NO hace peticiones inútiles
- **Sin Wi-Fi → cero tráfico** (ni puede alcanzar el backend).
- **Sin token y sin acción del usuario → silencio total.** No hay polling de entitlement en
  background; el vínculo se decide al aprovisionar (acción humana).
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

## OTA local (subir el `.bin` desde el teléfono)

Caso de uso: el usuario tiene el binario en su teléfono y lo **sube al dispositivo
por la red del AP**, sin internet ni nube. Útil cuando el equipo está en un lugar
de difícil acceso. **No confundir con la "OTA segura" sobre TLS/Internet** (sección
anterior): esta es local, por HTTP dentro del enlace WPA2, y sin firma.

**Estado:** implementado de punta a punta a nivel de firmware y cliente. El **cliente**
está en **ambos portales**: la V1 (portal embebido hoy) y la V2. En los dos hay vistas
`ota`/`flashing` (input de archivo, subida `multipart` por `XMLHttpRequest` con barra de
progreso); en la V1 el acceso es un chip "Actualizar firmware" en el dashboard, junto al de
Wi-Fi, y pasa por la misma compuerta de login. El **endpoint** `POST /otaupdate` existe en
el firmware (`ESP32_controller.ino`) usando la librería `Update`. Pendiente: probar el flujo
completo en hardware real (hasta ahora el endpoint se ha ejercitado con `curl` y el cliente
con el mock del preview).

### Requisito CRÍTICO: tabla de particiones con dos slots OTA
La librería `Update` escribe el binario nuevo en el slot **inactivo** mientras el firmware
corre desde el activo, y al validar cambia el arranque en `otadata`. **Esto exige una tabla
de particiones con `ota_0` + `ota_1` + `otadata`.** Sin dos slots, `Update.begin()` falla.

El proyecto **fija su propia tabla** en `ESP32_controller/partitions.csv` (versionada, no
depende del menú *Tools → Partition Scheme* del IDE): flash 4 MB, `nvs` 20 KB, `otadata`
8 KB (termina justo en `0x10000`) y **dos slots de app de ~1.94 MB** (`ota_0` en `0x10000`,
`ota_1` en `0x200000`), sin SPIFFS. Con la app actual (~1.1 MB) cada slot queda holgado, con
margen para el cliente del backend y TLS futuros.

> **`ota_0` va en el offset estándar `0x10000`** (donde el core Arduino-ESP32 escribe la
> app). Es crítico que coincida: si la tabla declara la primera app en otro offset (p. ej.
> `0x20000`) pero el IDE la flashea en `0x10000`, el bootloader arranca en el slot equivocado
> y falla con `No bootable app partitions`. Por eso no se usa un offset no estándar.

> **Instalar esta tabla reescribe el layout de flash y BORRA la NVS** (usuario, Wi-Fi,
> `systemStatus`, `cropStart`): hay que re-registrar tras el primer flasheo. Es el "borra la
> flash" habitual del proyecto. En Arduino IDE hay que seleccionar el esquema de partición
> personalizado para que tome `partitions.csv`.

### Endpoint `POST /otaupdate`
1. **Auth por header** `Authorization: Bearer <token>`: el cuerpo es `multipart` (ocupado
   por el `.bin`), y el header llega **antes** que el binario, así que el token se valida en
   `UPLOAD_FILE_START` —antes de escribir un solo byte en flash— y se rechaza con `401` sin
   recibir el archivo completo. Requiere `server.collectHeaders({"Authorization"})` en
   `setup()` para que `WebServer` conserve la cabecera.
2. **Solo por el AP** (`403` si no): misma barrera fail-closed que el reset — se compara la
   IP local del socket con `WiFi.softAPIP()`.
3. **Escritura en streaming** con `Update.begin(UPDATE_SIZE_UNKNOWN)` → `Update.write()` por
   trozos → `Update.end(true)`. No requiere el binario completo en RAM.
4. **Integridad:** `Update` valida el *magic byte* del firmware ESP32 (rechaza un archivo
   que no sea firmware). Responde `500` con JSON `{message}` ante cualquier fallo, dejando
   intacto el firmware en ejecución.
5. **Reinicio** al terminar OK (`ESP.restart()`); el AP parpadea (radio única) y el cliente
   reconecta — el portal ya avisa "no cierres la ventana" y maneja el post-reinicio.
6. **Sin firma criptográfica:** acepta cualquier `.bin` válido de quien tenga sesión y esté
   en el AP. La firma es cosa de la OTA segura por Internet, no de esta local.

> **PENDIENTE (riesgo conocido) — sin rollback automático ante imagen no arrancable.**
> `Update.end(true)` valida que el `.bin` sea una imagen ESP32 **estructuralmente** válida
> (magic byte + checksum), pero NO garantiza que arranque en este C3 (p. ej. un binario de
> otra variante o con un bug de arranque pasa esa validación). El firmware reinicia directo
> a la imagen nueva **sin "marcado de app válida"**: el binario nuevo no confirma "arranqué
> bien", así que si no arranca, el equipo queda inservible (brick) — habría que reflashear
> por USB. El **doble slot** (`ota_0`/`ota_1`) es el prerequisito del rollback (la imagen
> anterior queda intacta en el otro slot), pero el **mecanismo** que lo dispara no está
> activado en Arduino. Mitigaciones por prioridad:
> 1. **[barata, sin rollback]** verificar **hash** del binario: el cliente envía el SHA/MD5 y
>    el firmware lo fija con `Update.setMD5(...)` → `Update.end()` falla si no coincide
>    (restringe a binarios aprobados; evita corruptos/equivocados).
> 2. **[completa]** habilitar rollback automático: marcar la app como válida tras un arranque
>    sano (`esp_ota_mark_app_valid_cancel_rollback()`) y activar el rollback del bootloader
>    (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`). Es de primera clase en **ESP-IDF**, por eso
>    encaja en la migración (ver [ROADMAP.md](ROADMAP.md)); en Arduino puro es incómodo.

`firmwareVersion` se expone en `/getparams` (constante de compilación en `Constants.h`), así
que la vista OTA puede mostrar la versión instalada. **Hay que subir ese valor en cada
release que se distribuya por OTA.**

---

## Nota de frecuencias

`irrH` y `ventH` son el **intervalo en horas entre activaciones**, no "veces por día". El firmware los valida con `isValidFrequency()` contra `validFrequencies` en `Constants.h`:

```cpp
const uint8_t validFrequencies[] = {0, 1, 2, 3, 4, 6, 8, 12, 24, 48, 72, 168};
```

| Valor | Significado |
|---|---|
| `0` | Apagado |
| `1`–`12` | Cada N horas (sub-diario) |
| `24` | Diario |
| `48` | Cada 2 días |
| `72` | Cada 3 días |
| `168` | Semanal |

Con este modelo el riego sub-diario y el espaciado multi-día usan la **misma** lógica en `manageDevice()`: un módulo sobre un contador continuo de horas derivado del RTC (`epochHours % intervalo`), que no se reinicia a medianoche. Es *stateless*, así que sobrevive cortes de luz sin derivar.

El tope es 255 (`uint8_t`): un intervalo mayor a ~10 días exigiría ampliar `validFrequencies` y `_systemStatus` a `uint16_t`.

Si `validFrequencies` cambia en `Constants.h`, hay que actualizar en paralelo las `<option>` del `<select>` y la constante JS `VALID_FREQUENCIES` en `HTML/mainForm.html`.

---

## Nota sobre el RTC y el modo seguro

El scheduling depende por completo del DS3231, así que el firmware **no acciona nada si la hora no es de fiar**. Una lectura se considera fiable solo si cumple las tres condiciones:

1. El chip responde por I²C y entrega los 7 registros de tiempo.
2. Los valores caen en rango (`seg/min ≤ 59`, `hora ≤ 23`, `diaSem 1–7`, `dia 1–31`, `mes 1–12`, `anio ≤ 99`).
3. El bit **OSF** (*Oscillator Stop Flag*, registro `0x0F` bit 7) está en 0.

El OSF es el que distingue una hora real de un valor por defecto: un DS3231 sin batería arranca en `2000-01-01 00:00:00`, que **pasa el chequeo de rangos** pero no es la hora. El chip levanta OSF cuando el oscilador se detuvo, y el firmware lo baja al escribir la hora desde el navegador.

**Consecuencias observables cuando la hora no es fiable:**

- Luces, bomba y ventilador quedan **apagados** (mismo apagado total que `enable: false`).
- `/getparams` reporta `rtcValid: false`.
- `dia` y `semana` de `/getparams` devuelven `0`; el portal los pinta como `—`, porque un `0` afirmaría una edad de cultivo que no se puede calcular.
- El indicador de estado del dashboard muestra un **tercer estado, "En espera"**, en lugar de "Activo": el sistema está habilitado pero no acciona nada. Sin ese estado la UI diría "Activo" mientras nada funciona.
- El log serie **no** lo dice de forma explícita: solo se deduce de la fecha del encabezado (un RTC sin hora imprime algo como `01/01/00`).

**Cómo se sale de ese estado:** guardando parámetros (`POST /newparams`), porque el navegador manda la fecha/hora y el firmware la escribe en el RTC y limpia el OSF. Es decir, se resuelve solo en el flujo normal de uso; no hace falta ninguna acción especial.

> Este es el motivo más probable de un "no hace nada" en un equipo recién montado o con la pila del RTC agotada: cambiar la pila y volver a guardar parámetros.

---

## Nota sobre `lampara`

La lámpara es el **control principal de iluminación**. Está en **GPIO 0 como salida digital**, no como canal PWM: solo tiene dos estados (encendida/apagada). Eso permite usar lámparas de cultivo comerciales que se manejen con un control ON/OFF simple, a través de la etapa de potencia (relé/contactor; ver [`HARDWARE.md`](HARDWARE.md)). Los canales PWM azul y rojo son un extra para quien quiera controlar esos espectros con precisión.

> **Cambio de nombre:** esta clave se llamaba `ledBlanco` y el error `INVALID_WHITE_LED_VALUE`. Un cliente que aún envíe `ledBlanco` recibe `MISSING_FIELDS`.

- El portal envía `0` o `1`.
- El firmware **exige** `0` o `1`: cualquier otro valor se rechaza con `INVALID_LAMP_VALUE` (400). La validación tiene su propio estado, separado de `INVALID_LED_VALUE`, que cubre solo los espectros azul y rojo.
- Internamente se guarda tal cual en `_systemStatus[lampOn]` y se aplica con **lógica activa en bajo**: `deviceOn` (hoy nivel bajo) enciende y `deviceOff` (nivel alto) apaga. La polaridad de todas las salidas ON/OFF está centralizada en `deviceOn`/`deviceOff` (`Constants.h`). `/getparams` siempre devuelve `0` o `1`.

Los canales azul (`ledAzul`) y rojo (`ledRojo`) sí son PWM y usan el rango 0–100 % completo, escalado internamente a 0–255.

> **Nombres y contrato:** las claves son `ledAzul`, `ledRojo` (`0–100`, PWM) y `lampara` (`0/1` estricto, salida digital). `lampara` distinto de `0/1` devuelve `INVALID_LAMP_VALUE`. Cualquier otro nombre de clave produce `MISSING_FIELDS` (falta una obligatoria).

---

## Nota sobre frecuencia y duración

`irrH`/`ventH` dicen **cada cuánto** se activa el dispositivo; `irrM`/`ventM`, **cuánto tiempo** permanece encendido dentro de la hora en que toca.

**Regla:** si la frecuencia es mayor que 0, la duración debe ser **al menos 1 minuto**. La combinación "frecuencia activa + duración 0" se rechaza con `INVALID_IRRIGATION_DURATION` / `INVALID_VENTILATION_DURATION` (400).

El motivo es que esa combinación **no hace nada**: `manageDevice()` enciende mientras `minuto_actual < duración`, así que con duración 0 la condición nunca se cumple y el dispositivo jamás arranca. Al mismo tiempo, el portal y el dashboard mostrarían una frecuencia configurada ("Cada 3h"), de modo que un riego que nunca ocurre pasaría inadvertido. Para desactivar existe la forma canónica y explícita: **frecuencia `0` ("Apagado")**.

La regla es asimétrica a propósito: **frecuencia 0 con duración > 0 sí es válida**. Conservar el valor de duración mientras el dispositivo está apagado es útil para cuando se reactive.

---

## Diferencias entre versiones

El comportamiento descrito en este documento es el del firmware **Arduino-ESP32** (`ESP32_controller/`), que es el estable. El firmware **ESP-IDF** (`firmware/`, rama `esp-idf-migration`) está en desarrollo y busca el mismo comportamiento frente al portal y al usuario (misma API, mismas reglas de validación y seguridad). Las diferencias deliberadas se documentarán aquí.

Lo que cambia es la capa de framework:

| Arduino-ESP32 | ESP-IDF |
|---|---|
| `Preferences` | `nvs_flash` / API `nvs` |
| `Wire` | `driver/i2c_master.h` |
| `ledcAttachChannel`, `pinMode`, `digitalWrite` | `driver/ledc.h`, `driver/gpio.h` |
| `WiFi.*` | `esp_wifi` + `esp_netif` |
| `DNSServer` | servidor DNS captivo propio |
| `WebServer` | `esp_http_server` |
| `String` | `std::string` / `char[]` |

La lógica de `Plant` y `utils` (scheduling, sesión, validación, RTC) no depende de Arduino y se porta casi sin cambios. El estado de la migración, por fase, está en [`ROADMAP.md`](ROADMAP.md#plan-de-migración-a-esp-idf).
