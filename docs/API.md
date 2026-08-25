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

`/newparams` y `/wificredentials` exigen un **token de sesión** vigente. El flujo es:

1. `POST /usercredentials` (primer arranque) o `POST /authusercredentials` (login) devuelven un `token`.
2. El token es una cadena de **32 caracteres hex** (128 bits de `esp_random()`).
3. Vive **solo en RAM**: un reinicio lo invalida. TTL **fijo de 30 min** desde su emisión — la actividad **no** lo renueva.
4. Solo existe **una sesión activa**: un login nuevo invalida el token anterior.
5. Se envía en el **body** para los `POST` y como **query param** `?token=` en `GET /getparams`.

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

**Presentes solo si `hasRegisteredUser == true`:**

| Campo | Tipo | Descripción |
|---|---|---|
| `planta` | string | Nombre de la planta |
| `enable` | bool | Sistema activo |
| `fpOn` | 0–23 | Hora de prendido del fotoperiodo |
| `fpOff` | 0–23 | Hora de apagado del fotoperiodo |
| `ledA` | 0–100 | Espectro azul (%) |
| `ledR` | 0–100 | Espectro rojo (%) |
| `ledB` | 0 / 1 | Luz blanca ON/OFF (ver [nota sobre `ledB`](#nota-sobre-ledb)) |
| `irrH` | horas | Intervalo de riego (ver [nota de frecuencias](#nota-de-frecuencias)) |
| `irrM` | 0–59 | Duración del riego (min) |
| `ventH` | horas | Intervalo de ventilación |
| `ventM` | 0–59 | Duración de la ventilación (min) |
| `dia` | number | Día del ciclo del cultivo (derivado del RTC; `0` = sin anclar) |
| `semana` | number | Semana del ciclo, derivada de `dia` |

`dia` y `semana` **no** se almacenan: se calculan desde el ancla `cropStart` en NVS y la fecha del RTC, así que el cultivo sigue envejeciendo aunque el equipo haya estado apagado.

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

**Éxito:** guarda las credenciales en NVS (namespace `config`), marca `hasRegisteredUser=1` (namespace `system`) y **emite un token** para que el usuario nuevo entre a editar sin volver a autenticarse:

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

1. Si `pass == "**reset**"` → `hardReset()` borra los namespaces `config` y `system`, responde `HARD_RESET` y **el dispositivo reinicia** tras 1 s. Se evalúa **antes** de comparar credenciales, así que funciona aunque se haya olvidado la contraseña.
2. Si no, valida longitudes y compara contra lo guardado. En éxito emite token (no reinicia); si no coinciden → `MISMATCH_CREDENTIALS`.

```json
{ "status": true, "message": "Acceso concedido.", "token": "a1b2c3…" }
```

---

## `POST /newparams`

Guarda los parámetros del cultivo y sincroniza el RTC. **Requiere token.** Los cambios se aplican en vivo, **sin reiniciar**.

| | |
|---|---|
| Handler | `handleNewParameters` → `Plant::validateCropParameters` |

Todas las claves son obligatorias; su ausencia devuelve `MISSING_FIELDS` (o `INVALID_SESSION` si falta `token`).

**Parámetros del cultivo:**

| Campo | Rango | Notas |
|---|---|---|
| `planta` | 3–20 chars | Charset con espacios; sin espacios dobles; no solo dígitos; sin 4+ repetidos |
| `enable` | bool | Sistema activo |
| `fpOn` | 0–23 | `fpOn != fpOff` |
| `fpOff` | 0–23 | El ciclo puede cruzar medianoche |
| `ledA` | 0–100 | Espectro azul (%) |
| `ledR` | 0–100 | Espectro rojo (%) |
| `ledB` | 0 / 1 | Luz blanca ON/OFF |
| `irrH` | `validFrequencies` | Intervalo en horas |
| `irrM` | 0–59 | Duración en minutos |
| `ventH` | `validFrequencies` | Intervalo en horas |
| `ventM` | 0–59 | Duración en minutos |

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
  "ledA": 70, "ledR": 45, "ledB": 1,
  "irrH": 3, "irrM": 15, "ventH": 4, "ventM": 20,
  "token": "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6",
  "seg": 15, "min": 30, "hr": 10,
  "diaSem": 4, "dia": 18, "mes": 6, "anio": 26
}
```

En el primer `/newparams` con fecha válida se ancla `cropStart` en NVS. El guard `_cropStartDay == 0` evita reescribirlo en ediciones posteriores.

---

## `GET /wifiscan`

Escaneo de redes on-demand. Devuelve las **5 más fuertes sin SSID repetido**.

| | |
|---|---|
| Handler | `handleWifiScan` → `Plant::scanNetworks` |
| Respuesta | JSON directo |

```json
{
  "networks": [
    { "ssid": "MiRed",  "rssi": -48, "secure": true  },
    { "ssid": "Vecino", "rssi": -71, "secure": false }
  ]
}
```

El escaneo **bloquea ~2 s** dentro del request: aceptable para una acción puntual del usuario. Ante dos APs con el mismo SSID (repetidores) conserva el de mayor RSSI, y omite las redes ocultas (SSID vacío).

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

## Nota sobre `ledB`

El LED blanco está en **GPIO 0 como salida digital**, no como canal PWM: solo tiene dos estados.

- El portal envía `0` o `1`.
- El firmware sigue aceptando `0–100` (la validación `INVALID_LED_VALUE` es común a los tres LEDs) pero lo interpreta como booleano: `_systemStatus[whiteDutyCycle] > 0 ? LOW : HIGH` (lógica invertida). Cualquier valor mayor a 0 enciende.

Los canales azul (`ledA`) y rojo (`ledR`) sí son PWM y usan el rango 0–100 % completo, escalado internamente a 0–255.

---

## Catálogo de errores

Definido en `buildHttpResponse()` (`Plant.cpp`).

### Generales

| Estado | HTTP | Mensaje |
|---|---|---|
| `STATUS_OK` | 200 | Parámetros actualizados correctamente. |
| `HARD_RESET` | 200 | Factory reset ejecutado. |
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
| `INVALID_SESSION` | **401** | Tu sesión expiró. Vuelve a iniciar sesión. |

### Nombre de la planta

| Estado | HTTP | Mensaje |
|---|---|---|
| `MISSING_PLANTNAME_FIELD` | 400 | El campo planta es obligatorio. |
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
| `INVALID_LED_VALUE` | 400 | Los valores de los LEDs deben estar entre 0 y 100%. |

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
