# Python Backend

> **Esqueleto — por completar.** Este skill define el alcance y las reglas base del backend de
> SmartPlant. Las secciones marcadas *(por completar)* se desarrollarán cuando se retome el
> trabajo de backend (actualmente la prioridad es la migración del firmware a ESP-IDF).

## Purpose

Guiar el desarrollo, revisión y depuración del backend de SmartPlant: la nube que gestiona
cuentas de usuario, vinculación de dispositivos, telemetría y distribución de firmware OTA.

Stack real del proyecto (ver [`docs/BACKEND.md`](../../../docs/BACKEND.md) como referencia
autoritativa y [`pythonServer/README.md`](../../../pythonServer/README.md) para levantarlo):

* **FastAPI** (Python) — API REST
* **PostgreSQL + TimescaleDB** — datos relacionales + hypertable de telemetría
* **Alembic** — migraciones de esquema
* **JWT** — autenticación (access/refresh), con flag `is_admin`
* **MQTT (Mosquitto)** — ingesta de telemetría (diseñada, aún no implementada)
* **S3-compatible (MinIO)** — almacenamiento de binarios de firmware para OTA
* **Docker Compose** — infraestructura local

El objetivo es producir backend **correcto, seguro, con aislamiento por cuenta, verificado y
coherente con el contrato ya documentado**, sin reescribir lo que ya funciona.

---

## Reglas base (vigentes desde ya)

1. **`docs/BACKEND.md` es la referencia autoritativa.** Antes de implementar un endpoint,
   leer su contrato ahí (ruta, payload, códigos de estado, autorización). No inventar
   contratos nuevos; si falta algo, proponerlo y confirmarlo.
2. **Aislamiento por cuenta.** Un recurso de otra cuenta responde **404, no 403** (no revelar
   existencia). Patrón ya establecido en las lecturas de `/me/devices`, `/devices/{id}/...`.
3. **No romper lo verificado.** Auth, provisión, config, telemetría y lecturas para la app
   ya están operativas y verificadas end-to-end: preservar sus interfaces.
4. **Secretos fuera del repo.** `pythonServer/.env` está en `.gitignore` (JWT_SECRET_KEY,
   credenciales de Postgres/MinIO). Nunca hard-codear ni commitear secretos.
5. **Migraciones por Alembic.** Cambios de esquema vía migración, nunca a mano sobre la BD.
6. **Verificar antes de declarar hecho.** Levantar el stack en Docker y probar el endpoint
   (o indicar explícitamente qué quedó sin probar).

---

## Áreas a desarrollar *(por completar)*

- **Diseño de endpoints**: convenciones de FastAPI (routers, deps, modelos Pydantic, códigos
  de estado, manejo de errores) *(por completar)*.
- **Modelo de datos y Alembic**: tablas, hypertable de telemetría, versionado de migraciones,
  compatibilidad hacia atrás *(por completar)*.
- **Autenticación/autorización**: JWT access/refresh, tokens de dispositivo revocables,
  `is_admin`, aislamiento por cuenta *(por completar)*.
- **OTA (entregable pendiente)**: `POST /admin/firmware`, `GET /admin/firmware`,
  `GET /firmware/latest` con comparador SemVer, URLs prefirmadas de S3/MinIO, `sha256`
  server-side *(por completar — contrato en `docs/BACKEND.md` §4)*.
- **Ingesta MQTT**: suscripción a `devices/+/telemetry`, validación, persistencia en la
  hypertable *(por completar — diseño en `docs/BACKEND.md` §5)*.
- **Pruebas**: estrategia de tests (unitarios de validación/lógica, integración contra la BD
  en Docker) *(por completar)*.
- **Despliegue**: Docker Compose, variables de entorno, TLS/Caddy a futuro *(por completar)*.

---

## Reporte de cambios

Al terminar una modificación, reportar: **qué cambió**, **por qué**, **qué se verificó**
(y qué no), y **riesgos/pendientes**. Ser conciso y técnicamente específico.
