# SmartPlant — Backend

Servicio de nube para SmartPlant: cuentas de usuario, vinculación de dispositivos, telemetría y distribución de firmware OTA.

> **Estado: infraestructura lista, código por escribir.**
>
> El prototipo anterior se eliminó por completo para que no hubiera dos fuentes de verdad.
> Lo que queda en esta carpeta es la infraestructura local y el DDL de referencia.
>
> **El diseño a implementar está en [`../docs/BACKEND.md`](../docs/BACKEND.md)**: stack,
> modelo de datos, contratos de API con payloads exactos, flujo OTA y orden de entregables.
> Empieza por ahí.

---

## Qué hay aquí

```
pythonServer/
├── docker-compose.yml        # Infraestructura local (funciona hoy)
├── db/schema.sql             # DDL de referencia → base de la 1ª migración de Alembic
├── mosquitto/config/         # Configuración del broker
├── requirements.txt          # Dependencias previstas (sin versiones fijadas aún)
└── README.md
```

No hay código de aplicación. El esqueleto de FastAPI es el entregable 2 del
[§8 de `BACKEND.md`](../docs/BACKEND.md).

---

## Levantar la infraestructura

```bash
cd pythonServer
docker compose up -d
```

Deja corriendo tres servicios, sin conflictos de puerto:

| Servicio | Puerto | Qué es |
|---|---|---|
| `db` | 5432 | Postgres 15 + TimescaleDB |
| `minio` | 9000 · 9001 | Object storage S3 (API · consola web) |
| `mosquitto` | 1883 | Broker MQTT |

Consola de MinIO en `http://localhost:9001` (usuario/contraseña por defecto: `admin` / `admin123456`).

Los servicios `backend` y `caddy` están comentados a propósito: requieren un `Dockerfile` y
un `Caddyfile` que todavía no existen. `docker compose up` tiene que funcionar hoy.

### El esquema lo crea Alembic, no el compose

Al levantar, `db` queda **vacío** con la extensión de Timescale disponible. `schema.sql`
**ya no** se aplica automáticamente: es material de referencia para escribir la primera
migración. Así hay un único dueño del esquema y `alembic upgrade head` es lo que lleva
cualquier entorno al mismo estado.

Detalle de por qué, y las dos cosas que Alembic no autogenera con Timescale (la extensión y
`create_hypertable()`), en el [§8 de `BACKEND.md`](../docs/BACKEND.md).

---

## Configuración

Todo por variables de entorno, sin nada hardcodeado (regla 1 del §7 de `BACKEND.md`). El
compose lee un `.env` opcional en esta carpeta:

| Variable | Default (desarrollo) |
|---|---|
| `POSTGRES_USER` | `smartplant` |
| `POSTGRES_PASSWORD` | `smartplant_secret` |
| `POSTGRES_DB` | `smartplant_db` |
| `MINIO_ROOT_USER` | `admin` |
| `MINIO_ROOT_PASSWORD` | `admin123456` |

**Los defaults son solo para desarrollo local.** Para cualquier despliegue real, definirlos
en un `.env` fuera del control de versiones. El `.gitignore` de la raíz ya cubre `.env` y
`.env.*`.

---

## Dos cosas a resolver temprano

**Autenticación del broker.** Mosquitto está hoy con `allow_anonymous true`: cualquiera en
la red puede publicar y suscribirse. Sirve para desarrollo, no para nada expuesto. A
diferencia de EMQX, Mosquitto no valida contra la base de datos de forma nativa. Las tres
salidas y la recomendación (arrancar la telemetría por REST) están en el
[caveat del §5 de `BACKEND.md`](../docs/BACKEND.md#caveat-mosquitto-no-autentica-contra-la-base-de-datos).

**Versiones de dependencias.** `requirements.txt` lista los paquetes correctos pero sin
fijar versiones. Conviene pinnearlas al crear el entorno para que sea reproducible.

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
