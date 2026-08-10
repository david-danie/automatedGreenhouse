# Backend SmartPlant — Diseño y guía de arranque

> **Propósito de este documento.** Fijar las decisiones de arquitectura del backend
> para que la próxima sesión (humana o IA) pueda **arrancar la implementación sin
> recontexto**. Todo lo que sigue está **confirmado** salvo lo marcado explícitamente
> como "pendiente". Complementa la sección *"Modelado de la base de datos (backend
> futuro)"* y *"TLS en el ESP32-C3"* de [`ARCHITECTURE.md`](ARCHITECTURE.md), que
> siguen siendo la referencia del lado del firmware.

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
| **Ingesta de telemetría** | **MQTT sobre TLS** (broker **EMQX**) | Conexión persistente: amortiza el costoso handshake TLS del C3. REST/HTTPS solo para acciones puntuales |
| **Storage OTA** | **S3-compatible**: **MinIO** local + **S3** en nube | Misma API (`aioboto3`); solo cambia `endpoint_url`. Binarios fuera de la BD |
| **Auth apps** | **JWT propio** (access + refresh) | `1 usuario = 1 cuenta`, login simple; no hace falta proveedor gestionado |
| **Auth dispositivos** | Token de dispositivo revocable | Aprovisionamiento `user+pass+mac` sobre TLS → backend emite token; se guarda `token_hash` |
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
> `/newparams` del firmware (ver `HTML/test/rutas_y_parametros.txt` y `Plant.cpp`). Si cambia
> el formulario del portal, cambia esta tabla.

> **`firmware_version` aparece en dos lugares a propósito, no por error:**
> `devices.firmware_version` guarda la versión **vigente** del dispositivo (para
> decidir OTA en `/firmware/latest?current=X`); `device_telemetry.firmware_version`
> estampa la versión **en cada lectura**, para poder analizar el histórico sabiendo
> qué firmware lo produjo (p. ej. detectar que un sensor empezó a reportar raro tras
> una actualización).

---

## 4. Contratos de la API (esquema, pendiente de detallar payloads)

Dos superficies distintas:

### 4.1 Device-facing (ESP32-C3 → backend)
Autenticadas con el **token de dispositivo** (salvo la provisión, que usa user+pass+mac).

| Método | Ruta | Propósito |
|---|---|---|
| POST | `/devices/provision` | `{user, pass, mac}` sobre TLS → valida cuenta, crea/vincula `devices`, **emite token**. Único momento en que viaja la contraseña |
| POST | `/devices/{id}/config` | `{token, …params}` → inserta fila en `device_configs` |
| POST | `/devices/{id}/telemetry` | `{token, …}` → inserta en `device_telemetry`. **Vía MQTT** en régimen continuo; REST como fallback puntual |
| GET | `/firmware/latest?current=X` | Metadatos del release aplicable → device descarga el binario firmado **en streaming** |

### 4.2 App-facing (móvil/tablet → backend)
Autenticadas con **JWT de usuario**.

| Método | Ruta | Propósito |
|---|---|---|
| POST | `/auth/register` · `/auth/login` | Alta y login → devuelven access + refresh JWT |
| GET | `/me/devices` | Lista de dispositivos de la cuenta |
| GET | `/devices/{id}/state` | Última config + telemetría reciente |
| GET | `/devices/{id}/telemetry?from&to&bucket` | Histórico agregado (aprovecha *continuous aggregates* de Timescale) |

### 4.3 Admin-facing (staff del fabricante → backend)
Autenticadas con **JWT de usuario con `is_admin = true`**. Publican firmware (ver §6.1).

| Método | Ruta | Propósito |
|---|---|---|
| POST | `/admin/firmware` | Multipart: `.bin` + `version` + `min_version` + `signature` → sube el binario al object storage y registra el release |
| GET | `/admin/firmware` | Lista releases publicados (catálogo `firmware_releases`) |

> **Pendiente:** aterrizar los payloads JSON exactos (request/response) de cada ruta.
> Deben espejar las llaves del firmware donde aplique (ver §3).

---

## 5. Ingesta MQTT (telemetría)

- **Broker:** EMQX (auth por dispositivo + ACLs por tópico). Mosquitto si se busca lo mínimo.
- **Patrón:** el device mantiene **una** conexión MQTT/TLS persistente y publica
  telemetría a un tópico por dispositivo (p. ej. `devices/{id}/telemetry`).
- Un **consumidor** (servicio Python, puede vivir junto al API o aparte) se suscribe
  e inserta en la hypertable `device_telemetry`.
- **Por qué MQTT y no REST para esto:** el handshake TLS es lo caro en el C3; una
  conexión persistente lo amortiza. Detalle y trade-offs de TLS/memoria en el C3 →
  sección *"TLS en el ESP32-C3"* de [`ARCHITECTURE.md`](ARCHITECTURE.md).
- **Regla de oro del firmware:** *sin Wi-Fi → cero tráfico*; *sin token y sin acción
  del usuario → silencio total* (no hay polling de entitlement). Ver ARCHITECTURE.md.

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

## 8. Cómo arrancar (próxima sesión)

Estado actual: **diseño cerrado, sin código todavía.** No queda nada por confirmar.
Orden sugerido de entregables:

1. **DDL SQL completo** — tablas del §3 + convertir `device_telemetry` en hypertable
   (`SELECT create_hypertable(...)`) + FKs + índices (`device_id`, `ts`). Base para
   la primera migración de Alembic.
2. **Contratos de endpoints** (§4) con payloads JSON exactos — request/response de
   cada ruta, espejando las llaves del firmware donde aplique.
3. **`docker-compose.yml`** del stack local: FastAPI + Postgres/Timescale + EMQX +
   MinIO + Caddy, cableados por variables de entorno (§7).
4. **Esqueleto FastAPI**: estructura de proyecto, settings por env, auth JWT,
   modelos SQLAlchemy, y el consumidor MQTT.

### Checklist de arranque
- [x] `docker-compose up` levanta Postgres/Timescale, Mosquitto, MinIO y Caddy.
- [ ] Migración inicial de Alembic crea el esquema del §3 (con la hypertable).
- [ ] Endpoints de auth (register/login) emiten y validan JWT.
- [ ] `/devices/provision` emite token de dispositivo y persiste `token_hash`.
- [ ] Consumidor MQTT suscrito a `devices/+/telemetry` inserta en la hypertable.
- [ ] `/firmware/latest` devuelve metadatos y el binario se sirve desde MinIO/S3.
- [ ] `POST /admin/firmware` (JWT admin) sube el binario al object storage y registra el release.
- [ ] Push OTA por MQTT retenido + poll diario notifican al device (§6.2).

---

## 9. Referencias cruzadas
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — diseño del firmware; secciones *"Modelado de
  la base de datos (backend futuro)"* y *"TLS en el ESP32-C3"* son el origen de este doc.
- [`../HTML/test/rutas_y_parametros.txt`](../HTML/test/rutas_y_parametros.txt) — API HTTP del firmware
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
