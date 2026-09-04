"""Endpoints de dispositivos.

- `POST /provision`: el ESP32 envía las credenciales del usuario junto con su MAC y
  recibe a cambio un token de dispositivo (§4.0/§4.1 de docs/BACKEND.md).
- `POST /{device_id}/config`: registra la config vigente del cultivo (histórico).
- `POST /{device_id}/telemetry`: registra una lectura de telemetría.

Los dos últimos se autentican con el token de dispositivo (Bearer) vía
`get_current_device`; el device solo puede escribir en su propia fila.
"""

from datetime import datetime, timezone

from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, EmailStr, Field
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.deps import get_current_device
from app.models import Device, DeviceConfig, DeviceTelemetry, User
from app.security import generate_device_token, hash_secret, verify_secret

router = APIRouter(prefix="/devices", tags=["devices"])


# --- Schemas ---


class ProvisionRequest(BaseModel):
    email: EmailStr
    # El firmware envía la contraseña con la llave `pass`; en Python `pass` es
    # palabra reservada, así que se mapea con alias a un atributo válido.
    password: str = Field(alias="pass")
    # MAC en formato AA:BB:CC:DD:EE:FF (17 caracteres).
    mac: str = Field(min_length=17, max_length=17)


class ProvisionResponse(BaseModel):
    device_id: int
    # El token se devuelve UNA sola vez: en la BD solo queda su hash.
    token: str
    name: str | None = None


# --- Endpoints ---


@router.post(
    "/provision",
    response_model=ProvisionResponse,
    status_code=status.HTTP_201_CREATED,
)
async def provision(body: ProvisionRequest, db: AsyncSession = Depends(get_db)):
    """Vincula un dispositivo a una cuenta y emite su token.

    Es el único momento en que la contraseña del usuario viaja por la red. El
    backend valida la cuenta, crea o revincula la fila en `devices` y devuelve un
    token nuevo; en la base solo se persiste `token_hash` (revocable).
    """
    # 1. Validar credenciales del usuario.
    result = await db.execute(select(User).where(User.email == body.email))
    user = result.scalar_one_or_none()
    if not user or not verify_secret(body.password, user.password_hash):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Credenciales inválidas",
        )

    # 2. Emitir el token de dispositivo y guardar solo su hash.
    token = generate_device_token()
    token_hash = hash_secret(token)

    # 3. ¿La MAC ya existe? Determina si es alta o re-provisión.
    result = await db.execute(select(Device).where(Device.mac == body.mac))
    device = result.scalar_one_or_none()

    if device is None:
        # Alta: dispositivo nuevo vinculado a esta cuenta.
        device = Device(mac=body.mac, user_id=user.id, token_hash=token_hash)
        db.add(device)
    elif device.user_id != user.id:
        # La MAC pertenece a otra cuenta: no se roba ni se revincula.
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="La MAC ya está vinculada a otra cuenta",
        )
    else:
        # Re-provisión del mismo dueño: se rota el token.
        device.token_hash = token_hash

    await db.commit()
    await db.refresh(device)

    return ProvisionResponse(device_id=device.id, token=token, name=device.name)



# ---------------------------------------------------------------------------
#  Config y telemetría (device-facing, autenticadas por token de dispositivo)
# ---------------------------------------------------------------------------


class ConfigRequest(BaseModel):
    """Config vigente del cultivo. Las llaves espejan las de /newparams del
    firmware, en snake_case. `led_b` es 0/1 (el LED blanco es ON/OFF)."""
    planta: str
    enable: bool
    fp_on: int = Field(ge=0, le=23)
    fp_off: int = Field(ge=0, le=23)
    led_a: int = Field(ge=0, le=100)
    led_r: int = Field(ge=0, le=100)
    led_b: int = Field(ge=0, le=1)
    irr_h: int = Field(ge=0)
    irr_m: int = Field(ge=0, le=59)
    vent_h: int = Field(ge=0)
    vent_m: int = Field(ge=0, le=59)
    crop_start_day: int | None = None


class ConfigResponse(BaseModel):
    config_id: int
    applied_at: datetime


class TelemetryRequest(BaseModel):
    """Una lectura de telemetría. Todos los campos son opcionales, pero debe
    llegar al menos uno (ver validador). `ts` lo pone el servidor."""
    temp: float | None = None
    humidity: float | None = None
    wifi_rssi: int | None = None
    uptime: int | None = None
    firmware_version: str | None = None


class TelemetryResponse(BaseModel):
    accepted: int


@router.post(
    "/{device_id}/config",
    response_model=ConfigResponse,
    status_code=status.HTTP_201_CREATED,
)
async def post_config(
    body: ConfigRequest,
    device: Device = Depends(get_current_device),
    db: AsyncSession = Depends(get_db),
):
    """Registra la config vigente del cultivo. Inserta una fila NUEVA en
    `device_configs` (histórico): no actualiza la anterior. La última fila es la
    config vigente."""
    config = DeviceConfig(device_id=device.id, **body.model_dump())
    db.add(config)
    await db.commit()
    await db.refresh(config)
    return ConfigResponse(config_id=config.id, applied_at=config.applied_at)


@router.post(
    "/{device_id}/telemetry",
    response_model=TelemetryResponse,
    status_code=status.HTTP_202_ACCEPTED,
)
async def post_telemetry(
    body: TelemetryRequest,
    device: Device = Depends(get_current_device),
    db: AsyncSession = Depends(get_db),
):
    """Registra una lectura de telemetría en la hypertable. El `ts` lo pone el
    servidor (no se depende del reloj del dispositivo). De paso, refresca
    `last_seen_at` y la `firmware_version` vigente del dispositivo."""
    fields = body.model_dump(exclude_none=True)
    if not fields:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Debe llegar al menos un campo de telemetría",
        )

    reading = DeviceTelemetry(device_id=device.id, **fields)
    db.add(reading)

    # El contacto del dispositivo actualiza su presencia y versión vigente.
    device.last_seen_at = datetime.now(timezone.utc)
    if body.firmware_version:
        device.firmware_version = body.firmware_version

    await db.commit()
    return TelemetryResponse(accepted=1)
