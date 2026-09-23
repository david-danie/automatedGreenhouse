# API HTTP del dispositivo (firmware)

Rutas que expone el **ESP32-C3** desde el portal captivo (AP `SmartPlant`, servidor en el puerto 80). Es la API **local** que consume el navegador del usuario dentro de la red del AP.

**Fuente:** `ESP32_controller/ESP32_controller.ino`, `Plant.cpp`, `Constants.h`.

> No confundir con la API del backend en la nube, que es un contrato distinto y todavía en diseño — ver [`BACKEND.md`](BACKEND.md).

---

## Convenciones

- Las respuestas de escritura usan el sobre `{"status": <bool>, "message": "<texto>"}`.
- Las respuestas de lectura (`/getparams`, `/wifiscan`) devuelven el objeto directo, sin sobre.
- Los `POST` esperan body JSON con `Content-Type: application/json`.
- Códigos: `200` éxito · `400` error de validación · `401` sesión inválida o expirada · `500` error interno.
- **Ninguna ruta reinicia el dispositivo salvo el factory reset.** Los parámetros se aplican en vivo.

## Autenticación

`/newparams`, `/newcrop` y `/wificredentials` exigen un **token de sesión** vigente. El flujo es:

1. `POST /usercredentials` (primer arranque) o `POST /authusercredentials` (login) devuelven un `token`.
2. El token es una cadena de **32 caracteres hex** (128 bits de `esp_random()`).
3. **En el dispositivo** vive **solo en RAM** (`_sessionToken`): un reinicio lo invalida. TTL **fijo de 30 min** desde su emisión — la actividad **no** lo renueva. **En el cliente**, el portal guarda su copia en `localStorage` (`spToken`) para reenviarla; si el dispositivo se reinició, esa copia deja de ser válida y el cliente la descarta al recibir un `401` (ver más abajo).
4. Solo existe **una sesión activa**: un login nuevo invalida el token anterior.
5. Se envía en el **body** para los `POST` y como **query param** `?token=` en `GET /getparams`.

### Validación en el firmware

Hay **un único** validador, `Plant::isSessionValid(const String& token)` (`Plant.cpp`), y
comprueba cuatro cosas en orden: que haya una sesión activa, que el token mida 32 caracteres,
que coincida con el vigente, y que no haya expirado (comparación de `millis()` a prueba de
wrap-around). Las rutas que **bloquean** con `401 INVALID_SESSION` si falla son
`POST /newparams` (`validateCropParameters`), `POST /newcrop` (`startNewCrop`) y
`POST /wificredentials` (`saveWifiCredentials`),
que esperan el token en la clave `token` del body JSON. `GET /getparams` (`buildParamsJson`)
usa el validador pero **no bloquea**: solo reporta `sessionValid` para que el front sepa si
puede saltarse el login. No hay ningún otro mecanismo de autorización (ni middleware, ni
filtrado por IP/MAC, ni cabeceras): **el token en el body es la única llave**.

### Manejo del 401 en el cliente

El portal guarda el token que devuelven login y registro, y lo reenvía en `/newparams`,
`/newcrop`, `/wificredentials` (body) y `/getparams` (`?token=`). Si el firmware responde
`401 INVALID_SESSION` (token perdido tras un reinicio del ESP32, o expirado a los 30 min), el
portal **limpia el token guardado y devuelve al usuario a la pantalla de login**, en lugar de
mostrar un error genérico.

Dos detalles que hacen que eso sea utilizable y no una molestia:

- **Se conserva lo que el usuario había escrito.** Al volver de una sesión caída el portal
  **no** repuebla el formulario desde el dispositivo. Ocultar una vista no borra los campos,
  así que los valores sobreviven al paso por el login; lo único que los destruía era el propio
  portal al repoblarlos al reentrar. El mensaje lo dice: *"tus cambios siguen en el
  formulario"*. Sin esto el usuario se re-autenticaba, veía los valores viejos de vuelta y
  podía guardarlos **creyendo que guardaba los suyos** — un fallo silencioso en un equipo que
  programa riego.
- **Antes de entrar a una vista protegida se refresca el estado.** El portal consulta
  `/getparams` antes de decidir si la sesión sigue viva, en vez de fiarse de la última copia
  en memoria. Sin ese refresco se entraba a editar con un `sessionValid` obsoleto y el 401
  aparecía recién al guardar, cuando el trabajo ya estaba hecho.

El TTL es **fijo** (30 min desde el login, no se renueva con la actividad), así que la
expiración a media sesión es un caso normal, no excepcional: de ahí que la recuperación tenga
que ser limpia.

---

## `GET /`

Sirve la página única del portal (el HTML embebido en `mainForm.h`).

| | |
|---|---|
| Handler | `handleRoot` |
| Entrada | — |
| Respuesta | `text/html` |

Se usa `send_P` para servir el HTML por trozos directo desde flash, evitando un `String` temporal de ~50 KB que podía agotar el heap en modo AP+STA.

---

## `GET /getparams`

Estado del dispositivo. Es la primera llamada que hace el front para decidir qué pantalla pintar.

| | |
|---|---|
| Handler | `handleGetParameters` → `Plant::buildParamsJson` |
| Entrada | `?token=<32 hex>` (opcional) |
| Respuesta | JSON directo |

**Siempre presente:**

| Campo | Tipo | Descripción |
|---|---|---|
| `hasRegisteredUser` | bool | Decide registro vs. dashboard en el front |
| `sessionValid` | bool | `true` si el `token` recibido sigue vigente (permite saltar el login) |
| `wifiConnected` | bool | Estado del STA (conexión a la red del usuario) |
| `wifiSsid` | string | SSID asociado, o `""` si el STA no está conectado |
| `firmwareVersion` | string | Versión del binario en ejecución (constante de compilación `firmwareVersion` en `Constants.h`, **no** un valor en NVS: así cada binario reporta lo que realmente es tras un OTA). La vista OTA del portal la muestra; si falta, pinta "desconocida" |
| `rtcValid` | bool | Salud del reloj. En `false` el firmware **no acciona nada** aunque `enable` sea `true` (ver [nota sobre el RTC](#nota-sobre-el-rtc-y-el-modo-seguro)). El portal lo usa para el tercer estado del indicador; el backend lo querrá como telemetría |

**Presentes solo si `hasRegisteredUser == true`:**

| Campo | Tipo | Descripción |
|---|---|---|
| `planta` | string | Nombre de la planta |
| `enable` | bool | Interruptor general del cultivo. En `false` no se acciona **nada** (luces, riego ni ventilación), ignorando fotoperiodo e intervalos |
| `fpOn` | 0–23 | Hora de prendido del fotoperiodo |
| `fpOff` | 0–23 | Hora de apagado del fotoperiodo |
| `ledAzul` | 0–100 | Espectro azul (%) |
| `ledRojo` | 0–100 | Espectro rojo (%) |
| `ledBlanco` | 0 / 1 | Luz blanca ON/OFF (ver [nota sobre `ledBlanco`](#nota-sobre-ledblanco)) |
| `irrH` | horas | Intervalo de riego (ver [nota de frecuencias](#nota-de-frecuencias)) |
| `irrM` | 0–59 | Duración del riego (min) |
| `ventH` | horas | Intervalo de ventilación |
| `ventM` | 0–59 | Duración de la ventilación (min) |
| `dia` | number | Día del ciclo del cultivo (derivado del RTC; `0` = sin anclar **o** RTC sin hora fiable) |
| `semana` | number | Semana del ciclo, derivada de `dia` |

`dia` y `semana` **no** se almacenan: se calculan desde el ancla `cropStart` en NVS y la fecha del RTC, así que el cultivo sigue envejeciendo aunque el equipo haya estado apagado. Devuelven `0` también si el RTC no da una hora de confianza (ver [nota sobre el RTC](#nota-sobre-el-rtc-y-el-modo-seguro)).

---

## `POST /usercredentials`

Alta del usuario en el primer arranque. **No** reinicia.

| | |
|---|---|
| Handler | `handleUserCredentials` → `Plant::validateUserCredentials` |

```json
{
  "user": "admin",
  "pass": "miClave123"
}
```

**Validación:**
- `user`: 4–32 caracteres. Charset: letras, dígitos y `_-.@!#$%&*?+=`. Sin espacios.
- `pass`: 8–64 caracteres. Mismo charset, sin espacios.
- Ninguno con 4 o más caracteres idénticos consecutivos.

**Éxito:** guarda el usuario y **la contraseña derivada** en NVS (namespace `config`: `username`, `pwSalt`, `pwHash`), marca `hasRegisteredUser=1` (namespace `system`) y **devuelve un `token`** de sesión. La contraseña en claro nunca se persiste ni se conserva en RAM (ver [Contraseña de usuario](ARCHITECTURE.md#1-contraseña-de-usuario-resuelto)).

```json
{ "status": true, "message": "Usuario registrado.", "token": "a1b2c3…" }
```

El registro **no** acepta el comando de factory reset.

---

## `POST /authusercredentials`

Login para desbloquear la edición, y puerta del factory reset.

| | |
|---|---|
| Handler | `handleAuthUserCredentials` → `Plant::authUserCredentials` |

```json
{
  "user": "admin",
  "pass": "miClave123"
}
```

**Lógica, en orden:**

1. Si `pass == "**reset**"` → **reset de fábrica**: `hardReset()` borra **todos** los namespaces de Preferences (`system`, `plantData`, `config` y `wifi`, incluida la passphrase de la red del usuario), responde `HARD_RESET` y **el dispositivo reinicia** tras 1 s. Se evalúa **antes** de comparar credenciales, así que funciona aunque se haya olvidado la contraseña — es su razón de ser.

   **Solo se acepta si la petición entró por la interfaz del AP.** Como no exige credenciales, su única barrera es la cercanía física. Hoy la comprobación es redundante —el servidor solo escucha en la IP del SoftAP, ver [Caveats de seguridad](ARCHITECTURE.md#caveats-de-seguridad)— y se mantiene como defensa en profundidad. Si llegara por otra interfaz se rechaza con `RESET_REQUIRES_AP` (403), comparando `server.client().localIP()` con `WiFi.softAPIP()`: la interfaz que aceptó la conexión, no un dato que el cliente pueda falsificar.

   Para empezar una cosecha nueva sin perder la cuenta ni la red, la operación correcta es [`POST /newcrop`](#post-newcrop).
2. **Bloqueo por intentos fallidos.** Si hay una ventana de bloqueo activa se responde `TOO_MANY_ATTEMPTS` (**429**) y se corta ahí. Se evalúa **después** del reset y **antes** de derivar el hash, y ese orden es deliberado:
   - Después del reset, porque el reset es la vía de recuperación: si el bloqueo lo tapara, quien olvide su contraseña e insista se quedaría sin salida.
   - Antes de derivar, porque cada derivación PBKDF2 cuesta cientos de ms bloqueando `handleClient()`. Rechazar rápido es lo que evita que el propio login sea un vector de denegación de servicio.

   **Política:** los primeros **5** fallos consecutivos no se castigan (los errores de tecleo son normales). Del quinto en adelante la espera arranca en **5 s** y se **duplica** con cada fallo adicional —10, 20, 40, 80, 160— hasta un tope de **5 min**. Un login correcto limpia el contador. El contador **no** se reinicia al expirar la ventana: quien siga insistiendo espera cada vez más.

   El estado vive **solo en RAM**: reiniciar el equipo lo borra. Es un trade-off aceptado —evita desgastar flash y evita un bloqueo persistente que sería un DoS— y significa que esto frena scripts, no a alguien con acceso físico.
3. Si no, valida longitudes y compara contra lo guardado: deriva la contraseña con el salt de NVS y compara el resultado en tiempo constante. En éxito emite token (no reinicia); si no coinciden → `MISMATCH_CREDENTIALS` y se suma un fallo.

```json
{ "status": true, "message": "Acceso concedido.", "token": "a1b2c3…" }
```

---

## `POST /newparams`

Guarda los parámetros del cultivo y sincroniza el RTC. **Requiere token.** Los cambios se aplican en vivo, **sin reiniciar**.

| | |
|---|---|
| Handler | `handleNewParameters` → `Plant::validateCropParameters` |

Todas las claves son obligatorias. La ausencia de una de las **11 claves de cultivo**
(`planta`, `enable`, `fpOn`, `fpOff`, `ledAzul`, `ledRojo`, `ledBlanco`, `irrH`, `irrM`,
`ventH`, `ventM`) devuelve `MISSING_FIELDS`; la ausencia de `token` devuelve
`INVALID_SESSION`. Las **7 claves de fecha/hora** (`seg`, `min`, `hr`, `diaSem`, `dia`,
`mes`, `anio`) no se comprueban por presencia: si faltan, fallan la validación de tipo y
devuelven el `INVALID_*_FORMAT` correspondiente.

**Parámetros del cultivo:**

| Campo | Rango | Notas |
|---|---|---|
| `planta` | 3–20 chars | Charset con espacios; sin espacios dobles; no solo dígitos; sin 4+ repetidos |
| `enable` | bool | Interruptor general del cultivo. En `false` no se acciona **nada** (luces, riego ni ventilación), ignorando fotoperiodo e intervalos |
| `fpOn` | 0–23 | `fpOn != fpOff` |
| `fpOff` | 0–23 | El ciclo puede cruzar medianoche |
| `ledAzul` | 0–100 | Espectro azul (%) |
| `ledRojo` | 0–100 | Espectro rojo (%) |
| `ledBlanco` | 0 / 1 | Luz blanca ON/OFF (**estricto**: cualquier otro valor → `INVALID_WHITE_LED_VALUE`) |
| `irrH` | `validFrequencies` | Intervalo en horas |
| `irrM` | 0–59 | Duración en minutos. **Debe ser ≥ 1 si `irrH > 0`** (ver [nota de duración](#nota-sobre-frecuencia-y-duración)) |
| `ventH` | `validFrequencies` | Intervalo en horas |
| `ventM` | 0–59 | Duración en minutos. **Debe ser ≥ 1 si `ventH > 0`** |

**Sesión:**

| Campo | Notas |
|---|---|
| `token` | 32 hex, vigente. Sustituye el reenvío de credenciales en claro |

**Fecha/hora** (sincroniza el RTC; todos deben caber en `uint8_t`):

| Campo | Rango | Notas |
|---|---|---|
| `seg` | 0–59 | |
| `min` | 0–59 | |
| `hr` | 0–23 | |
| `diaSem` | 1–7 | El front envía `getDay()+1` → domingo = 1, sábado = 7 |
| `dia` | 1–31 | No se valida contra la longitud real del mes |
| `mes` | 1–12 | |
| `anio` | 0–99 | Año a 2 dígitos |

**Ejemplo:**

```json
{
  "planta": "Albahaca", "enable": true,
  "fpOn": 18, "fpOff": 6,
  "ledAzul": 70, "ledRojo": 45, "ledBlanco": 1,
  "irrH": 3, "irrM": 15, "ventH": 4, "ventM": 20,
  "token": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6",
  "seg": 15, "min": 30, "hr": 10,
  "diaSem": 4, "dia": 18, "mes": 6, "anio": 26
}
```

En el primer `/newparams` con fecha válida se ancla `cropStart` en NVS. El guard `_cropStartDay == 0` evita reescribirlo en ediciones posteriores.

---

## `POST /newcrop`

Cierra el cultivo actual y deja el equipo listo para empezar otro. Es la operación de **cada cosecha**: rutinaria y autenticada.

| | |
|---|---|
| Handler | `handleNewCrop` → `Plant::startNewCrop` |
| Cuerpo | `{"token": "<hex 32>"}` |
| Autorización | **Token de sesión obligatorio** |
| Respuesta | Sobre `{status, message}` |

```bash
curl -X POST http://192.168.4.1/newcrop \
     -H "Content-Type: application/json" \
     -d '{"token":"a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"}'
```

**Qué borra y qué conserva:**

| Se borra | Se conserva |
|---|---|
| Nombre de la planta (`plantData`) | Usuario y contraseña (`config`) |
| Ancla de la edad del cultivo (`cropStart`) | Credenciales Wi-Fi (`wifi`) |
| Parámetros: fotoperiodo, LEDs, riego y ventilación | Flags `hasRegisteredUser` y `hasWifiCredentials` |
| `enable` (queda en `false`, todo apagado) | |

Se aplica **al instante**: con `enable` en `false`, `turnOnDevices()` apaga todos los actuadores. Después, `dia` y `semana` valen `0` hasta que un `/newparams` con fecha válida vuelva a anclar el cultivo, que arranca de nuevo en el día 1.

> **No confundir con el reset de fábrica.** Son dos operaciones distintas a propósito:
>
> | | `POST /newcrop` | `**reset**` en `/authusercredentials` |
> |---|---|---|
> | Propósito | Empezar una cosecha nueva | Escotilla de recuperación |
> | Frecuencia | Habitual | Excepcional |
> | Autenticación | **Token obligatorio** | **Ninguna** (es la salida si se olvida la contraseña) |
> | Borra la cuenta | No | Sí |
> | Borra la Wi-Fi | No | Sí |
> | Desde dónde | Cualquier interfaz | **Solo por el AP** |

---

## `GET /wifiscan`

Escaneo de redes on-demand, **asíncrono**. Devuelve las **5 más fuertes sin SSID repetido**.

| | |
|---|---|
| Handler | `handleWifiScan` → `Plant::scanNetworks` |
| Respuesta | JSON directo |

Mientras el escaneo está en curso:

```json
{ "scanning": true, "networks": [] }
```

Cuando termina:

```json
{
  "scanning": false,
  "networks": [
    { "ssid": "MiRed",  "rssi": -48, "secure": true  },
    { "ssid": "Vecino", "rssi": -71, "secure": false }
  ]
}
```

El escaneo es **asíncrono**: la primera petición lo arranca y responde al instante con `{"scanning": true, "networks": []}`, sin bloquear `handleClient()`. El cliente vuelve a llamar al mismo endpoint (el portal lo hace cada ~700 ms, con tope de ~8 s) hasta recibir `"scanning": false` con la lista. Al entregar los resultados se liberan, así que la siguiente llamada arranca un escaneo nuevo — eso es lo que hace el botón "buscar de nuevo".

Ante dos APs con el mismo SSID (repetidores) conserva el de mayor RSSI, y omite las redes ocultas (SSID vacío).

> **Caveat de radio:** el ESP32-C3 tiene una sola antena, así que durante el escaneo salta de canal y el AP puede perder algún paquete. El escaneo es asíncrono y el portal no se congela, pero una petición que caiga justo en ese momento puede tardar más de lo normal.

---

## `POST /wificredentials`

Configura la red del usuario (modo STA). **Requiere token.** Arranca el intento sin bloquear.

| | |
|---|---|
| Handler | `handleWifiCredentials` → `Plant::saveWifiCredentials` |

```json
{
  "ssid": "MiRed",
  "pass": "claveDeMiRed",
  "token": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6"
}
```

**Validación:**
- `token` obligatorio y vigente → si no, `INVALID_SESSION` (401).
- `ssid`: 1–32 caracteres.
- `pass`: vacía (red abierta) o 8–63 caracteres (WPA2).

**Importante:** `STATUS_OK` significa *"intento iniciado"*, no *"conectado"*. La respuesta es inmediata para no bloquear `handleClient()` varios segundos; el front confirma el resultado haciendo **polling a `/getparams`** y leyendo `wifiConnected`.

Las credenciales **no se persisten aquí**. `updateWifi()` (llamado desde `loop()`) las escribe en NVS solo cuando la conexión alcanza `WL_CONNECTED`, para no dejar guardada una contraseña que no sirve.

---

## `GET /exit`

Logout. Invalida la sesión en el dispositivo (`clearSession()`); el front además limpia su token de `localStorage`.

```json
{ "status": true, "message": "Desconectado correctamente. Ya puedes cerrar esta ventana y desconectarte de la red SmartPlant." }
```

---

## Cualquier otra ruta

`handleNotFound` responde `302` con `Location: /`. Es lo que hace que el sistema operativo detecte el portal captivo y abra el formulario automáticamente.

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

## Nota sobre `ledBlanco`

El LED blanco está en **GPIO 0 como salida digital**, no como canal PWM: solo tiene dos estados.

- El portal envía `0` o `1`.
- El firmware **exige** `0` o `1`: cualquier otro valor se rechaza con `INVALID_WHITE_LED_VALUE` (400). La validación tiene su propio estado, separado de `INVALID_LED_VALUE`, que cubre solo los espectros azul y rojo.
- Internamente se guarda tal cual en `_systemStatus[whiteLedOn]` y se aplica con **lógica directa**: `deviceOn` (nivel alto) enciende. La polaridad de todas las salidas ON/OFF está centralizada en `deviceOn`/`deviceOff` (`Constants.h`). `/getparams` siempre devuelve `0` o `1`.

Los canales azul (`ledAzul`) y rojo (`ledRojo`) sí son PWM y usan el rango 0–100 % completo, escalado internamente a 0–255.

> **Nombres y contrato:** las claves son `ledAzul`, `ledRojo` (`0–100`, PWM) y `ledBlanco` (`0/1` estricto, salida digital). `ledBlanco` distinto de `0/1` devuelve `INVALID_WHITE_LED_VALUE`. Cualquier otro nombre de clave produce `MISSING_FIELDS` (falta una obligatoria).

---

## Nota sobre frecuencia y duración

`irrH`/`ventH` dicen **cada cuánto** se activa el dispositivo; `irrM`/`ventM`, **cuánto tiempo** permanece encendido dentro de la hora en que toca.

**Regla:** si la frecuencia es mayor que 0, la duración debe ser **al menos 1 minuto**. La combinación "frecuencia activa + duración 0" se rechaza con `INVALID_IRRIGATION_DURATION` / `INVALID_VENTILATION_DURATION` (400).

El motivo es que esa combinación **no hace nada**: `manageDevice()` enciende mientras `minuto_actual < duración`, así que con duración 0 la condición nunca se cumple y el dispositivo jamás arranca. Al mismo tiempo, el portal y el dashboard mostrarían una frecuencia configurada ("Cada 3h"), de modo que un riego que nunca ocurre pasaría inadvertido. Para desactivar existe la forma canónica y explícita: **frecuencia `0` ("Apagado")**.

La regla es asimétrica a propósito: **frecuencia 0 con duración > 0 sí es válida**. Conservar el valor de duración mientras el dispositivo está apagado es útil para cuando se reactive.

---

## Catálogo de errores

Definido en `buildHttpResponse()` (`Plant.cpp`).

### Generales

| Estado | HTTP | Mensaje |
|---|---|---|
| `STATUS_OK` | 200 | Parámetros actualizados correctamente. |
| `HARD_RESET` | 200 | Factory reset ejecutado. |
| `NEW_CROP_DONE` | 200 | Cultivo reiniciado. Configura los parámetros del nuevo cultivo para comenzar. |
| `RESET_REQUIRES_AP` | 403 | El restablecimiento solo puede hacerse desde la red Wi-Fi del dispositivo. Conéctate a la red SmartPlant e inténtalo de nuevo. |
| `INVALID_JSON` | 400 | El formato de envío es inválido. |
| `STORAGE_ERROR` | 400 | Los datos recibidos no se pudieron guardar. |
| `MISSING_FIELDS` | 400 | Campos requeridos faltantes. |
| *(default)* | 500 | Error interno del sistema. |

### Credenciales y sesión

| Estado | HTTP | Mensaje |
|---|---|---|
| `MISSING_CREDENTIALS` | 400 | Los campos de usuario y contraseña son obligatorios. |
| `INVALID_USERNAME_LENGTH` | 400 | Longitud de usuario inválida (4-32 caracteres). |
| `INVALID_USERPASS_LENGTH` | 400 | Longitud de contraseña inválida (8-64 caracteres). |
| `INVALID_USERNAME_CHARS` | 400 | El nombre de usuario solo permite los caracteres (_-.@!#$%&*?+=). |
| `INVALID_USERPASS_CHARS` | 400 | La contraseña de usuario solo permite los caracteres (_-.@!#$%&*?+=). |
| `USERNAME_REPEATED_CHARS` | 400 | El nombre de usuario tiene un caracter repetido más de 3 veces. |
| `USERPASS_REPEATED_CHARS` | 400 | La contraseña de usuario tiene un caracter repetido más de 3 veces. |
| `MISMATCH_CREDENTIALS` | 400 | Las credenciales enviadas no coinciden. |
| `TOO_MANY_ATTEMPTS` | **429** | Demasiados intentos fallidos. Espera un momento antes de volver a intentarlo. |
| `INVALID_SESSION` | **401** | Tu sesión expiró. Vuelve a iniciar sesión. |

### Nombre de la planta

| Estado | HTTP | Mensaje |
|---|---|---|
| `MISSING_PLANTNAME_FIELD` | 400 | El campo planta es obligatorio. *(Definido pero no emitido: la ausencia de `planta` cae en `MISSING_FIELDS`.)* |
| `INVALID_PLANTNAME_LENGTH` | 400 | Longitud de planta inválida (3-20 caracteres). |
| `INVALID_PLANTNAME_CHARS` | 400 | El nombre de la planta solo permite los caracteres (_-.@!#$%&*?+=). |
| `PLANTNAME_REPEATED_CHARS` | 400 | El nombre de la planta tiene un caracter repetido más de 3 veces. |
| `PLANTNAME_REPEATED_SPACES` | 400 | El nombre de la planta no puede tener espacios consecutivos. |
| `PLANTNAME_ONLY_DIGITS` | 400 | El nombre de la planta no puede ser solo números. |

### Parámetros del cultivo

| Estado | HTTP | Mensaje |
|---|---|---|
| `INVALID_PHOTOPERIOD_TYPE` | 400 | Valor de fotoperiodo inválido (solamente enteros). |
| `INVALID_IRRIGATION_TYPE` | 400 | Valores de irrigación inválidos (frecuencia permitida y minutos 0-59). |
| `INVALID_VENTILATION_TYPE` | 400 | Valores de ventilación inválidos (frecuencia permitida y minutos 0-59). |
| `INVALID_IRRIGATION_DURATION` | 400 | Con el riego activo la duración debe ser de al menos 1 minuto. Para desactivarlo, elige la frecuencia 'Apagado'. |
| `INVALID_VENTILATION_DURATION` | 400 | Con la ventilación activa la duración debe ser de al menos 1 minuto. Para desactivarla, elige la frecuencia 'Apagado'. |
| `INVALID_LED_VALUE` | 400 | Los espectros azul y rojo deben estar entre 0 y 100%. |
| `INVALID_WHITE_LED_VALUE` | 400 | La luz blanca solo acepta 0 (apagada) o 1 (encendida). |

### Fecha y hora

| Estado | HTTP | Mensaje |
|---|---|---|
| `INVALID_SECOND_FORMAT` | 400 | El campo segundo debe ser un entero sin signo (0-59). |
| `INVALID_MINUTE_FORMAT` | 400 | El campo minuto debe ser un entero sin signo (0-59). |
| `INVALID_HOUR_FORMAT` | 400 | El campo hora debe ser un entero sin signo (0-23). |
| `INVALID_WEEKDAY_FORMAT` | 400 | El campo dia de la semana debe ser un entero sin signo (1-7). |
| `INVALID_DAY_FORMAT` | 400 | El campo dia debe ser un entero sin signo (1-31). |
| `INVALID_MONTH_FORMAT` | 400 | El campo mes debe ser un entero sin signo (1-12). |
| `INVALID_YEAR_FORMAT` | 400 | El campo año debe ser un entero sin signo (0-99). |

### Red Wi-Fi

| Estado | HTTP | Mensaje |
|---|---|---|
| `MISSING_WIFI_FIELDS` | 400 | Falta la red Wi-Fi a configurar. |
| `INVALID_SSID` | 400 | El nombre de la red (SSID) es inválido (1-32 caracteres). |
| `INVALID_WIFI_PASS` | 400 | La contraseña Wi-Fi debe tener entre 8 y 63 caracteres. |

---

## Ejemplos con `curl`

Desde una máquina conectada al AP `SmartPlant` (gateway `192.168.4.1`):

```bash
# Estado del dispositivo
curl http://192.168.4.1/getparams

# Login → devuelve token
curl -X POST http://192.168.4.1/authusercredentials \
  -H "Content-Type: application/json" \
  -d '{"user":"admin","pass":"miClave123"}'

# Escanear redes
curl http://192.168.4.1/wifiscan

# Factory reset (no requiere conocer la contraseña)
curl -X POST http://192.168.4.1/authusercredentials \
  -H "Content-Type: application/json" \
  -d '{"user":"admin","pass":"**reset**"}'
```
