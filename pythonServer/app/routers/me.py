"""Endpoints app-facing (móvil/tablet → backend), autenticados con JWT de usuario.

Contratos en el §4.2 de docs/BACKEND.md:

- `GET /me/devices`            : lista los dispositivos de la cuenta autenticada.
- `GET /devices/{id}/state`    : última config + telemetría más reciente + edad del cultivo.
- `GET /devices/{id}/telemetry?from&to&bucket` : histórico agregado con `time_bucket`.

Aislamiento por cuenta: un dispositivo que no pertenece al usuario responde `404`
(no `403`), para no revelar su existencia (§4.2).
"""

from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, HTTPException, Path, Query, status
from pydantic import BaseModel
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.deps import get_current_user
from app.models import Device, DeviceConfig, DeviceTelemetry, User

router = APIRouter(tags=["app"])

# Un dispositivo se considera "online" si contactó dentro de esta ventana. Es un
# valor derivado de last_seen_at, no una columna (§4.2).
ONLINE_WINDOW = timedelta(minutes=5)

# El día 0 del "epoch de días" del firmware es 1970-01-01. crop_start_day se
# guarda como daysSinceEpoch(fecha de anclaje); ver "Edad del cultivo" en
# ARCHITECTURE.md. Se deriva aquí con la MISMA regla que el firmware.
_EPOCH_DATE = datetime(1970, 1, 1, tzinfo=timezone.utc)

# Anchos de agregación soportados para el histórico. La clave es lo que manda el
# cliente; el valor es un timedelta que asyncpg mapea nativamente a `interval`
# de Postgres (pasar un string haría fallar a time_bucket por tipo).
_BUCKETS = {"5m": timedelta(minutes=5), "1h": timedelta(hours=1), "1d": timedelta(days=1)}


def _is_online(last_seen_at: datetime | None) -> bool:
    if last_seen_at is None:
        return False
    return datetime.now(timezone.utc) - last_seen_at <= ONLINE_WINDOW


def _crop_age(crop_start_day: int | None) -> dict:
    """Deriva `dia`/`semana` desde crop_start_day, igual que el firmware.

    `dia = daysSinceEpoch(hoy) − crop_start_day + 1` (el día del ancla es el 1);
    `semana = (dia − 1) // 7 + 1`. Antes de anclar o si la fecha es futura, `0`.
    """
    if not crop_start_day:
        return {"dia": 0, "semana": 0}
    days_since_epoch = (datetime.now(timezone.utc) - _EPOCH_DATE).days
    dia = days_since_epoch - crop_start_day + 1
    if dia < 1:
        return {"dia": 0, "semana": 0}
    semana = (dia - 1) // 7 + 1
    return {"dia": dia, "semana": semana}


async def _owned_device_or_404(device_id: int, user: User, db: AsyncSession) -> Device:
    """Trae el dispositivo SOLO si pertenece al usuario; si no, 404.

    El filtro por user_id va en el WHERE: si el id existe pero es de otra cuenta,
    el resultado es vacío y se responde 404 igual que si no existiera (§4.2).
    """
    result = await db.execute(
        select(Device).where(Device.id == device_id, Device.user_id == user.id)
    )
    device = result.scalar_one_or_none()
    if device is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Dispositivo no encontrado")
    return device


# --- Schemas ---


class DeviceSummary(BaseModel):
    id: int
    mac: str
    name: str | None
    firmware_version: str | None
    last_seen_at: datetime | None
    online: bool


class DevicesResponse(BaseModel):
    devices: list[DeviceSummary]


# --- Endpoints ---


@router.get("/me/devices", response_model=DevicesResponse)
async def list_my_devices(
    user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Lista los dispositivos de la cuenta autenticada."""
    result = await db.execute(
        select(Device).where(Device.user_id == user.id).order_by(Device.id)
    )
    devices = result.scalars().all()
    return DevicesResponse(
        devices=[
            DeviceSummary(
                id=d.id,
                mac=d.mac,
                name=d.name,
                firmware_version=d.firmware_version,
                last_seen_at=d.last_seen_at,
                online=_is_online(d.last_seen_at),
            )
            for d in devices
        ]
    )


@router.get("/devices/{device_id}/state")
async def device_state(
    device_id: int = Path(..., description="devices.id (entero)"),
    user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Última config conocida + lectura más reciente + edad del cultivo."""
    device = await _owned_device_or_404(device_id, user, db)

    # Última config (la fila más reciente por applied_at es la vigente).
    cfg_result = await db.execute(
        select(DeviceConfig)
        .where(DeviceConfig.device_id == device.id)
        .order_by(DeviceConfig.applied_at.desc())
        .limit(1)
    )
    cfg = cfg_result.scalar_one_or_none()

    # Última telemetría.
    tel_result = await db.execute(
        select(DeviceTelemetry)
        .where(DeviceTelemetry.device_id == device.id)
        .order_by(DeviceTelemetry.ts.desc())
        .limit(1)
    )
    tel = tel_result.scalar_one_or_none()

    config_block = None
    crop_block = {"dia": 0, "semana": 0}
    if cfg is not None:
        config_block = {
            "planta": cfg.planta,
            "enable": cfg.enable,
            "fp_on": cfg.fp_on,
            "fp_off": cfg.fp_off,
            "led_a": cfg.led_a,
            "led_r": cfg.led_r,
            "led_b": cfg.led_b,
            "irr_h": cfg.irr_h,
            "irr_m": cfg.irr_m,
            "vent_h": cfg.vent_h,
            "vent_m": cfg.vent_m,
            "applied_at": cfg.applied_at,
        }
        crop_block = _crop_age(cfg.crop_start_day)

    telemetry_block = None
    if tel is not None:
        telemetry_block = {
            "ts": tel.ts,
            "temp": tel.temp,
            "humidity": tel.humidity,
            "wifi_rssi": tel.wifi_rssi,
        }

    return {
        "device": {
            "id": device.id,
            "name": device.name,
            "online": _is_online(device.last_seen_at),
            "firmware_version": device.firmware_version,
            "last_seen_at": device.last_seen_at,
        },
        "config": config_block,
        "crop": crop_block,
        "telemetry": telemetry_block,
    }


@router.get("/devices/{device_id}/telemetry")
async def device_telemetry_history(
    device_id: int = Path(..., description="devices.id (entero)"),
    from_: datetime | None = Query(None, alias="from", description="ISO 8601 inicio del rango"),
    to: datetime | None = Query(None, description="ISO 8601 fin del rango"),
    bucket: str = Query("1h", description="Ancho de agregación: 5m, 1h o 1d"),
    user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    """Histórico agregado con `time_bucket()` de Timescale.

    `from`/`to` en ISO 8601; `bucket` uno de {5m, 1h, 1d}. Devuelve promedios,
    mínimos, máximos y conteo de muestras por bucket. `400` si el bucket no está
    soportado o el rango es inválido (from > to).
    """
    device = await _owned_device_or_404(device_id, user, db)

    if bucket not in _BUCKETS:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"bucket no soportado; usa uno de {sorted(_BUCKETS)}",
        )
    if from_ is not None and to is not None and from_ > to:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="rango inválido: 'from' es posterior a 'to'",
        )

    # time_bucket() de Timescale espera su primer argumento como `interval`.
    # _BUCKETS[bucket] es un timedelta, que asyncpg envía como `interval` nativo.
    bucket_expr = func.time_bucket(_BUCKETS[bucket], DeviceTelemetry.ts).label("bucket")
    query = (
        select(
            bucket_expr,
            func.avg(DeviceTelemetry.temp).label("temp_avg"),
            func.min(DeviceTelemetry.temp).label("temp_min"),
            func.max(DeviceTelemetry.temp).label("temp_max"),
            func.avg(DeviceTelemetry.humidity).label("humidity_avg"),
            func.count().label("samples"),
        )
        .where(DeviceTelemetry.device_id == device.id)
        .group_by(bucket_expr)
        .order_by(bucket_expr)
    )
    if from_ is not None:
        query = query.where(DeviceTelemetry.ts >= from_)
    if to is not None:
        query = query.where(DeviceTelemetry.ts <= to)

    rows = (await db.execute(query)).all()

    def _round(v):
        return round(v, 2) if v is not None else None

    series = [
        {
            "ts": r.bucket,
            "temp_avg": _round(r.temp_avg),
            "temp_min": _round(r.temp_min),
            "temp_max": _round(r.temp_max),
            "humidity_avg": _round(r.humidity_avg),
            "samples": r.samples,
        }
        for r in rows
    ]

    return {"device_id": device.id, "bucket": bucket, "series": series}
