# API HTTP del backend (nube)

Rutas que expone el servicio **FastAPI** de la nube (`pythonServer/`). Es la API que consumen dos clientes muy distintos: el **dispositivo** (ESP32-C3, tras provisionarse) y la **app** del usuario (móvil/tablet). No confundir con la [API local del dispositivo](API.md), que es un contrato aparte servido por el propio firmware en la red del AP.

**Fuente:** `pythonServer/app/main.py`, `app/routers/auth.py`, `app/routers/devices.py`, `app/routers/me.py`, `app/deps.py`, `app/security.py`, `app/models.py`.

> El **diseño autoritativo** (modelo de datos, decisiones y contratos completos, incluyendo lo aún no implementado) está en [`BACKEND.md`](BACKEND.md). Este documento describe lo que **existe en el código** y marca explícitamente lo que todavía **no** está implementado.

---

## Estado de implementación

| Superficie | Rutas | Estado |
|---|---|---|
| Salud | `GET /health` | ✅ Implementado y verificado |
| Auth de usuario | `POST /auth/register` · `/login` · `/refresh` | ✅ Implementado y verificado |
| Provisión de dispositivo | `POST /devices/provision` | ✅ Implementado y verificado |
| Device-facing | `POST /devices/{id}/config` · `/telemetry` | ✅ Implementado y verificado |
| App-facing | `GET /me/devices` · `/devices/{id}/state` · `/devices/{id}/telemetry` | ✅ Implementado y verificado |
| OTA (device) | `GET /firmware/latest` | ⛔ No implementado — diseño en [`BACKEND.md` §4.1/§6](BACKEND.md) |
| Admin (firmware) | `POST /admin/firmware` · `GET /admin/firmware` | ⛔ No implementado — diseño en [`BACKEND.md` §4.3/§6](BACKEND.md) |
| Ingesta MQTT | `devices/+/telemetry` | ⛔ No implementado — diseño en [`BACKEND.md` §5](BACKEND.md) |

---

## Convenciones

- Todo es JSON con `Content-Type: application/json` (la subida de firmware, cuando exista, será `multipart/form-data`).
- Los **errores** usan el formato de FastAPI: `{"detail": "<mensaje>"}` con el código HTTP correspondiente. No hay sobre `{status, message}` como en la API del dispositivo.
- Los **timestamps** son ISO 8601 con zona (`2026-09-03T21:44:08+00:00`). Los pone el servidor.
- `{id}` en las rutas es `devices.id` (entero autoincremental), **no** la MAC.
- Códigos habituales: `200`/`201`/`202` éxito · `400` validación de negocio · `401` autenticación · `404` recurso ajeno o inexistente · `409` conflicto · `422` body mal formado (validación de Pydantic).
- La documentación OpenAPI viva está en `/docs` (Swagger UI) y `/openapi.json`.

---

## Autenticación

El backend maneja **dos esquemas de credenciales distintos**, ambos vía cabecera `Authorization: Bearer <…>`. No se cruzan: un token de dispositivo no sirve en rutas de la app, y viceversa.

| | JWT de usuario | Token de dispositivo |
|---|---|---|
| Lo usa | La app (móvil/tablet) | El ESP32 tras provisionarse |
| Formato | JWT firmado (HS256) | 64 caracteres hex (`secrets.token_hex(32)`) |
| Se emite en | `register` / `login` / `refresh` | `POST /devices/provision` (una sola vez) |
| Se guarda en BD | Solo el hash de la contraseña (`users.password_hash`) | Solo `devices.token_hash` (bcrypt) |
| Dependencia | `get_current_user` (`app/deps.py`) | `get_current_device` (`app/deps.py`) |
| Vigencia | `access` 30 min · `refresh` 7 días | Hasta que se revoque (borrar/rotar la fila) |
| Rutas | `/me/*`, `GET /devices/{id}/state`, `GET /devices/{id}/telemetry` | `POST /devices/{id}/config`, `POST /devices/{id}/telemetry` |

> **No confundir** el token de dispositivo del backend con el **token de sesión del portal local** del firmware (RAM, 30 min, red del AP). Son cosas distintas; la comparación está en el [§4.0 de `BACKEND.md`](BACKEND.md).

### JWT de usuario (`get_current_user`)

El JWT lleva `sub` (id del usuario), `admin` (bool) y `type` (`access` o `refresh`). La dependencia:

1. Exige la cabecera `Authorization: Bearer <jwt>` → si falta, `401`.
2. Valida firma y expiración con `JWT_SECRET_KEY` / `JWT_ALGORITHM` → si falla, `401`.
3. Exige `type == "access"`: un **refresh token no autentica** peticiones normales → `401`.
4. Resuelve `sub` a `users.id`; si el usuario no existe, `401`.

Todos los fallos responden `401` con `{"detail": "<causa>"}` y `WWW-Authenticate: Bearer`.

### Token de dispositivo (`get_current_device`)

En rutas `POST /devices/{device_id}/...`. La dependencia trae el dispositivo por su `{device_id}` y verifica el token con `verify_secret(token, device.token_hash)` (bcrypt). No se puede buscar por `WHERE token_hash = hash(token)` porque bcrypt usa un salt distinto en cada hash.

Si el dispositivo no existe **o** el token no verifica, responde el **mismo** `401`: no se revela la existencia de un id a quien no trae el token correcto.

### Aislamiento por cuenta (rutas de la app)

`GET /devices/{id}/state` y `GET /devices/{id}/telemetry` filtran por `Device.user_id == user.id` en el `WHERE`. Si el id existe pero pertenece a **otra** cuenta, la consulta sale vacía y se responde `404` (no `403`), para no revelar su existencia.

---

## `GET /health`

Health check. Sin autenticación.

| | |
|---|---|
| Handler | `main.health` |
| Entrada | — |
| Respuesta | `{ "status": "ok" }` |

---

## `POST /auth/register`

Alta de una cuenta de usuario. Devuelve tokens para entrar sin re-autenticar.

| | |
|---|---|
| Handler | `auth.register` |
| Auth | — |

```json
{ "email": "usuario@ejemplo.com", "password": "miClave123" }
```

**Éxito** `201`:

```json
{
  "access_token": "<jwt>",
  "refresh_token": "<jwt>",
  "token_type": "bearer",
  "expires_in": 1800
}
```

`409` si el email ya está registrado. `422` si el `email` no es válido o falta un campo. La contraseña se guarda como hash bcrypt (`users.password_hash`); nunca en claro.

---

## `POST /auth/login`

Autenticación con email + contraseña.

| | |
|---|---|
| Handler | `auth.login` |
| Auth | — |

```json
{ "email": "usuario@ejemplo.com", "password": "miClave123" }
```

**Éxito** `200`: mismo sobre que `register` (access + refresh). `401` si las credenciales no coinciden.

---

## `POST /auth/refresh`

Emite un `access_token` nuevo a partir de un `refresh_token` válido, sin reenviar la contraseña.

| | |
|---|---|
| Handler | `auth.refresh` |
| Auth | Refresh token en el body |

```json
{ "refresh_token": "<jwt>" }
```

**Éxito** `200`: mismo sobre que `login`. `401` si el token es inválido, no es de `type=refresh`, o el usuario ya no existe.

---

## `POST /devices/provision`

Vincula un dispositivo a una cuenta y **emite su token de dispositivo**. Es el único momento en que la contraseña del usuario viaja por la red (debe ir sobre TLS en producción).

| | |
|---|---|
| Handler | `devices.provision` |
| Auth | Credenciales del usuario en el body |

```json
{ "email": "usuario@ejemplo.com", "pass": "miClave123", "mac": "AA:BB:CC:DD:EE:FF" }
```

> La contraseña se envía con la clave `pass` (lo que manda el firmware). En Python `pass` es palabra reservada, así que el schema la mapea con alias. `mac` es de longitud fija: 17 caracteres (`AA:BB:CC:DD:EE:FF`).

**Éxito** `201` — el `token` se devuelve **una sola vez**; no hay endpoint para recuperarlo:

```json
{ "device_id": 12, "token": "<64 hex>", "name": null }
```

**Lógica de alta vs. re-provisión:**

- MAC nueva → alta: crea la fila en `devices` vinculada a la cuenta.
- MAC existente del **mismo** dueño → re-provisión: **rota** el token (invalida el anterior).
- MAC existente de **otra** cuenta → `409`: no se roba ni se revincula.

`401` si las credenciales del usuario no son válidas. En la BD solo queda `devices.token_hash` (bcrypt): una filtración no entrega credenciales funcionales, y el token es revocable borrando o rotando esa fila.

---

## `POST /devices/{id}/config`

Registra la configuración vigente del cultivo. **Requiere token de dispositivo.**

| | |
|---|---|
| Handler | `devices.post_config` |
| Auth | `Authorization: Bearer <token de dispositivo>` |

Inserta una **fila nueva** en `device_configs` (histórico); **no** actualiza la anterior. La última fila por `applied_at` es la config vigente. Las claves espejan las de `/newparams` del firmware, en `snake_case`.

| Campo | Rango | Notas |
|---|---|---|
| `planta` | string | Nombre de la planta |
| `enable` | bool | Sistema activo |
| `fp_on` | 0–23 | Hora de prendido del fotoperiodo |
| `fp_off` | 0–23 | Hora de apagado |
| `led_a` | 0–100 | Espectro azul (%) |
| `led_r` | 0–100 | Espectro rojo (%) |
| `led_b` | 0 / 1 | Luz blanca ON/OFF (el LED blanco es digital, no PWM) |
| `irr_h` | ≥ 0 | Intervalo de riego en horas |
| `irr_m` | 0–59 | Duración del riego (min) |
| `vent_h` | ≥ 0 | Intervalo de ventilación en horas |
| `vent_m` | 0–59 | Duración de la ventilación (min) |
| `crop_start_day` | int / null | Ancla de edad del cultivo (`daysSinceEpoch` del día 1). Opcional |

```json
{
  "planta": "Albahaca", "enable": true,
  "fp_on": 18, "fp_off": 6,
  "led_a": 70, "led_r": 45, "led_b": 1,
  "irr_h": 3, "irr_m": 15, "vent_h": 4, "vent_m": 20,
  "crop_start_day": 20693
}
```

**Éxito** `201`:

```json
{ "config_id": 88, "applied_at": "2026-09-03T21:44:07+00:00" }
```

`401` sin token válido. `422` si algún campo viola su rango (validación de Pydantic).

---

## `POST /devices/{id}/telemetry`

Registra una lectura de telemetría. **Requiere token de dispositivo.** Vía de arranque para la telemetría (y fallback puntual cuando MQTT esté en pie).

| | |
|---|---|
| Handler | `devices.post_telemetry` |
| Auth | `Authorization: Bearer <token de dispositivo>` |

Todos los campos son **opcionales**, pero debe llegar **al menos uno**. El `ts` lo pone el servidor (no se depende del reloj del dispositivo). De paso, refresca `devices.last_seen_at` y, si viene, `devices.firmware_version`.

| Campo | Tipo | Notas |
|---|---|---|
| `temp` | float | Temperatura |
| `humidity` | float | Humedad relativa |
| `wifi_rssi` | int | RSSI del enlace Wi-Fi |
| `uptime` | int | Segundos encendido |
| `firmware_version` | string | Versión vigente; refresca la del dispositivo |

```json
{ "temp": 24.5, "humidity": 61.2, "wifi_rssi": -48, "uptime": 86400, "firmware_version": "1.0.1" }
```

**Éxito** `202`:

```json
{ "accepted": 1 }
```

`400` si el body está vacío (`{}`) — "Debe llegar al menos un campo de telemetría". `401` sin token válido.

---

## `GET /me/devices`

Lista los dispositivos de la cuenta autenticada. **Requiere JWT de usuario.**

| | |
|---|---|
| Handler | `me.list_my_devices` |
| Auth | `Authorization: Bearer <access JWT>` |

**Éxito** `200`:

```json
{
  "devices": [
    {
      "id": 12,
      "mac": "AA:BB:CC:DD:EE:FF",
      "name": "Invernadero patio",
      "firmware_version": "1.0.1",
      "last_seen_at": "2026-09-03T21:44:08+00:00",
      "online": true
    }
  ]
}
```

`online` es **derivado**, no una columna: `true` si `last_seen_at` cae dentro de la ventana reciente (`ONLINE_WINDOW`, 5 min). Una cuenta sin dispositivos recibe `{ "devices": [] }`. `401` sin JWT válido.

---

## `GET /devices/{id}/state`

Última configuración conocida + lectura de telemetría más reciente + edad del cultivo. **Requiere JWT de usuario.**

| | |
|---|---|
| Handler | `me.device_state` |
| Auth | `Authorization: Bearer <access JWT>` |

**Éxito** `200`:

```json
{
  "device": {
    "id": 12, "name": "Invernadero patio", "online": true,
    "firmware_version": "1.0.1", "last_seen_at": "2026-09-03T21:44:08+00:00"
  },
  "config": {
    "planta": "Albahaca", "enable": true, "fp_on": 18, "fp_off": 6,
    "led_a": 70, "led_r": 45, "led_b": 1,
    "irr_h": 3, "irr_m": 15, "vent_h": 4, "vent_m": 20,
    "applied_at": "2026-09-03T21:44:07+00:00"
  },
  "crop": { "dia": 34, "semana": 5 },
  "telemetry": {
    "ts": "2026-09-03T21:44:08+00:00", "temp": 24.5, "humidity": 61.2, "wifi_rssi": -48
  }
}
```

- `config`, `crop` y `telemetry` son `null` (o `{dia:0, semana:0}`) si aún no hay datos.
- `crop.dia` / `crop.semana` se **derivan** de `config.crop_start_day` con la misma regla que el firmware: `dia = daysSinceEpoch(hoy) − crop_start_day + 1` (el día del ancla es el 1), `semana = (dia − 1) // 7 + 1`. Antes de anclar o si la fecha es futura, ambos valen `0`. No se almacenan.
- `404` si el dispositivo pertenece a otra cuenta o no existe (aislamiento). `401` sin JWT válido.

---

## `GET /devices/{id}/telemetry`

Histórico de telemetría **agregado** con `time_bucket()` de Timescale. **Requiere JWT de usuario.**

| | |
|---|---|
| Handler | `me.device_telemetry_history` |
| Auth | `Authorization: Bearer <access JWT>` |

**Query params:**

| Param | Tipo | Notas |
|---|---|---|
| `from` | ISO 8601 | Inicio del rango (opcional) |
| `to` | ISO 8601 | Fin del rango (opcional) |
| `bucket` | `5m` · `1h` · `1d` | Ancho de agregación. Default `1h` |

**Éxito** `200`:

```json
{
  "device_id": 12,
  "bucket": "1h",
  "series": [
    {
      "ts": "2026-09-03T20:00:00+00:00",
      "temp_avg": 25.0, "temp_min": 24.0, "temp_max": 26.0,
      "humidity_avg": 58.5, "samples": 2
    }
  ]
}
```

Cada punto agrega por bucket: promedio/mín/máx de `temp`, promedio de `humidity` y `samples` (conteo). Los promedios se redondean a 2 decimales.

- `400` si el `bucket` no es uno de `{5m, 1h, 1d}`, o si `from > to`.
- `404` si el dispositivo es de otra cuenta o no existe. `401` sin JWT válido.

> **Nota de implementación.** `time_bucket` recibe el ancho como un `timedelta` (que asyncpg mapea a `interval` de Postgres). Pasarlo como string provoca un `500` (`function time_bucket(character varying, …) does not exist`), porque el parámetro enlazado no hace el cast implícito que sí haría un literal SQL.

---

## Rutas no implementadas (diseño)

Estas rutas están **diseñadas** en [`BACKEND.md`](BACKEND.md) pero **no existen** en el código todavía. Se listan para dar el mapa completo; sus contratos definitivos viven en el diseño.

### `GET /firmware/latest?current=<version>` (device-facing) — ⛔

Resuelve si hay una actualización aplicable para la versión actual del dispositivo. `200` con `{version, url, sha256, signature, size}` (la `url` es prefirmada y temporal; el binario se sirve en streaming desde el object storage) o `204 No Content` si ya está al día. Ver §4.1 y §6 de `BACKEND.md`.

### `POST /admin/firmware` · `GET /admin/firmware` (admin-facing) — ⛔

Requieren JWT de usuario con `is_admin = true`. `POST` sube el `.bin` (`multipart/form-data`) al object storage y registra el release; el `sha256` lo calcula el servidor. `GET` lista los releases. Ver §4.3 y §6 de `BACKEND.md`.

### Ingesta MQTT — ⛔

Consumidor suscrito a `devices/+/telemetry` que inserta en la hypertable. Requiere resolver antes la autenticación del broker (hoy `allow_anonymous`). Ver §5 de `BACKEND.md`.

---

## Catálogo de errores

Los errores siguen el formato de FastAPI: `{"detail": "<mensaje>"}`.

### Autenticación (`401`)

| Contexto | Mensaje |
|---|---|
| Falta el JWT de usuario | Falta el token de acceso |
| JWT inválido/expirado | Token de acceso inválido |
| Se usó un refresh token donde va un access | Token no es de tipo access |
| JWT sin `sub` | Token sin sujeto |
| Usuario del JWT ya no existe | Usuario no existe |
| Falta el token de dispositivo | Falta el token de dispositivo |
| Token de dispositivo inválido o id ajeno | Token de dispositivo inválido |
| Login/provisión con credenciales malas | Credenciales inválidas |
| Refresh token inválido / tipo incorrecto | Refresh token inválido · Token no es de tipo refresh |

### Negocio y recursos

| Estado | HTTP | Contexto |
|---|---|---|
| Email ya registrado | `409` | `POST /auth/register` |
| MAC de otra cuenta | `409` | `POST /devices/provision` |
| Telemetría con body vacío | `400` | `POST /devices/{id}/telemetry` — "Debe llegar al menos un campo de telemetría" |
| `bucket` no soportado | `400` | `GET /devices/{id}/telemetry` |
| Rango inválido (`from > to`) | `400` | `GET /devices/{id}/telemetry` |
| Dispositivo ajeno o inexistente | `404` | Rutas app-facing — "Dispositivo no encontrado" |
| Body mal formado / campo fuera de rango | `422` | Validación de Pydantic (automática) |

---

## Ejemplos con `curl`

Contra el backend local (con el override de puertos de esta máquina, en `http://localhost:8001`; por defecto sería `:8000`).

```bash
BASE=http://localhost:8001

# 1. Registrar una cuenta → devuelve JWT
curl -X POST $BASE/auth/register \
  -H "Content-Type: application/json" \
  -d '{"email":"usuario@ejemplo.com","password":"miClave123"}'

# 2. Provisionar un dispositivo → devuelve el token de dispositivo (una sola vez)
curl -X POST $BASE/devices/provision \
  -H "Content-Type: application/json" \
  -d '{"email":"usuario@ejemplo.com","pass":"miClave123","mac":"AA:BB:CC:DD:EE:FF"}'

# 3. El dispositivo registra su config (Bearer = token de dispositivo)
curl -X POST $BASE/devices/1/config \
  -H "Authorization: Bearer <token-de-dispositivo>" \
  -H "Content-Type: application/json" \
  -d '{"planta":"Albahaca","enable":true,"fp_on":18,"fp_off":6,"led_a":70,"led_r":45,"led_b":1,"irr_h":3,"irr_m":15,"vent_h":4,"vent_m":20,"crop_start_day":20693}'

# 4. El dispositivo envía telemetría
curl -X POST $BASE/devices/1/telemetry \
  -H "Authorization: Bearer <token-de-dispositivo>" \
  -H "Content-Type: application/json" \
  -d '{"temp":24.5,"humidity":61.2,"wifi_rssi":-48,"uptime":86400,"firmware_version":"1.0.1"}'

# 5. La app lista los dispositivos de la cuenta (Bearer = access JWT del usuario)
curl $BASE/me/devices -H "Authorization: Bearer <access-jwt>"

# 6. La app consulta el estado de un dispositivo
curl $BASE/devices/1/state -H "Authorization: Bearer <access-jwt>"

# 7. La app pide el histórico agregado por hora
curl "$BASE/devices/1/telemetry?bucket=1h" -H "Authorization: Bearer <access-jwt>"
```
