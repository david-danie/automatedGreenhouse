# SmartPlant — Controlador ESP32 (firmware + portal captivo)

Firmware para un **ESP32-C3** que controla un cultivo (iluminación LED por espectro,
riego y ventilación) y se configura desde el celular/PC vía un **portal captivo
Wi-Fi**. El dispositivo levanta un Access Point, sirve un formulario HTML embebido y
recibe la configuración por HTTP en JSON.

Este documento cubre el **diseño interno**: por qué el código está escrito así, qué
trade-offs se tomaron y qué caveats quedan abiertos. Para el contrato de la API ver
[`API.md`](API.md); para pines y periféricos, [`HARDWARE.md`](HARDWARE.md); el índice
completo está en [`README.md`](README.md).

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
- Quitar los comentarios del `.h` **ya casi no ahorra nada**: medido hoy, el fuente son
  75,353 bytes y el artefacto 74,879 (~73 KB), es decir **474 bytes (0.6 %)**. El
  generador solo elimina comentarios de **línea completa**, y al crecer el portal la
  proporción de esas líneas se volvió marginal. La cifra "~57 KB → ~49 KB" que aparecía
  aquí correspondía a un estado muy anterior del archivo. El ahorro real está en
  **gzip** (~12–18 KB estimados), todavía pendiente.
- Generar el `.h` desde el `.html`: strip de comentarios HTML (`<!-- -->`), CSS (`/* */`)
  y JS (`//`) + colapso de líneas en blanco + wrapper `static const char mainForm[] = R"===(` … `)===";`.
  Tras generar, vale la pena un `node --check` sobre el `<script>` para confirmar que el
  strip no rompió sintaxis. Para verificar que ambos archivos coinciden: regenera a un
  temporal y haz `diff` contra el `.h` del repo (deben salir idénticos).
- **`HTML/mainForm.preview.html` es el otro artefacto generado** (`scripts/gen_preview.py`):
  el mismo portal con un mock de `fetch` inyectado antes de `</head>`, que responde los 8
  endpoints con datos simulados y sin validar token. Igual que el `.h`, **no se edita a
  mano**: se regenera desde el fuente. El escenario simulado se controla con las banderas del
  bloque `MOCK.params` del propio archivo generado.

---

## Hardware (ESP32-C3, ver `Constants.h`)
- LED blanco → GPIO 0 (salida digital, lógica directa: nivel alto enciende), LED azul → GPIO 1 (canal 1), LED rojo → GPIO 2 (canal 2)
- Buzzer → GPIO 3, ventilador → GPIO 7, bomba de agua → GPIO 10
- PWM: 1 kHz, 8 bits (duty 0–255) en azul y rojo; se envían como 0–100 % y se escalan internamente. El blanco es digital: el portal manda `0`/`1` y el firmware evalúa `> 0`.
- **Jerarquía de iluminación (producto):** el hardware principal contempla **una sola lámpara, la blanca**; los espectros azul y rojo son un extra para el cultivador avanzado, no el control primario. El portal refleja esto en la vista `edit`: la **Luz Blanca** es una tarjeta destacada al frente (la "lámpara principal") y **Azul/Rojo** viven en un bloque colapsable **"Espectros avanzados (opcional)"** dentro de la misma sección *Iluminación*. Se auto-expande si el cultivo ya tiene azul o rojo > 0. Es solo presentación: los tres campos (`ledAzul`/`ledRojo` sliders 0–100, `ledBlanco` toggle 0/1) conservan su contrato con el firmware.
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

## Flujo de la aplicación (página única)

Todo vive en **un solo HTML** (`HTML/mainForm.html`, servido como `mainForm.h`). No hay página de registro aparte;
el formulario es una máquina de estados que decide qué mostrar según el dispositivo.

Al cargar, el `#dashboard` arranca **oculto** (`display:none`): el cliente hace
`GET /getparams` (adjuntando `?token=` si guardó uno) y, según la respuesta, revela directo
el panel correcto (sin parpadear datos antes de tiempo ni transiciones falsas). El JSON
espeja las llaves del formulario (`planta`, `fpOn`, `ledAzul`…), así que el front las usa tal
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

### Los estados, en pantalla

<table>
  <tr>
    <th><code>welcome</code></th>
    <th><code>register</code></th>
    <th><code>view</code></th>
  </tr>
  <tr>
    <td><img src="./img/portal-welcome.png" alt="Estado welcome" width="200"/></td>
    <td><img src="./img/portal-register.png" alt="Estado register" width="200"/></td>
    <td><img src="./img/portal-dashboard.png" alt="Estado view (dashboard)" width="200"/></td>
  </tr>
  <tr>
    <td>Dispositivo nuevo: invita a crear usuario o salir.</td>
    <td>Alta de credenciales; al guardar emite token y entra ya autenticado.</td>
    <td>Dashboard de solo lectura con el estado del cultivo.</td>
  </tr>
  <tr>
    <th><code>auth</code></th>
    <th><code>edit</code></th>
    <th><code>wifi</code></th>
  </tr>
  <tr>
    <td><img src="./img/portal-auth.png" alt="Estado auth" width="200"/></td>
    <td><img src="./img/portal-edit.png" alt="Estado edit" width="200"/></td>
    <td><img src="./img/portal-wifi.png" alt="Estado wifi" width="200"/></td>
  </tr>
  <tr>
    <td>Login para desbloquear la edición. Se salta si el token sigue vigente.</td>
    <td>Formulario editable; aplica en vivo sin reiniciar.</td>
    <td>Escaneo de redes y conexión del dispositivo a la Wi-Fi del usuario.</td>
  </tr>
</table>

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

Son **dos operaciones distintas**, y separarlas fue una decisión de diseño: antes el
`**reset**` cargaba con los dos propósitos y por eso resultaba contradictorio —una acción
que se hace cada cosecha no puede exigir las mismas garantías que una escotilla de
emergencia, ni borrar las mismas cosas.

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
2. Se persiste a NVS (namespaces `system` y `plantData`) para sobrevivir cortes de luz.
3. Se fija la hora del navegador en el RTC (`setCurrentTime()`).
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
todo **de inmediato**, sin reiniciar.

> Hasta esta versión `systemEnable` se guardaba, se reportaba en `/getparams` y se
> imprimía como `ACTIVO/INACTIVO`, pero **nunca se consultaba**: el cultivo seguía
> operando aunque la UI dijera "INACTIVO". La edad del cultivo (`dia`/`semana`) **sí**
> sigue avanzando con el sistema desactivado, porque se deriva del calendario.

Luego el `loop()`, cada `deviceUpdateInterval`, refresca `_currentTime` desde el RTC
(`getCurrentTime()`) y re-aplica `turnOnDevices()`. **Todo corre en un único task
(`loopTask`)** — I²C, handlers HTTP y el log del estado cada `systemLogInterval`. Antes el
log vivía en un `printTask` aparte que leía `_currentTime`/`_systemStatus` mientras este
loop los reescribía: sin sincronización podía imprimir un arreglo a medio actualizar. Se
eliminó esa tarea en lugar de añadir un mutex, así la carrera desaparece por construcción
y se liberan sus 2 KB de stack. Tras un corte de luz, `begin()` recarga `_systemStatus`
desde NVS y el loop lo re-aplica en segundos. El reboot anterior era **redundante** para
aplicar la config; solo `**reset**` reinicia ahora.

---

## Portal V2 (en desarrollo, aún no homologado)

Existe una reescritura del portal en `HTML/portal-v2/mainForm.html`. **Todavía NO
es la versión oficial:** el dispositivo sigue sirviendo la V1 (`HTML/mainForm.html`
→ `mainForm.h`). La V2 vive aparte como borrador y no está en el artefacto servido.

**Qué cambia respecto a la V1:**
- **Navegación hub-and-spoke** en vez de pasos lineales: el dashboard es el centro
  y de ahí se sale a destinos independientes (`edit`, `wifi`, `ota`) y se vuelve.
  `auth` deja de ser un destino y pasa a ser una **compuerta**: solo se cruza si la
  sesión no está vigente, recuerda a dónde ibas (`pendingIntent`) y te deposita ahí.

  La compuerta (`requireAuth(intent)`) **refresca `/getparams` antes de decidir**, en vez
  de fiarse de la copia en memoria: con el TTL fijo de 30 min, `sessionValid` se queda
  obsoleto con facilidad y sin el refresco se entraría a editar para descubrir el 401 al
  guardar. Y `sesionInvalida(mensaje, intent, conservarFormulario)` acepta un tercer
  argumento que evita repoblar el formulario al reentrar, para no borrar lo que el usuario
  había escrito (ver [Manejo del 401](API.md#manejo-del-401-en-el-cliente)). Los cuatro
  flujos protegidos —`edit`, `wifi`, `ota` y `/newcrop`— pasan por la misma compuerta.
- **Estados:** `welcome · register · auth · view · edit · wifi · ota · flashing · exit`
  (la V1 no tenía `ota`, `flashing` ni `exit` como vista propia).
- **Navegación del dashboard como fila de íconos.** Los cuatro destinos hub→spoke
  (`edit`, `wifi`, `ota`, `exit`) pasaron de botones de texto de ancho completo —que en
  móvil envolvían a dos filas— a una sola fila de **íconos SVG con etiqueta corta**
  (`Editar · Wi-Fi · Firmware · Salir`), en el contenedor `.dash-nav`. Los SVG son inline,
  de solo trazo (`stroke="currentColor"`, sin relleno), así que pesan poco y heredan el
  color de la paleta activa.

  - **OTA vuelve a la fila principal.** En la V1 el acceso a OTA se apartaba en un
    `footer` discreto ("la prominencia sigue la frecuencia de uso"), para que no se pulsara
    por inercia. En la V2 se decidió lo contrario: como los cuatro destinos pasan por la
    **misma compuerta de login** (`requireAuth`) y OTA además pide confirmación antes de
    flashear, la barrera real ya no es esconderlo. Se deja en la fila con un matiz cálido
    (`--secondary`) que lo distingue como acción de riesgo sin sacarlo de sitio.
  - **El estado del Wi-Fi no se pierde.** La etiqueta visible solo dice "Wi-Fi", así que el
    estado (conectado/sin conectar) se refleja en un **punto indicador** sobre el ícono
    (`#wifiDot`, verde/gris) y en el `aria-label` del botón. Se conserva el `<span id="wifiState" hidden>`
    para no romper el JS que ya lo actualizaba (`actualizarDashboard`); los ids de los cuatro
    botones (`btnEdit/btnWifi/btnOta/btnExit`) se mantuvieron intactos, así que la migración
    fue solo de markup + CSS, sin tocar los handlers.
  - **Accesibilidad:** cada botón lleva `aria-label`, los SVG van `aria-hidden`, y la etiqueta
    de texto queda visible bajo el ícono (no se depende solo de la forma).
- **CSS con paletas intercambiables.** El markup se construyó primero con hooks
  (`.view`, `.field`, `.dash-section`, `.dash-item`, `.help`, `.actions`, `.dash-nav`, `.msg`,
  `.net-list`, `.crop-reset`) y la hoja se añadió encima **sin tocar el JS**. Está
  organizada en dos capas: los colores viven aislados como **variables CSS** en cuatro
  bloques de paleta, y los ~59 selectores de componente referencian **solo variables**
  — ningún color literal. Cambiar de paleta es editar un atributo:

  ```html
  <html lang="es" data-theme="verde">   <!-- verde · oscuro · tierra · limpio -->
  ```

  Las cuatro paletas definen el **mismo juego de 18 variables** de color, así que
  ninguna queda a medias heredando un color de otra. La forma (`--radius`, `--gap`,
  `--font`) va en un bloque aparte común a todas: la paleta cambia el color, no el ritmo.

  > **Invariante:** el cambio de vista usa el atributo `hidden`, no clases, y lo sostiene
  > `[hidden] { display: none !important; }`. Si una regla de `.view` ganara especificidad
  > sobre el atributo, **las ocho pantallas se verían a la vez**. Ninguna regla de `.view`
  > debe tocar `display`.
- **Textos de ayuda** con `<details>/<summary>` nativos en la vista de edición.
- **Banner de solo lectura** (`#dashAuthHint`): el dashboard no exige sesión (leer es
  libre); el banner recuerda que para *editar* hay que iniciar sesión. Solo se
  muestra cuando no hay sesión vigente (`getToken() && sessionValid`).

**Qué le falta para homologarse:**
1. ~~**CSS**~~ — **ya implementado** (ver arriba): hoja con paletas intercambiables por
   `data-theme`. Queda elegir la definitiva y, si se quiere, recortar las no usadas.
2. **Endpoint OTA en el firmware** (`POST /otaupdate`, ver abajo): la vista `ota`
   sube el `.bin` por `multipart`, pero el firmware aún no expone la ruta.
3. ~~**`firmwareVersion` en `/getparams`**~~ — **ya implementado**: el firmware lo
   expone desde la constante de compilación `firmwareVersion` (`Constants.h`).
4. **Promover** `portal-v2/mainForm.html` → `HTML/mainForm.html`, regenerar
   `mainForm.h` y actualizar esta doc (la máquina de estados cambia).

**Cómo probarla sin dispositivo:** hay una copia con firmware simulado en
`HTML/portal-v2/mainForm.preview.html` (mock de `fetch`/`XHR`). Se abre directo en
el navegador; arranca en el dashboard sin login y simula Wi-Fi, guardado y OTA.
**No se sirve desde el ESP32 ni se regenera a `mainForm.h`** — es solo para revisar
UI/UX. La fuente de verdad de la V2 es `mainForm.html` (sin mock). Editar
`MOCK.params` dentro del `.preview.html` cambia el escenario (p. ej.
`hasRegisteredUser:false` → arranca en `welcome`).

---

## Endpoints HTTP
Ver el detalle de payloads, validaciones y catálogo de errores en **[`API.md`](API.md)**.

| Método | Ruta                  | Handler                  | Propósito |
|--------|-----------------------|--------------------------|-----------|
| GET    | `/`                   | `handleRoot`             | Sirve el HTML único (`mainForm`) vía `send_P` (directo desde flash, sin copiarlo a un `String` de ~73 KB en cada request) |
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
- **Frecuencia/duración coherentes:** si `irrH`/`ventH` > 0, la duración correspondiente
  debe ser ≥ 1 min. Con duración 0 el dispositivo nunca encendería (`manageDevice` exige
  `minuto < duración`) pero la UI lo mostraría como configurado; para desactivar existe
  la frecuencia `0`. La regla es asimétrica: frecuencia 0 con duración > 0 sí se acepta,
  para conservar el valor mientras está apagado.

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
- Reusa `daysSinceEpoch()` del control de riego. Los enumeradores `cropDay`/`cropWeek`
  **se eliminaron** de `SystemStatus`: eran los dos últimos índices y ya no se leían ni
  escribían. `_systemStatus` pasó de 15 a 13 bytes, dimensionado por el centinela
  `systemStatusCount` para que el arreglo siga al enum automáticamente. Como el blob de
  NVS cambia de tamaño, al cargar este firmware hay que **borrar la flash** (es el
  procedimiento habitual del proyecto): `Preferences::getBytes` no copia nada si lo
  guardado es más grande que el buffer, así que un blob viejo se leería en ceros.

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
parte del número — `0x00` segundos (bit 7), `0x02` horas (bits 6–5, modo 12/24 h) y
`0x05` mes (bit 7, *century*) — para que un bit alto no corrompa la conversión.

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

## Propuesta: servir el HTML comprimido (gzip) para mayor performance

Hoy el HTML se embebe como texto (`mainForm.h`, ~73 KB tras quitarle los comentarios).
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
- El AP `SmartPlant` usa **WPA2-PSK** (`sensible.h`), así que el enlace va cifrado; aun así
  **no hay TLS** dentro del enlace. El token añade UX y cierra el hueco de reenviar
  credenciales en claro a `/newparams`, pero la confidencialidad real la aporta el WPA2: con
  una passphrase compartida y conocida, alguien en la misma red podría capturar el token
  (de ahí la recomendación de contraseña por dispositivo en `sensible.h`).
- **El portal se sirve SOLO por el AP.** `WebServer server(apGatewayIp, 80)` liga el socket
  a la IP del SoftAP (`192.168.4.1`) en vez de al wildcard, así que **ningún endpoint
  responde por la interfaz STA**. Es la decisión de fondo: el portal es el plano de control
  *local*, y el enlace con el backend será **saliente**, iniciado por el dispositivo.

  Antes, con `WebServer server(80)`, el socket se ligaba a `0.0.0.0` —el propio comentario de
  `NetworkServer::begin()` lo dice: *"leave it all-zero so the socket binds to the wildcard
  (listen on every interface)"*— y en cuanto el equipo se unía a la red del usuario, **todo**
  el portal quedaba alcanzable desde esa LAN: login, edición de parámetros, configuración de
  red y el comando de reset, sin necesidad de la passphrase del AP. Un escaneo del `/24`
  buscando el puerto 80 lo encontraba.

  | | Antes | Ahora |
  |---|---|---|
  | Vía AP (`192.168.4.1`) | Sí | Sí |
  | Vía red del usuario (STA) | **Sí** | **No** |

  **Precio aceptado:** no se puede abrir el dashboard desde el celular estando en la Wi-Fi de
  casa; hay que cambiarse a la red `SmartPlant`. A cambio, el modelo de amenaza vuelve a ser
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
- El token se genera solo en login/registro (no por request), evitando escrituras innecesarias.

### Huecos de seguridad identificados y su resolución

Los tres huecos identificados están **resueltos**. Eran **interdependientes**: el límite de
intentos podía dejar al usuario fuera del equipo si se implementaba sin resolver antes el
reset, y por eso se hicieron en ese orden.

#### 1. Contraseña de usuario (resuelto)

La contraseña ya **no se guarda ni se conserva en RAM**: en su lugar viven un salt aleatorio
y la clave derivada con **PBKDF2-HMAC-SHA256** (`config/pwSalt` y `config/pwHash`, en hex).
El buffer `_userpass` desapareció, igual que `maskPassword()`, y el log serie ya no imprime
nada de la contraseña.

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

Se resolvió **sin exigir autenticación**, que era la vía planteada originalmente y resultó
ser la equivocada: el reset existe para cuando no se puede iniciar sesión, así que pedirle
token lo habría inutilizado.

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
  lista. Antes bloqueaba ~2 s dentro del request y congelaba el portal. Queda el límite
  físico de una sola antena: durante el escaneo el AP puede perder algún paquete.
- **Sin tráfico saliente todavía:** el firmware no abre ninguna conexión a un
  backend. Por construcción, un dispositivo **no hace peticiones inútiles**.

---

## Modelado de la base de datos (backend futuro)

> **Esta sección es el origen histórico del diseño; la referencia vigente es
> [`BACKEND.md`](BACKEND.md).** Se conserva porque explica el *lado firmware* (qué manda el
> dispositivo y por qué). Para nombres de tabla, columnas y payloads exactos, usa
> `BACKEND.md`: ahí `accounts` se llama **`users`** y se identifica por **`email`**.

Diseño de referencia para cuando se agregue el backend Python. Persiste la
configuración/estado de cada dispositivo para que **otros dispositivos o apps de
la misma cuenta** puedan consultarlos. Modelo **por cuenta**: una cuenta posee sus
dispositivos y sus datos.

### Identidad y autenticación (resumen)
- El dispositivo se autentica **una vez** con `email + pass + mac` sobre **TLS**.
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
| `users` (aquí `accounts`) | `id`, `email` (UNIQUE), `password_hash`, `is_admin`, `created_at` | Cuenta dueña de los dispositivos. Hash con bcrypt/argon2 |
| `devices` | `id`, `mac` (UNIQUE), `account_id` (FK), `name`, `token_hash`, `firmware_version`, `last_seen_at`, `created_at` | La MAC identifica; `token_hash` = token de dispositivo hasheado (revocable) |
| `device_configs` | `id`, `device_id` (FK), `planta`, `enable`, `fp_on`, `fp_off`, `led_a/r/b`, `irr_h/m`, `vent_h/m`, `crop_start_day`, `applied_at` | **Una fila por cambio** (histórico). La última = config vigente. Espeja los campos de `/newparams` en **snake_case**; ojo: los LED se llaman `led_a`/`led_r`/`led_b` aquí, mientras el portal usa `ledAzul`/`ledRojo`/`ledBlanco` (ver [nota](#desfase-de-nombres-entre-portal-y-backend)) |
| `device_telemetry` | `id`, `device_id` (FK), `ts`, `wifi_rssi`, `uptime`, *(sensores futuros: temp, humedad…)* | **Serie temporal**: candidato a hypertable de **TimescaleDB** |
| `firmware_releases` | `version`, `url`, `sha256`, `signature`, `min_version`, `published_at` | Catálogo para OTA (ver sección TLS / OTA) |

#### Desfase de nombres entre portal y backend

Los campos de LED **no se llaman igual** en los dos contratos:

| Portal ↔ firmware (`/newparams`, `/getparams`) | Firmware → backend (REST, snake_case) |
|---|---|
| `ledAzul` (0–100) | `led_a` |
| `ledRojo` (0–100) | `led_r` |
| `ledBlanco` (0/1) | `led_b` |

El backend ya define `led_a/led_r/led_b` en `models.py`, `schema.sql`, la migración `001_initial_schema.py` y los routers (`devices.py` ya valida `led_b` como `ge=0, le=1`, igual que el firmware). Las claves del portal se renombraron después, así que **cuando se implemente el envío de configuración al backend habrá que mapear explícitamente** `ledAzul→led_a`, `ledRojo→led_r`, `ledBlanco→led_b`, o bien renombrar las columnas con una migración nueva.

Hoy no hay incompatibilidad activa: el firmware todavía no envía configuración al backend (`downloadOTA()`/`getToken()` siguen sin implementar). Es una decisión pendiente, no un bug.

### Flujos de información

1. **Aprovisionamiento** (acción del usuario): device → `POST /devices/provision
   {email,pass,mac}` → el backend valida la cuenta, crea/vincula la fila en
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

## OTA local (subir el `.bin` desde el teléfono) — pendiente en firmware

Caso de uso: el usuario tiene el binario en su teléfono y lo **sube al dispositivo
por la red del AP**, sin internet ni nube. Útil cuando el equipo está en un lugar
de difícil acceso. **No confundir con la "OTA segura" sobre TLS/Internet** (sección
anterior): esta es local, por HTTP dentro del enlace WPA2, y sin firma.

**Estado:** el **lado cliente ya está** en la V2 (vistas `ota`/`flashing`: input de
archivo, subida `multipart` por `XMLHttpRequest` con barra de progreso). **Falta el
endpoint en el firmware:** `POST /otaupdate` **no existe todavía**.

### Requisito CRÍTICO: tabla de particiones con dos slots OTA
La librería `Update` (y `ArduinoOTA`/`httpUpdate`) escribe el binario nuevo en el
slot **inactivo** mientras el firmware corre desde el activo, y al validar cambia el
arranque en `otadata`. **Esto exige una tabla de particiones con `ota_0` + `ota_1` +
`otadata`.** No es opcional: sin dos slots, `Update` de la app falla. Escribir sobre
el slot en ejecución dejaría el equipo **inservible** ante un corte a media escritura.

- Esquemas que **SÍ** sirven: *Default 4MB with spiffs* (~1.2 MB/slot),
  *Minimal SPIFFS (1.9MB APP with OTA)* (~1.9 MB/slot).
- Esquemas que **NO** sirven: *Huge APP (3MB No OTA)*, *Minimal (No OTA)* — un solo
  slot de app, sin OTA.

> **Reproducibilidad:** hoy el proyecto **no** declara esquema de particiones (ni un
> `.csv` en el sketch, ni `build.partitions` en `ci.json`): depende del menú
> *Tools → Partition Scheme* del Arduino IDE, ajuste manual que no queda versionado.
> Antes de implementar la OTA conviene **fijar un `partitions.csv`** en el sketch y
> declararlo, para que el slot OTA no dependa de recordar un ajuste del IDE (en otra
> máquina/Ubuntu la OTA fallaría de forma silenciosa). Verificar además que el `.bin`
> (AP+STA + WebServer + portal + `Update`) cabe en el slot elegido.

### Diseño previsto del endpoint `POST /otaupdate`
1. **Auth por token en la query** (`?token=`): el cuerpo es `multipart`, no JSON.
   Validar el token **al inicio** de la subida y abortar en el primer trozo si es
   inválido, para no recibir ~1 MB antes de rechazar (`401`).
2. **Escritura en streaming** con `Update.begin(UPDATE_SIZE_UNKNOWN)` → `Update.write()`
   por trozos → `Update.end(true)`. No requiere el binario completo en RAM.
3. **Integridad:** `Update` valida el *magic byte* del firmware ESP32 (rechaza un
   archivo que no sea firmware). Opcional: que el cliente mande tamaño/MD5 para
   detectar corrupción de transporte.
4. **Reinicio** al terminar; el AP parpadea (radio única) y el cliente reconecta —
   la V2 ya avisa "no cierres la ventana" y maneja el post-reinicio.
5. **Sin firma criptográfica:** acepta cualquier `.bin` válido de quien tenga sesión
   y esté en el AP. Aceptable para subir el propio binario; la firma es cosa de la
   OTA segura por Internet, no de esta local.

`firmwareVersion` ya se expone en `/getparams` (constante de compilación en
`Constants.h`), así que la vista OTA puede mostrar la versión instalada. **Hay que
subir ese valor en cada release que se distribuya por OTA.**

---

## Caveats conocidos
- **`StaticJsonDocument` ajustado.** `/newparams` recibe ~20 claves; el buffer se subió a
  1024 B para evitar `NoMemory` con credenciales/planta largas.
- La fuente de verdad es `HTML/mainForm.html` (comentado); `mainForm.h` es el artefacto
  generado sin comentarios que sirve el dispositivo. No editar el `.h` a mano.
