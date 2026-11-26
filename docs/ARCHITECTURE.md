# SmartPlant — Controlador ESP32 (firmware + portal captivo)

Firmware para un **ESP32-C3** que controla un cultivo (iluminación LED por espectro,
riego y ventilación) y se configura desde el celular/PC vía un **portal captivo
Wi-Fi**. El dispositivo levanta un Access Point, sirve un formulario HTML embebido y
recibe la configuración por HTTP en JSON.

Este documento cubre el **diseño interno**: por qué el código está escrito así, qué
trade-offs se tomaron y qué caveats quedan abiertos. Para el comportamiento del firmware ver
[`FIRMWARE.md`](FIRMWARE.md); para el contrato de la API,
[`API-device.md`](API-device.md); para pines y periféricos, [`HARDWARE.md`](HARDWARE.md); el índice
completo está en [`README.md`](README.md).

---

## Decisiones de ingeniería (resumen)

Lo interesante de un sistema embebido no es *qué* hace, sino *cómo* resuelve los problemas
que impone el hardware (1 núcleo RISC-V @160 MHz, RAM y flash limitadas). Este proyecto es un
sistema **de extremo a extremo** bajo esas restricciones:

- **Firmware en C++** (ESP32-C3): PWM multicanal, relés, RTC por I²C, persistencia en NVS,
  servidor HTTP y DNS captivo.
- **Frontend embebido**: una SPA completa (máquina de estados) en un solo HTML con vanilla JS,
  servida desde el flash del dispositivo — sin build, sin dependencias.
- **Diseño con trade-offs documentados**: cada decisión no trivial está razonada en este
  documento (enlaces en la tabla).
- **Mentalidad de producto**: UX cuidada, seguridad por capas y un roadmap de backend/OTA
  pensado para el hardware.

Las decisiones más relevantes, cada una con enlace a su detalle:

| Reto | Solución | Detalle |
|------|----------|---------|
| Que el riego sobreviva cortes de luz | Scheduling **stateless**: el encendido se recalcula desde un contador continuo de horas derivado del RTC (`epochHours % intervalo`); cubre riego sub-diario y espaciado de varios días sin un contador que se pierda al apagar. | [Frecuencias](FIRMWARE.md#frecuencias-de-riegoventilación-irrh--venth) |
| Edad del cultivo correcta tras estar apagado | `día`/`semana` se **derivan del calendario real** (ancla `cropStart` en NVS), no de un contador; el cultivo "sigue envejeciendo" sin desgastar flash cada noche. | [Edad del cultivo](FIRMWARE.md#edad-del-cultivo-dia--semana) |
| Conectar a Wi-Fi sin congelar el portal | Conexión STA **no bloqueante** + **polling** desde el front: el `POST` arranca el intento y responde al instante; el cliente confirma vía `/getparams`. | [Conexión no bloqueante](FIRMWARE.md#conexión-no-bloqueante--polling) |
| No guardar una contraseña Wi-Fi que no sirve | **Persistencia diferida**: las credenciales se escriben en NVS *solo* al llegar a `WL_CONNECTED`. | [Persistencia diferida](FIRMWARE.md#persistencia-diferida-solo-si-conecta) |
| Autenticación sin reenviar credenciales | **Token de sesión** de 128 bits (`esp_random()`), TTL fijo en RAM (muere con el reboot, expiración por `millis()` a prueba de wrap-around). AP cifrado con WPA2-PSK. | [Sesión con token](FIRMWARE.md#sesión-persistente-con-token-implementada) |
| Validación idéntica en navegador y dispositivo | Reglas replicadas bit a bit front↔firmware, contando por **carácter UTF-8** (no bytes), para que acentos y ñ no descuadren los límites entre JS y C++. | [Reglas de validación](#reglas-de-validación-deben-coincidir-front--firmware) |
| Servir el portal sin agotar el heap | `send_P` entrega el portal (~16 KB gzip) directo desde flash; evita el pico de un `String` temporal que podía fallar en AP+STA. | [gzip](#servir-el-html-comprimido-gzip) |
| Menos flash y carga más rápida | El HTML fuente se regenera sin comentarios y **comprimido con gzip** al artefacto del ESP32 (de ~93 KB a ~16 KB, −83 %); `handleRoot` lo sirve con `Content-Encoding: gzip`. | [gzip](#servir-el-html-comprimido-gzip) |
| Seguridad del portal | Contraseña con **PBKDF2-HMAC-SHA256**, reset de fábrica restringido a la interfaz del AP y **límite de intentos** de login. | [Huecos de seguridad](FIRMWARE.md#huecos-de-seguridad-identificados-y-su-resolución) |

> Para el estado de cada capacidad (qué está verificado, en curso o planeado) ver
> [ROADMAP.md § Estado actual](ROADMAP.md#estado-actual).

---

## Estructura

```
ESP32_controller/
  ESP32_controller.ino     # Punto de entrada: AP Wi-Fi, DNS captivo, rutas HTTP y handlers
  Plant.h / Plant.cpp # Lógica del cultivo: validación de payloads, persistencia, PWM/relés, RTC, sesión
  Constants.h         # Pines, canales PWM, límites de validación, TTL de sesión, enums de estado y de error
  sensible.h          # *** Secretos del AP (apSsid / apPassword) — NO versionar, copiar por equipo ***
  mainForm.h          # *** Artefacto GENERADO (HTML sin comentarios, gzip, arreglo de bytes PROGMEM) que sirve el ESP32 — no editar a mano ***
  ci.json             # Config de build (FQBN / placa)
HTML/
  mainForm.html       # *** Fuente de verdad: HTML legible y comentado del formulario — edita AQUÍ ***
```

### Convención importante (leer antes de tocar formularios)
- **Edita siempre la fuente HTML** (legible y comentada), nunca el artefacto. La pista
  activa (la que sirve el dispositivo) es `HTML/mainForm.html`; la próxima ronda de
  rediseño irá en `HTML/dev/mainForm.html`, y el portal anterior quedó archivado en
  `HTML/deprecated/mainForm.html`.
- **`ESP32_controller/mainForm.h` es un artefacto generado**: el HTML (sin comentarios de
  línea completa) **comprimido con gzip** y embebido como arreglo de bytes en PROGMEM
  (`mainForm_gz` + `mainForm_gz_len`). Es lo que el dispositivo sirve; **no se edita a mano**.
  Se produce con `python3 HTML/scripts/gen_mainform.py` (estable) o `--dev` (desarrollo).
- **gzip es el ahorro real**: el HTML (~93 KB) baja a **~16 KB (−83 %)** en el `.h`. (Quitar
  comentarios por sí solo ahorraba <1 %; el peso lo quita gzip.) El navegador descomprime
  solo; el firmware únicamente declara `Content-Encoding: gzip` en `handleRoot`.
- Generar el `.h` desde el `.html`: strip de comentarios HTML (`<!-- -->`), CSS (`/* */`)
  y JS (`//`) → **gzip -9 determinista** (`mtime=0`) → arreglo de bytes PROGMEM. Tras generar,
  vale la pena un `node --check` sobre el `<script>` del fuente para confirmar que el strip no
  rompió sintaxis. Verificación de integridad: descomprimir el `.gz` del `.h` debe devolver
  exactamente el HTML limpio.
- **`HTML/mainForm.preview.html` es el otro artefacto generado** (`HTML/scripts/gen_preview.py`):
  el mismo portal con un mock de `fetch` inyectado antes de `</head>`, que responde los 8
  endpoints con datos simulados y sin validar token. Igual que el `.h`, **no se edita a
  mano**: se regenera desde el fuente. El escenario simulado se controla con las banderas del
  bloque `MOCK.params` del propio archivo generado.

### Flujo de trabajo del portal (regenerar y promover)

El formulario tiene **una pista activa** (la que sirve el dispositivo) y los artefactos se
regeneran de su fuente, **nunca al revés**:

- **Activa** (sirve el ESP32): `HTML/mainForm.html`
- **Desarrollo** (próxima ronda de rediseño, aún no activa): `HTML/dev/mainForm.html`
- **Archivada** (portal anterior, ya no se usa): `HTML/deprecated/mainForm.html`

```bash
# Previews para revisar en el navegador sin dispositivo (cada uno inyecta su propio
# mock de fetch/XHR que simula el firmware):
python3 HTML/scripts/gen_preview.py      # -> HTML/mainForm.preview.html      (activa)
python3 HTML/scripts/gen_preview_dev.py  # -> HTML/dev/mainForm.preview.html  (desarrollo)

# Regenerar el artefacto embebido que sirve el ESP32 (un solo .h):
python3 HTML/scripts/gen_mainform.py        # -> mainForm.h desde la pista ACTIVA
python3 HTML/scripts/gen_mainform.py --dev  # previsualiza el .h desde DESARROLLO (sin promover)
```

Cuando una ronda de desarrollo se da por buena, se **promueve**: su fuente pasa a
`HTML/mainForm.html`, la anterior baja a `HTML/deprecated/`, y se regenera `mainForm.h`. Si
existe `firmware/main/` (rama `esp-idf-migration`), `gen_mainform.py` regenera también
`firmware/main/mainForm.h` en la misma ejecución, para que ambos firmwares sirvan el mismo
portal.

**Dos bases de firmware y documentación común.** `ESP32_controller/` (Arduino, estable, rama
`main`) y `firmware/` (ESP-IDF, migración, rama `esp-idf-migration`) comparten portal, backend,
hardware y documentación: lo común se edita en `main` y se lleva a `esp-idf-migration` con
`git merge main`; lo propio del firmware ESP-IDF vive en `firmware/README.md`. El plan y el
estado de la migración están en [`ROADMAP.md`](ROADMAP.md).

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

Estados del front: `welcome` · `register` · `view` · `auth` · `edit` · `wifi` · `ota` · `flashing`.
Los estados **`wifi`** (config de red) y **`ota`** (subida de firmware) se entran desde una
**fila de dos chips** en el dashboard (Wi-Fi y "Actualizar firmware"), encima de los botones
Editar/Salir, y están gateados por sesión igual que `edit` (ver "Conexión Wi-Fi del
usuario" y "OTA local").

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
  <tr>
    <th><code>ota</code></th>
    <th><code>flashing</code></th>
    <th></th>
  </tr>
  <tr>
    <td>Subida del firmware <code>.bin</code> desde el dispositivo por el AP, sin internet. Pasa por la compuerta de login como <code>edit</code>/<code>wifi</code>.</td>
    <td>Estado <strong>bloqueante</strong> (sin botones de salida) mientras se escribe la flash: barra de progreso de la subida; al terminar el equipo se reinicia y cae a <code>exit</code>.</td>
    <td></td>
  </tr>
</table>

---

## El portal (hub-and-spoke)

El portal que sirve el dispositivo es una SPA hub-and-spoke (fuente:
`HTML/mainForm.html`, artefacto `mainForm.h`). Reemplazó al portal lineal anterior,
que se conserva solo como referencia en `HTML/deprecated/mainForm.html` (ya no se
genera nada de él). La próxima ronda de rediseño, cuando exista, vivirá en
`HTML/dev/`.

**Modelo de navegación:**
- **Navegación hub-and-spoke**: el dashboard es el centro
  y de ahí se sale a destinos independientes (`edit`, `wifi`, `ota`) y se vuelve.
  `auth` deja de ser un destino y pasa a ser una **compuerta**: solo se cruza si la
  sesión no está vigente, recuerda a dónde ibas (`pendingIntent`) y te deposita ahí.

  La compuerta (`requireAuth(intent)`) **refresca `/getparams` antes de decidir**, en vez
  de fiarse de la copia en memoria: con el TTL fijo de 30 min, `sessionValid` se queda
  obsoleto con facilidad y sin el refresco se entraría a editar para descubrir el 401 al
  guardar. Y `sesionInvalida(mensaje, intent, conservarFormulario)` acepta un tercer
  argumento que evita repoblar el formulario al reentrar, para no borrar lo que el usuario
  había escrito (ver [Manejo del 401](API-device.md#manejo-del-401-en-el-cliente)). Los cuatro
  flujos protegidos —`edit`, `wifi`, `ota` y `/newcrop`— pasan por la misma compuerta.
- **Estados:** `welcome · register · auth · view · edit · wifi · ota · flashing · exit`
  (el portal lineal anterior no tenía `ota`, `flashing` ni `exit` como vista propia).
- **Navegación del dashboard como fila de íconos.** Los cuatro destinos hub→spoke
  (`edit`, `wifi`, `ota`, `exit`) pasaron de botones de texto de ancho completo —que en
  móvil envolvían a dos filas— a una sola fila de **íconos SVG con etiqueta corta**
  (`Editar · Wi-Fi · Firmware · Salir`), en el contenedor `.dash-nav`. Los SVG son inline,
  de solo trazo (`stroke="currentColor"`, sin relleno), así que pesan poco y heredan el
  color de la paleta activa.

  - **OTA vuelve a la fila principal.** En el portal anterior el acceso a OTA se apartaba en un
    `footer` discreto ("la prominencia sigue la frecuencia de uso"), para que no se pulsara
    por inercia. Aquí se decidió lo contrario: como los cuatro destinos pasan por la
    **misma compuerta de login** (`requireAuth`) y OTA además pide confirmación antes de
    flashear, la barrera real ya no es esconderlo. Los cuatro íconos comparten el mismo
    estilo (sin recuadro, color `--text-dim` que vira a `--primary` al pasar/enfocar).
  - **El estado del Wi-Fi no se pierde.** La etiqueta visible solo dice "Wi-Fi", así que el
    estado (conectado/sin conectar) se refleja en el **color del propio ícono SVG**
    (`#wifiIco.is-on`, con el color primario de la paleta cuando hay conexión) y en el
    `aria-label` del botón. Se conserva el `<span id="wifiState" hidden>`
    para no romper el JS que ya lo actualizaba (`actualizarDashboard`); los ids de los cuatro
    botones (`btnEdit/btnWifi/btnOta/btnExit`) se mantuvieron intactos, así que la migración
    fue solo de markup + CSS, sin tocar los handlers.
  - **Accesibilidad:** cada botón lleva `aria-label`, los SVG van `aria-hidden`, y la etiqueta
    de texto queda visible bajo el ícono (no se depende solo de la forma).
- **CSS con paletas intercambiables.** El markup se construyó primero con hooks
  (`.view`, `.field`, `.dash-block`/`.dash-card`, `.dash-hero`, `.dash-phrase`,
  `.dash-spectrum`/`.dash-dot`, `.help`, `.actions`, `.dash-nav`/`.nav-item`, `.msg`, `.toast`,
  `.net-list`, `.crop-reset`) y la hoja se añadió encima **sin tocar el JS**. Está organizada
  en dos capas: los colores viven aislados como **variables CSS** por paleta (18 variables cada
  una), y los selectores de componente referencian **solo variables** — ningún color literal.
  Cambiar de paleta es editar un atributo:

  ```html
  <html lang="es" data-theme="tierra">   <!-- tierra (claro, embebida hoy) · invernadero · slate · verde-oscuro -->
  ```

  Cada paleta define el **mismo juego de 18 variables** de color, así que
  ninguna queda a medias heredando un color de otra. Hoy el portal embebe **tierra**
  (clara); las demás candidatas —invernadero y slate (claras) y **verde-oscuro**
  (oscura, verde)— viven en `HTML/dev/paletas-guardadas.css` y se activan cambiando
  el `data-theme`.

  > **Invariante:** el cambio de vista usa el atributo `hidden`, no clases, y lo sostiene
  > `[hidden] { display: none !important; }`. Si una regla de `.view` ganara especificidad
  > sobre el atributo, **las ocho pantallas se verían a la vez**. Ninguna regla de `.view`
  > debe tocar `display`.
- **Banner de solo lectura** (`#dashAuthHint`): el dashboard no exige sesión (leer es
  libre); el banner recuerda que para *editar* hay que iniciar sesión. Solo se
  muestra cuando no hay sesión vigente (`getToken() && sessionValid`).
- **Modelo de notificaciones (tres canales, por tipo de mensaje):**
  - **Confirmaciones → toast efímero** (`mostrarToast`, flota abajo y se desvanece).
  - **Errores/atención → banner persistente** (`showMsg`/`clearMsg`, `#msg` in-flow con
    `[data-ok]`); `setEstado` llama `clearMsg()` al cambiar de vista para que un mensaje no
    quede colgado en otra pantalla (quien deba mostrar un mensaje al entrar a una vista llama
    `showMsg` *después* de `setEstado`).
  - **Validación por campo → field-error**: los validadores devuelven errores etiquetados
    `{f, m}`; `pintarErroresCampos` marca el input (`.input-invalid` + `aria-invalid`) y
    cuelga un `<small class="field-error">` bajo el campo, que se limpia al editarlo. Los
    `<form>` llevan `novalidate` para que valide el JS, no el navegador. No se usan `alert()`.

**Estado y mantenimiento:**
- **CSS / estilos**: hoja completa con **paletas intercambiables** (hoy embebe la
  clara *tierra*). Están estilizados contenedor, tipografía, campos
  (inputs/selects/sliders/toggles y el desplegable de espectro avanzado), botones
  (`.primary`/`.secondary`/`.crop-reset`), dashboard (hero, secciones, ítems y la
  fila de íconos sin recuadro), notificaciones (toast + banner + validación por
  campo) y los componentes de wifi/ota/flashing. Todos los selectores referencian
  **solo variables** de paleta. Pendiente menor: repaso visual fino/responsive. Más
  paletas para probar en `HTML/dev/paletas-guardadas.css`.
- **Endpoint OTA en el firmware**: `POST /otaupdate` (Arduino con la librería
  `Update`; ESP-IDF con `esp_ota_ops`), doble slot vía `partitions.csv`. El cliente
  sube el `.bin` por `multipart` con el token en el header `Authorization: Bearer`.
- **`firmwareVersion` en `/getparams`**: el firmware lo expone desde la constante de
  compilación `firmwareVersion` (`Constants.h`).
- **Modelo de pistas (una sola activa).** La fuente del portal servido es
  `HTML/mainForm.html`; su artefacto es `mainForm.h`. El portal lineal anterior
  quedó en `HTML/deprecated/` (solo referencia). La próxima ronda de rediseño se
  trabajará en `HTML/dev/mainForm.html` y, al terminar, se **promueve**: su fuente
  pasa a `HTML/mainForm.html`, la anterior baja a `HTML/deprecated/`, y se regeneran
  los artefactos con `python3 HTML/scripts/gen_mainform.py`. `--dev` solo previsualiza
  la pista de desarrollo sin tocar la estable. Al promover hay que actualizar esta doc
  (la máquina de estados y el portal servido cambian).

**Cómo probarlo sin dispositivo:** la pista activa tiene su preview con firmware
simulado (`HTML/mainForm.preview.html`), que se **genera** desde su fuente con
`HTML/scripts/gen_preview.py`, inyectando un mock de `fetch`/`XHR` antes de `</head>`.
Se abre directo en el navegador; arranca en el dashboard sin login y simula Wi-Fi,
guardado y OTA. La pista de desarrollo (cuando exista) tiene su propio
`HTML/scripts/gen_preview_dev.py`. **El preview no se sirve desde el ESP32 ni se
regenera a `mainForm.h`.** La fuente de verdad es `mainForm.html` (sin mock); editar
`MOCK.params` en el generador cambia el escenario (p. ej. `hasRegisteredUser:false`
→ arranca en `welcome`).

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

## Servir el HTML comprimido (gzip)

> **Estado:** implementado.

El portal se embebe **comprimido con gzip**: el `.h` ya no es texto crudo sino el HTML
gzipeado como arreglo de bytes. Medido: el HTML limpio (~93 KB) baja a **~16 KB en el `.h`
(−83 %)** → menos flash ocupado, menos chunks por el AP y carga más rápida. Los navegadores
descomprimen gzip de forma transparente; el firmware solo declara `Content-Encoding: gzip`.

### Flujo (integrado en `gen_mainform.py`)
La compresión es parte del generador del `.h`, no un paso manual, para que **nunca se
desincronice** del fuente. Un solo comando produce el `.h` de la pista elegida:

```bash
python3 HTML/scripts/gen_mainform.py          # activa -> ESP32_controller/mainForm.h (gzip)
python3 HTML/scripts/gen_mainform.py --dev     # previsualiza desde HTML/dev/ (sin promover)
```

Si existe `firmware/main/` (rama `esp-idf-migration`), el mismo comando regenera también
`firmware/main/mainForm.h` (mismo contenido + preámbulo que define `PROGMEM` vacío), para que
ambos firmwares sirvan siempre el mismo portal.

Internamente: limpia comentarios → `gzip.compress(..., compresslevel=9, mtime=0)` → emite

```cpp
static const unsigned char mainForm_gz[] PROGMEM = { 0x1f, 0x8b, ... };
static const unsigned int  mainForm_gz_len = 16075;   // valor actual; cambia con cada regeneración
```

`mtime=0` hace la salida **determinista** (mismo HTML → mismos bytes; no cambia por el
timestamp que gzip incrusta por defecto), para que el `.h` no genere diffs espurios.

### Servido en `handleRoot`
Al ser binario (contiene bytes nulos) se envía con longitud explícita:

```cpp
void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char*)mainForm_gz, mainForm_gz_len);
}
```

### Consideraciones
- **Se regenera el `.h` en cada cambio de UI** con el mismo generador; mismo riesgo de
  sincronía que ya existía entre el `.h` y el `.html`, pero cubierto por un único comando.
- En producción vive **solo el `.gz`** (no se guarda el crudo en el `.h`); la fuente de
  verdad para depurar es el `.html` legible.
- **No afecta** a las rutas POST (JSON) ni a `/getparams`: solo cambia cómo se entrega el HTML.
- Verificación de integridad: descomprimir el arreglo del `.h` debe devolver exactamente el
  HTML limpio (round-trip), y los bytes deben empezar con el magic gzip `1f 8b`.

---

## Caveats conocidos
- **`StaticJsonDocument` ajustado.** `/newparams` recibe ~20 claves; el buffer se subió a
  1024 B para evitar `NoMemory` con credenciales/planta largas.
- La fuente de verdad es `HTML/mainForm.html` (comentado); `mainForm.h` es el artefacto
  generado sin comentarios que sirve el dispositivo. No editar el `.h` a mano.
