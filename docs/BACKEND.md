# Backend SmartPlant — Diseño y guía de arranque

> **Propósito de este documento.** Fijar las decisiones de arquitectura del backend
> para que la próxima sesión (humana o IA) pueda **arrancar la implementación sin
> recontexto**. Todo lo que sigue está **confirmado** salvo lo marcado explícitamente
> como "pendiente". Complementa la sección *"Modelado de la base de datos (backend
> futuro)"* y *"TLS en el ESP32-C3"* de [`ARCHITECTURE.md`](ARCHITECTURE.md), que
> siguen siendo la referencia del lado del firmware.

> **Estado: esqueleto funcional con auth implementado.** La infraestructura local corre con
> `docker compose up -d --build`. Alembic administra el esquema (migración `001` aplicada:
> extensión, 5 tablas, hypertable, índices). Los endpoints de autenticación (`register`,
> `login`, `refresh`) están operativos con JWT + bcrypt. El siguiente entregable es la
> provisión de dispositivos (`POST /devices/provision`).
>
> Decisiones que estaban abiertas y ya se cerraron:
> - **Broker: Mosquitto** (no EMQX). Ver el [caveat de autenticación MQTT](#caveat-mosquitto-no-autentica-contra-la-base-de-datos).
> - **Identificador de usuario: `email`**, como ya lo define el esquema (`UNIQUE NOT NULL`).
> - **Dueño del esquema: Alembic.** El compose ya **no** aplica `schema.sql`; la primera
>   migración crea extensión, tablas e hypertable.

---

## 1. Qué resuelve el backend

Cuatro requisitos, cada uno mapeado a una pieza del stack:

| Requisito | Naturaleza técnica | Pieza que lo resuelve |
|---|---|---|
| Persistir estado/config + variables de cultivo | Config (relacional) + telemetría (serie temporal) | Postgres + **TimescaleDB** |
| Dispositivos asociados a un usuario/cuenta | Multi-tenant simple (1:N) | Modelo relacional + JWT |
| Exponer binarios de firmware (OTA) | Archivos grandes, versionados, firmados | **Object storage S3-compatible** + tabla catálogo |
| Exponer histórico a móviles/tablets | API de lectura autenticada | **REST/HTTPS (FastAPI)** + JWT |

---

## 2. Decisiones confirmadas (el stack)

| Decisión | Valor | Notas |
|---|---|---|
| **Lenguaje/API** | **FastAPI (Python)** | Async, OpenAPI automático (clave para que móvil/tablet consuman sin fricción), Pydantic para validar payloads |
| **Base de datos** | **PostgreSQL + TimescaleDB** | Un solo motor: relacional + serie temporal. Hypertables, compresión, *continuous aggregates*, *retention policies* |
| **Ingesta de telemetría** | **MQTT** (broker **Mosquitto**) | Conexión persistente: amortiza el costoso handshake TLS del C3. REST/HTTPS solo para acciones puntuales. Ver [caveat de auth](#caveat-mosquitto-no-autentica-contra-la-base-de-datos) |
| **Storage OTA** | **S3-compatible**: **MinIO** local + **S3** en nube | Misma API (`aioboto3`); solo cambia `endpoint_url`. Binarios fuera de la BD |
| **Auth apps** | **JWT propio** (access + refresh) | `1 usuario = 1 cuenta` identificada por **`email`**; login simple, sin proveedor gestionado |
| **Auth dispositivos** | Token de dispositivo revocable | Aprovisionamiento `email+pass+mac` sobre TLS → backend emite token; se guarda `token_hash` |
| **ORM / migraciones** | **SQLAlchemy + Alembic** | Default de FastAPI |
| **Reverse proxy / TLS** | **Caddy** o **Traefik** | TLS/Let's Encrypt automático; el app **nunca** termina TLS directo |
| **Empaquetado** | **Docker** (compose para local) | Corre idéntico en servidor propio y en nube |

**Regla de oro que atraviesa todo:** el lenguaje del API es **solo una** de las piezas.
Postgres/Timescale, MQTT, object storage, JWT y el proxy son **idénticos** en servidor
propio o en nube; lo que cambia es la configuración (ver §6), no el código.

---

## 3. Modelo de datos (final)

Cardinalidades confirmadas:
- **1 usuario = 1 cuenta** → `users` y `accounts` colapsan en **una sola tabla** `users`.
- **1 usuario → N dispositivos**; **1 dispositivo → exactamente 1 usuario** (relación
  1:N; la FK vive en `devices`).

```
users (1) ──────< (N) devices
                        ├──< device_configs    (histórico de config)
                        └──< device_telemetry   (serie temporal → hypertable)

firmware_releases   (catálogo OTA; el binario vive en object storage, no en la BD)
```

| Tabla | Campos clave | Notas |
|---|---|---|
| `users` | `id`, `email` (UNIQUE), `password_hash`, `is_admin`, `created_at` | Cuenta = persona. Hash con **bcrypt/argon2**. `is_admin` (default `false`) habilita las rutas `/admin/*` (publicar firmware). Ver §6 |
| `devices` | `id`, `mac` (UNIQUE), `user_id` (FK → users, **NOT NULL**), `name`, `token_hash`, `firmware_version`, `last_seen_at`, `created_at` | La MAC identifica; `token_hash` = token de dispositivo hasheado (revocable) |
| `device_configs` | `id`, `device_id` (FK), `planta`, `enable`, `fp_on`, `fp_off`, `led_a/r/b`, `irr_h/m`, `vent_h/m`, `crop_start_day`, `applied_at` | **Una fila por cambio** (histórico). La última = config vigente. **Espeja las llaves de `/newparams`** del firmware |
| `device_telemetry` | `id`, `device_id` (FK), `ts`, `temp`, `humidity`, `firmware_version`, `wifi_rssi`, `uptime` | **Serie temporal → hypertable de TimescaleDB**. `temp`/`humidity` = lecturas de sensor (°C / % HR). `firmware_version` se estampa en cada lectura para correlacionar datos con la versión que los produjo |
| `firmware_releases` | `version`, `url`, `sha256`, `signature`, `min_version`, `published_at` | Catálogo para OTA. `url` apunta al object storage |

**Autorización (trivial gracias a 1:N):** cada request de la app trae el JWT del
usuario → *"¿`device.user_id` == `user.id` del token?"*. Una sola condición, sin
joins de permisos ni tabla de membresías.

> Las llaves de `device_configs` deben mantenerse en sync con el payload de
> `/newparams` del firmware (ver [`API.md`](API.md) y `Plant.cpp`). Si cambia
> el formulario del portal, cambia esta tabla.

> **`firmware_version` aparece en dos lugares a propósito, no por error:**
> `devices.firmware_version` guarda la versión **vigente** del dispositivo (para
> decidir OTA en `/firmware/latest?current=X`); `device_telemetry.firmware_version`
> estampa la versión **en cada lectura**, para poder analizar el histórico sabiendo
> qué firmware lo produjo (p. ej. detectar que un sensor empezó a reportar raro tras
> una actualización).

---

## 4. Contratos de la API

Tres superficies con esquemas de autenticación distintos. Convenciones comunes:

- Todo es JSON con `Content-Type: application/json`, salvo la subida de firmware (multipart).
- Errores con el formato de FastAPI: `{"detail": "<mensaje>"}` y el código HTTP correspondiente.
- Los timestamps son **ISO 8601 con zona** (`2026-08-24T17:04:00-06:00`).
- `{id}` en las rutas es `devices.id` (entero), no la MAC.

### 4.0 Cómo se autentican los dispositivos

Un dispositivo **no** guarda la contraseña del usuario. El flujo tiene dos fases:

**Fase 1 — provisión (una sola vez).** El usuario introduce sus credenciales en el portal
local del ESP32; el dispositivo las manda con su MAC a `POST /devices/provision` sobre TLS.
El backend valida la cuenta, crea o vincula la fila en `devices` y **emite un token de
dispositivo**. Es el único momento en que la contraseña del usuario viaja por la red.

**Fase 2 — operación (siempre).** El dispositivo se autentica con ese token en cada
petición, vía `Authorization: Bearer <token>`.

En la base se guarda **solo `devices.token_hash`** (bcrypt, igual que una contraseña): si se
filtra la BD, nadie obtiene credenciales funcionales. Y como el token vive en una fila, es
**revocable**: se borra o se rota y el dispositivo queda fuera hasta re-provisionarse.

> **No confundir con el token de sesión del firmware.** Son dos cosas distintas:
>
> | | Sesión del portal ([`API.md`](API.md)) | Token de dispositivo (este doc) |
> |---|---|---|
> | Vive en | RAM del ESP32 | `devices.token_hash` |
> | Sirve para | Editar parámetros en la red local | Hablar con la nube |
> | Vigencia | 30 min fijos, muere al reiniciar | Hasta que se revoque |
> | Lo emite | El propio dispositivo | El backend |

### 4.1 Device-facing (ESP32-C3 → backend)

Autenticadas con el **token de dispositivo**, salvo `/devices/provision`.

#### `POST /devices/provision`

```json
{ "email": "usuario@ejemplo.com", "pass": "miClave123", "mac": "A1:B2:C3:D4:E5:F6" }
```

`201` — el `token` se devuelve **una sola vez**; no hay endpoint para recuperarlo.

```json
{ "device_id": 12, "token": "<64 hex>", "name": null }
```

`401` credenciales inválidas · `409` la MAC ya está vinculada a otra cuenta.

#### `POST /devices/{id}/config`

Registra la configuración vigente del cultivo. Las llaves **espejan las del firmware**
(ver [`API.md`](API.md)), en `snake_case`.

```json
{
  "planta": "Albahaca", "enable": true,
  "fp_on": 18, "fp_off": 6,
  "led_a": 70, "led_r": 45, "led_b": 1,
  "irr_h": 3, "irr_m": 15, "vent_h": 4, "vent_m": 20,
  "crop_start_day": 20693
}
```

`201` → `{ "config_id": 88, "applied_at": "2026-08-24T17:04:00-06:00" }`

Inserta una fila nueva en `device_configs`; **no** actualiza la anterior (es un histórico).
`led_b` es `0`/`1`: el LED blanco del firmware es ON/OFF, no PWM.

#### `POST /devices/{id}/telemetry`

Vía de arranque para la telemetría, y fallback puntual cuando MQTT esté en pie.

```json
{ "temp": 24.5, "humidity": 61.2, "wifi_rssi": -48, "uptime": 86400, "firmware_version": "1.0.1" }
```

`202` → `{ "accepted": 1 }`. Todos los campos son opcionales salvo que llegue al menos uno.
`ts` lo pone el servidor si no viene, para no depender del reloj del dispositivo.

#### `GET /firmware/latest?current=1.0.1`

`200` cuando hay actualización aplicable:

```json
{
  "version": "1.1.0",
  "url": "https://…/firmware/xyzver1.1.0.bin",
  "sha256": "<64 hex>",
  "signature": "<base64>",
  "size": 1002944
}
```

`204 No Content` cuando el dispositivo ya está al día. La `url` es prefirmada y temporal;
el binario se sirve **en streaming** desde el object storage, no a través del API.

### 4.2 App-facing (móvil/tablet → backend)

Autenticadas con **JWT de usuario** (`Authorization: Bearer <access>`).

#### `POST /auth/register` · `POST /auth/login`

```json
{ "email": "usuario@ejemplo.com", "password": "miClave123" }
```

`200` en ambos:

```json
{ "access_token": "<jwt>", "refresh_token": "<jwt>", "token_type": "bearer", "expires_in": 1800 }
```

`register` → `409` si el email ya existe. `login` → `401` si no coincide.

#### `POST /auth/refresh`

`{ "refresh_token": "<jwt>" }` → mismo sobre que login, con un `access_token` nuevo.

#### `GET /me/devices`

```json
{
  "devices": [
    { "id": 12, "mac": "A1:B2:C3:D4:E5:F6", "name": "Invernadero patio",
      "firmware_version": "1.0.1", "last_seen_at": "2026-08-24T16:58:00-06:00", "online": true }
  ]
}
```

`online` es derivado, no una columna: `last_seen_at` dentro de una ventana reciente.

#### `GET /devices/{id}/state`

Última configuración conocida más la lectura más reciente. `404` si el dispositivo no
pertenece a la cuenta autenticada — no `403`, para no revelar su existencia.

```json
{
  "device": { "id": 12, "name": "Invernadero patio", "online": true,
              "firmware_version": "1.0.1", "last_seen_at": "2026-08-24T16:58:00-06:00" },
  "config": { "planta": "Albahaca", "enable": true, "fp_on": 18, "fp_off": 6,
              "led_a": 70, "led_r": 45, "led_b": 1,
              "irr_h": 3, "irr_m": 15, "vent_h": 4, "vent_m": 20,
              "applied_at": "2026-08-24T17:04:00-06:00" },
  "crop": { "dia": 34, "semana": 5 },
  "telemetry": { "ts": "2026-08-24T16:58:00-06:00", "temp": 24.5,
                 "humidity": 61.2, "wifi_rssi": -48 }
}
```

`crop.dia` / `crop.semana` se **derivan** de `crop_start_day`, con la misma regla que el
firmware (ver §3 y *"Edad del cultivo"* en [`ARCHITECTURE.md`](ARCHITECTURE.md)). No se
almacenan.

#### `GET /devices/{id}/telemetry?from&to&bucket`

`from`/`to` en ISO 8601; `bucket` es el ancho de agregación (`5m`, `1h`, `1d`).

```json
{
  "device_id": 12, "bucket": "1h",
  "series": [
    { "ts": "2026-08-24T16:00:00-06:00", "temp_avg": 24.1, "temp_min": 23.4,
      "temp_max": 25.0, "humidity_avg": 60.8, "samples": 12 }
  ]
}
```

Se apoya en `time_bucket()` de Timescale, y en *continuous aggregates* cuando el rango lo
justifique. `400` si el rango es inválido o el `bucket` no está soportado.

### 4.3 Admin-facing (staff del fabricante → backend)

Autenticadas con **JWT de usuario con `is_admin = true`**. Ver §6.1 para el ciclo completo.

#### `POST /admin/firmware`

`multipart/form-data` con los campos `file` (el `.bin`), `version`, `min_version` y
`signature`. Sube el binario al object storage y registra el release.

`201`:

```json
{ "version": "1.1.0", "sha256": "<64 hex>", "size": 1002944,
  "published_at": "2026-08-24T17:10:00-06:00" }
```

`409` si la versión ya existe. El `sha256` lo calcula el servidor al recibir el archivo; no
se acepta del cliente.

#### `GET /admin/firmware`

```json
{
  "releases": [
    { "version": "1.1.0", "min_version": "1.0.0", "sha256": "<64 hex>",
      "published_at": "2026-08-24T17:10:00-06:00" }
  ]
}
```

---

## 5. Ingesta MQTT (telemetría)

- **Broker:** **Mosquitto**. Suficiente para una cuenta con pocos dispositivos; su
  configuración son 6 líneas. Migrar a EMQX más adelante cambia un servicio del compose,
  no la aplicación: el protocolo es el mismo.
- **Patrón:** el device mantiene **una** conexión MQTT/TLS persistente y publica
  telemetría a un tópico por dispositivo (p. ej. `devices/{id}/telemetry`).
- Un **consumidor** (servicio Python, puede vivir junto al API o aparte) se suscribe
  e inserta en la hypertable `device_telemetry`.
- **Por qué MQTT y no REST para esto:** el handshake TLS es lo caro en el C3; una
  conexión persistente lo amortiza. Detalle y trade-offs de TLS/memoria en el C3 →
  sección *"TLS en el ESP32-C3"* de [`ARCHITECTURE.md`](ARCHITECTURE.md).
- **Regla de oro del firmware:** *sin Wi-Fi → cero tráfico*; *sin token y sin acción
  del usuario → silencio total* (no hay polling de entitlement). Ver ARCHITECTURE.md.

### Caveat: Mosquitto no autentica contra la base de datos

Esta es la contrapartida de elegir Mosquitto y hay que resolverla antes de mover la
telemetría a MQTT. **EMQX trae autenticación contra BD de fábrica; Mosquitto no.**

El estado actual de `mosquitto/config/mosquitto.conf` es `allow_anonymous true`: cualquiera
en la red puede publicar y suscribirse. Sirve para desarrollo local, **no** para nada
expuesto.

Tres caminos, en orden de esfuerzo creciente:

| Opción | Cómo funciona | Cuándo conviene |
|---|---|---|
| **REST primero** | La telemetría entra por `POST /devices/{id}/telemetry` con el token de dispositivo. MQTT se suma después. | Arranque. Desbloquea el resto del backend sin resolver esto ya. |
| **Archivo de contraseñas** | El backend regenera el archivo de Mosquitto (`mosquitto_passwd`) al provisionar y recarga el broker. Usuario = `device_id`, contraseña = token. | Pocos dispositivos, despliegue propio. |
| **Plugin contra Postgres** | `mosquitto-go-auth` valida usuario/contraseña y ACLs por tópico consultando `devices.token_hash`. | Cuando haya varios dispositivos o se exponga el broker. |

Recomendación: **arrancar por REST** y añadir MQTT cuando el resto esté en pie. El modelo
de datos no cambia — `device_telemetry` recibe lo mismo por cualquiera de las dos vías.

---

## 6. OTA — publicación, notificación y descarga

El ciclo completo del firmware over-the-air. Tres piezas ya diseñadas lo sostienen y
**ninguna tabla nueva hace falta**: `firmware_releases` (catálogo de versiones),
el **object storage** (guarda el `.bin`) y `devices.firmware_version` (versión vigente
de cada equipo).

### 6.1 Publicación de un release (admin)

Actor nuevo: **admin** = staff del fabricante, distinto de los usuarios finales. Se
modela con el flag `is_admin` en `users` (§3); las rutas `/admin/*` lo exigen.

Flujo recomendado — **subida a través del backend** (los binarios del C3 son pequeños,
~1–2 MB, así que no vale la pena complicarlo):

1. `POST /admin/firmware` (multipart): `.bin` + `version` + `min_version` + `signature`.
   Requiere JWT con `is_admin`.
2. El backend **sube el binario al object storage** en streaming (no lo carga entero en
   RAM), a una ruta determinista, p. ej. `firmware/{version}.bin`.
3. El backend **calcula el `sha256` del lado del servidor** leyendo el objeto ya
   guardado — **no** confía en un hash provisto por el cliente. Garantiza que `url` y
   `sha256` sean consistentes.
4. Inserta la fila en `firmware_releases` (`version`, `url`, `sha256`, `signature`,
   `min_version`, `published_at`).

**Alternativa (binarios grandes / descargar carga del API): presigned URL.** El backend
devuelve una URL de subida firmada, el admin sube **directo** al object storage y luego
confirma con `POST /admin/firmware/confirm` para registrar los metadatos. Para el C3 no
hace falta; se documenta por si el sistema crece.

**Inmutabilidad:** `version` es PK → re-publicar la misma versión **falla a propósito**.
Un release no se sobrescribe; si hubo un error, se publica una versión nueva. Así ningún
equipo queda con un `sha256` que ya no cuadra con el binario.

**Firma (OTA segura):** la firma se genera **offline, con una clave privada que NO vive
en el servidor**; el device la verifica con la clave pública embebida en su flash. Poner
la clave de firma en el backend anularía la protección si el servidor se compromete. Por
eso el admin **provee** la `signature` ya calculada al publicar (paso 1); el backend no
firma, solo la almacena.

### 6.2 Notificación de disponibilidad

Dos mecanismos **complementarios** (no alternativos): un push rápido + un poll de red de
seguridad.

- **Push por el canal de telemetría (rápido).** Al publicar un release, el backend
  publica en un tópico de bajada MQTT (`devices/{id}/ota` o uno común) con la versión
  disponible. **Mensaje retenido (`retain=true`)** para que un device que estaba offline
  lo reciba al reconectar y suscribirse. *(Si la telemetría fuera REST en vez de MQTT, el
  aviso viaja en el **body JSON** de la respuesta de telemetría — no en headers, que MQTT
  no tiene.)*
- **Poll diario (red de seguridad).** Una vez al día el device hace
  `GET /firmware/latest?current=X`. Garantiza que se ponga al día **aunque perdiera el
  push** (mensaje no retenido, reinicio inoportuno, bug). Costo ínfimo: un request
  minúsculo al día.

> El push sin el poll es frágil (si se pierde el aviso, el equipo queda desactualizado);
> el poll sin el push funciona pero con hasta 24 h de retraso. **Juntos: rápido y
> robusto.** El backend decide "hay update" comparando `current` (o
> `devices.firmware_version`) contra `firmware_releases` (última versión con
> `min_version <= current`).

### 6.3 Descarga y verificación (device)

Al detectar update por cualquiera de los dos canales:

1. `GET /firmware/latest?current=X` → metadatos (`url`, `sha256`, `signature`).
2. Descarga el `.bin` **en streaming** desde el object storage (`httpUpdate.update()`; no
   cabe entero en RAM).
3. **Verifica `sha256`** (integridad) y **`signature`** con la clave pública embebida
   (autenticidad). Si algo no cuadra, aborta sin escribir.
4. Escribe la partición OTA y reinicia en la versión nueva; el siguiente contacto reporta
   el nuevo `firmware_version` (que a su vez apaga el aviso).

Detalle de TLS/streaming y CA pinning en el C3 → sección *"OTA sobre TLS"* de
[`ARCHITECTURE.md`](ARCHITECTURE.md).

---

## 7. Portabilidad servidor propio ↔ nube

El destino es **servidor propio hoy, cloud-ready mañana**. No cambia el stack; impone
disciplina. Cuatro reglas:

1. **Config 100% por variables de entorno (12-factor).** Nada de endpoints,
   credenciales ni hosts hardcodeados. `.env` distinto por entorno; código igual.
   *Es la regla que más portabilidad compra.*
2. **Todo en Docker.** Corre idéntico en compose (servidor) y en la nube
   (ECS/EKS/Cloud Run/VM).
3. **Interfaces estándar, sin lock-in.** Storage S3-compatible (MinIO ↔ S3 con solo
   cambiar `endpoint_url`); **MQTT plano** (evitar features propietarias tipo AWS IoT
   Core si se quiere poder migrar el broker); Postgres + extensión Timescale estándar.
4. **TLS desacoplado del app.** En servidor lo pone Caddy/Traefik; en nube, un load
   balancer gestionado. FastAPI nunca termina TLS → da igual dónde corra.

### ⚠️ Gotcha crítico: TimescaleDB + nube gestionada
**AWS RDS gestionado NO soporta la extensión TimescaleDB** (GCP/Azure gestionados,
tampoco). Si se migra a base de datos *gestionada*, las opciones son:
- **Timescale Cloud** (managed, la vía limpia), o
- Postgres self-managed en una VM/EC2 (tú corres el contenedor), o
- renunciar a Timescale y usar **particionado nativo de Postgres**.

En servidor propio no hay problema (corre en Docker). Tenerlo presente **antes** de
migrar para no llevarse la sorpresa.

---

## 8. Cómo arrancar

**Estado:** diseño cerrado y sin ambigüedades. El prototipo previo se eliminó; se parte de
cero sobre la infraestructura del compose. Nada queda por decidir.

Lo que **ya está resuelto** y no hay que rehacer:

- El **DDL** del §3, en [`../pythonServer/db/schema.sql`](../pythonServer/db/schema.sql):
  tablas, FKs con `ON DELETE CASCADE`, la extensión, `create_hypertable()` y los índices
  `(device_id, ts DESC)` y `(device_id, applied_at DESC)`. Es la base de la migración inicial.
- Los **contratos de la API** con payloads exactos (§4).
- El **`docker-compose.yml`**: `docker compose up -d` levanta Postgres/Timescale (5432),
  MinIO (9000/9001) y Mosquitto (1883) sin conflictos.

### El esquema lo administra Alembic

Decisión importante para no tener dos dueños del esquema: el compose **ya no** aplica
`schema.sql`. Levanta un Postgres vacío con Timescale disponible, y **Alembic** crea todo.

Alembic mantiene una secuencia versionada de scripts con `upgrade()`/`downgrade()`, y anota
en la tabla `alembic_version` qué revisión está aplicada. Así, `alembic upgrade head` lleva
cualquier entorno —tu máquina, la Raspberry Pi, producción— al mismo esquema de forma
determinista, sin destruir el volumen ni aplicar SQL a mano.

Dos cosas que Alembic **no** hace solo con Timescale y hay que escribir a mano:

```python
def upgrade():
    op.execute("CREATE EXTENSION IF NOT EXISTS timescaledb")
    # ... create_table de users, devices, device_configs, device_telemetry, firmware_releases
    op.execute("SELECT create_hypertable('device_telemetry', 'ts')")
```

`revision --autogenerate` no detecta la extensión ni la hypertable, y `device_telemetry`
necesita la PK compuesta `(id, ts)` porque Timescale exige que la columna de partición
forme parte de la clave.

### Orden de entregables

1. **Migración inicial de Alembic** que reproduzca `schema.sql`, con extensión e hypertable.
2. **Esqueleto FastAPI**: estructura del proyecto, `settings` por variables de entorno
   (§7), modelos SQLAlchemy y `get_db()` async.
3. **Auth de usuario** (§4.2): `register` / `login` / `refresh` con JWT y hash bcrypt.
4. **Provisión de dispositivos** (§4.0, §4.1): `/devices/provision` emite el token y
   persiste solo `token_hash`; dependencia de FastAPI que resuelve `Bearer` → dispositivo.
5. **Config y telemetría por REST** (§4.1): `/devices/{id}/config` y
   `/devices/{id}/telemetry`. Con esto el ciclo dispositivo↔nube ya cierra.
6. **Lecturas para la app** (§4.2): `/me/devices`, `/state` y el histórico con `time_bucket`.
7. **OTA** (§6): `/admin/firmware` sube a MinIO y registra el release; `/firmware/latest`
   resuelve aplicabilidad y devuelve URL prefirmada.
8. **MQTT** (§5): resolver primero la autenticación del broker (ver el caveat), luego el
   consumidor suscrito a `devices/+/telemetry`.
9. **TLS** con Caddy y el servicio `backend` del compose habilitados.

### Checklist de arranque

- [x] `docker compose up -d` levanta Postgres/Timescale, MinIO y Mosquitto sin conflictos.
- [x] Migración inicial de Alembic crea el esquema del §3 (extensión + tablas + hypertable).
- [x] Endpoints de auth (`register`/`login`/`refresh`) emiten y validan JWT.
- [ ] `/devices/provision` emite token de dispositivo y persiste `token_hash`.
- [ ] `/devices/{id}/config` y `/devices/{id}/telemetry` autenticados por `Bearer`.
- [ ] `/me/devices` y `/devices/{id}/state` responden con el aislamiento por cuenta del §4.2.
- [ ] `POST /admin/firmware` (JWT admin) sube el binario al object storage y registra el release.
- [ ] `/firmware/latest` devuelve metadatos y el binario se sirve desde MinIO/S3.
- [ ] Autenticación del broker resuelta y consumidor MQTT insertando en la hypertable.
- [ ] Push OTA por MQTT retenido + poll diario notifican al device (§6.2).
- [ ] Caddy termina TLS y el servicio `backend` corre en el compose.

---

## 9. Referencias cruzadas
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — diseño del firmware; secciones *"Modelado de
  la base de datos (backend futuro)"* y *"TLS en el ESP32-C3"* son el origen de este doc.
- [`API.md`](API.md) — API HTTP del firmware
  (llaves de `/newparams` que `device_configs` debe espejar).
- `README.md` — roadmap del proyecto (el ítem *"Backend (FastAPI + Postgres)"* lo
  desarrolla este documento).

---

## 10. Guía de Despliegue en Raspberry Pi (Pruebas y Producción)

Para correr el proyecto en una Raspberry Pi y garantizar que la configuración sirva en entornos de producción más profesionales sin necesidad de reescribir nada, se ha provisto el archivo `docker-compose.yml` base bajo el enfoque de **Infraestructura como Código (IaC)**.

### Prerrequisitos de Hardware
1. **Sistema Operativo 64-bit:** Es **obligatorio** usar Raspberry Pi OS de 64 bits para tener soporte nativo con las imágenes oficiales de TimescaleDB y MinIO en arquitectura ARM64.
2. **SSD Externo (Crítico):** **NO utilices una tarjeta MicroSD** para almacenar la base de datos de telemetría. Las escrituras constantes por MQTT corromperán la tarjeta rápidamente. Usa un disco SSD conectado por USB y asegúrate de que los volúmenes de Docker se alojen en este almacenamiento.
3. **RAM:** Se recomienda encarecidamente una Raspberry Pi 4 o 5 de al menos 4 GB de RAM.

### Despliegue
1. Dirígete a la carpeta `pythonServer/`.
2. Copia el archivo `.env.example` a `.env` y configura tus secretos (especialmente contraseñas de BD y MinIO). **Nunca subas este archivo al repositorio**.
3. Revisa la configuración base del broker en `pythonServer/mosquitto/config/mosquitto.conf`.
4. Ejecuta: `docker-compose up -d`.

### De Pruebas a Producción
La gran ventaja de este diseño "Cloud-Ready" es que cuando desees llevar este backend a un entorno en la nube público (AWS, DigitalOcean, GCP):
- Solo deberás copiar el archivo `docker-compose.yml` y tu `.env` (con claves de producción seguras).
- Ajustarás `Caddy` para proveer los certificados de Let's Encrypt para tu dominio de producción.
- **Tu código y arquitectura no sufrirán modificaciones.**
