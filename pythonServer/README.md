# SmartPlant — Backend

Servicio de nube para SmartPlant: cuentas de usuario, vinculación de dispositivos, telemetría y distribución de firmware OTA.

> **Estado: auth, provisión, config, telemetría y lecturas para la app operativas y verificadas.**
>
> El backend corre en Docker junto a la infraestructura. La migración de Alembic crea el
> esquema completo (5 tablas + hypertable), los endpoints de autenticación están operativos
> y `POST /devices/provision` ya vincula dispositivos y emite su token (todo verificado
> end-to-end).
>
> El ciclo dispositivo↔nube por REST está cerrado y verificado: `POST /devices/{id}/config`
> (histórico) y `POST /devices/{id}/telemetry` (`ts` server-side, refresca presencia). Y las
> **lecturas para la app** (§4.2) también: `GET /me/devices`, `GET /devices/{id}/state`
> (con edad de cultivo derivada) y `GET /devices/{id}/telemetry` (histórico agregado con
> `time_bucket`), todas con **aislamiento por cuenta** (404, no 403, si el device es de otra).
>
> **El diseño completo a seguir implementando está en [`../docs/BACKEND.md`](../docs/BACKEND.md).**

---

## Qué hay aquí

```
pythonServer/
├── Dockerfile                # Imagen del backend (Python 3.11 slim + uvicorn)
├── docker-compose.yml        # Stack completo: DB + MinIO + Mosquitto + backend
├── alembic.ini               # Configuración de Alembic
├── alembic/                  # Migraciones
│   ├── env.py                # Entorno async para migraciones
│   └── versions/
│       └── 001_initial_schema.py  # Esquema inicial (extensión + tablas + hypertable)
├── app/                      # Código de la API
│   ├── main.py               # FastAPI app + health endpoint
│   ├── settings.py           # Config por variables de entorno (12-factor)
│   ├── database.py           # Conexión async SQLAlchemy
│   ├── models.py             # Modelos ORM (users, devices, configs, telemetry, firmware)
│   ├── security.py           # Hashing bcrypt + generación de tokens (compartido)
│   ├── deps.py               # Dependencias de auth (Bearer → dispositivo; JWT → usuario)
│   └── routers/
│       ├── auth.py           # POST /auth/register, /auth/login, /auth/refresh
│       ├── devices.py        # POST /devices/provision, /{id}/config, /{id}/telemetry
│       └── me.py             # GET /me/devices, /devices/{id}/state, /devices/{id}/telemetry
├── db/schema.sql             # DDL de referencia (la fuente real es Alembic)
├── mosquitto/config/         # Configuración del broker
├── requirements.txt          # Dependencias con versiones clave pinneadas
├── .env.example              # Plantilla de variables de entorno
└── README.md
```

---

## Levantar el stack

```bash
cd pythonServer
cp .env.example .env          # Ajusta secretos para tu entorno
docker compose up -d --build  # Construye el backend y levanta todo
docker compose exec backend alembic upgrade head  # Crea el esquema
```

Deja corriendo cuatro servicios:

| Servicio | Puerto | Qué es |
|---|---|---|
| `backend` | 8000 | API FastAPI (SmartPlant) |
| `db` | 5432 | Postgres 15 + TimescaleDB 2.14.2 |
| `minio` | 9000 · 9001 | Object storage S3 (API · consola web) |
| `mosquitto` | 1883 | Broker MQTT |

Consola de MinIO en `http://localhost:9001` (usuario/contraseña por defecto: `admin` / `admin123456`).

Documentación OpenAPI automática en `http://localhost:8000/docs`.

### Endpoints disponibles

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/health` | Health check |
| POST | `/auth/register` | Crear cuenta (devuelve JWT) |
| POST | `/auth/login` | Autenticar (devuelve JWT) |
| POST | `/auth/refresh` | Renovar access token |
| POST | `/devices/provision` | Vincular dispositivo a una cuenta y emitir su token |
| POST | `/devices/{id}/config` | Registrar config del cultivo (Bearer) |
| POST | `/devices/{id}/telemetry` | Registrar lectura de telemetría (Bearer) |
| GET | `/me/devices` | Listar dispositivos de la cuenta (JWT usuario) |
| GET | `/devices/{id}/state` | Última config + telemetría + edad del cultivo (JWT usuario) |
| GET | `/devices/{id}/telemetry` | Histórico agregado con `time_bucket` (JWT usuario) |

### El esquema lo administra Alembic

`schema.sql` es material de referencia. El dueño real del esquema es Alembic:

```bash
docker compose exec backend alembic upgrade head   # Aplica migraciones pendientes
docker compose exec backend alembic downgrade -1   # Revierte la última
```

La migración `001_initial_schema.py` crea la extensión TimescaleDB, las 5 tablas del modelo
de datos, la hypertable sobre `device_telemetry` y los índices recomendados.

Detalle de por qué Alembic no autogenera la extensión ni `create_hypertable()` en el
[§8 de `BACKEND.md`](../docs/BACKEND.md).

---

## Siguiente paso

**OTA (entregable 7 del §8).** `POST /admin/firmware` (JWT admin) sube el binario a MinIO y
registra el release; `GET /firmware/latest?current=…` resuelve aplicabilidad y devuelve una
URL prefirmada. Después vienen MQTT (entregable 8) y TLS/Caddy (entregable 9).

Nota de entorno para levantar el stack en esta máquina: los puertos **5432 y 8000 del host
están ocupados** por otro proyecto (`dbjaguar`, `jaguar`). El `docker-compose.override.yml`
los remapea a **5433** y **8001** con `ports: !override` (sin esa directiva, Compose
**concatena** las listas de puertos en vez de reemplazarlas). El backend queda en
`http://localhost:8001`.

---

## Configuración

Todo por variables de entorno, sin nada hardcodeado (regla 1 del §7 de `BACKEND.md`).
Copia `.env.example` a `.env` y ajusta:

| Variable | Default (desarrollo) | Notas |
|---|---|---|
| `POSTGRES_USER` | `smartplant` | |
| `POSTGRES_PASSWORD` | `smartplant_secret` | |
| `POSTGRES_DB` | `smartplant_db` | |
| `DATABASE_URL` | `postgresql+asyncpg://...` | La usa el backend |
| `JWT_SECRET_KEY` | `change-me-in-production` | **Cambiar obligatorio** |
| `MINIO_ROOT_USER` | `admin` | |
| `MINIO_ROOT_PASSWORD` | `admin123456` | |
| `MQTT_HOST` | `mosquitto` | |

**Los defaults son solo para desarrollo local.** Para cualquier despliegue real, definirlos
en un `.env` fuera del control de versiones.

---

## Dos cosas pendientes de resolver

**Autenticación del broker.** Mosquitto está hoy con `allow_anonymous true`: cualquiera en
la red puede publicar y suscribirse. Sirve para desarrollo, no para nada expuesto. Las tres
salidas y la recomendación (arrancar la telemetría por REST) están en el
[caveat del §5 de `BACKEND.md`](../docs/BACKEND.md#caveat-mosquitto-no-autentica-contra-la-base-de-datos).

**Caddy + TLS.** El servicio `caddy` queda comentado en el compose hasta que se tenga un
dominio y un `Caddyfile` configurado.

---

## Cómo se autentican los dispositivos

Resumen; el detalle y los payloads están en el
[§4.0 de `BACKEND.md`](../docs/BACKEND.md).

El dispositivo se provisiona **una vez** enviando `{email, pass, mac}` sobre TLS a
`POST /devices/provision`. Es el único momento en que la contraseña del usuario viaja. El
backend valida la cuenta, vincula la fila en `devices` y emite un **token de dispositivo**;
de ahí en adelante el ESP32 usa `Authorization: Bearer <token>` y nunca almacena la
contraseña del usuario.

En la base se guarda solo `devices.token_hash`, así que una filtración no entrega
credenciales funcionales, y el token es revocable borrando o rotando esa fila.

Ojo con no confundirlo con el token de sesión del portal local del firmware, que es otra
cosa (RAM, 30 min, red local). La tabla comparativa está en el §4.0.

---

## Documentación relacionada

- **[`../docs/BACKEND.md`](../docs/BACKEND.md)** — el diseño a implementar. Referencia única.
- **[`../docs/API.md`](../docs/API.md)** — API HTTP del **dispositivo**, un contrato distinto
  al de este backend. Define las llaves que `device_configs` espeja.
- **[`../docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md)** — firmware; secciones de TLS en el
  ESP32-C3 y OTA sobre TLS.
