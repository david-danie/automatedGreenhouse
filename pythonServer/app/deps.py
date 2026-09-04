"""Dependencias de FastAPI para autenticación.

Resuelve el `Authorization: Bearer <token>` de un dispositivo contra la fila de
`devices`. El token en claro nunca se guarda: en la BD solo vive `token_hash`
(bcrypt), así que la verificación es `verify_secret(token, device.token_hash)`
sobre el dispositivo identificado por la ruta `{device_id}`.

No se puede buscar por `WHERE token_hash = hash(token)` porque bcrypt usa un salt
distinto en cada hash; por eso se trae el dispositivo por su id y se verifica.
"""

from fastapi import Depends, HTTPException, Path, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from jose import JWTError, jwt
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models import Device, User
from app.security import verify_secret
from app.settings import settings

# auto_error=False para devolver 401 con nuestro mensaje en vez del genérico 403
# de FastAPI cuando falta la cabecera Authorization.
_bearer = HTTPBearer(auto_error=False)


async def get_current_device(
    device_id: int = Path(..., description="devices.id (entero), no la MAC"),
    creds: HTTPAuthorizationCredentials | None = Depends(_bearer),
    db: AsyncSession = Depends(get_db),
) -> Device:
    """Autentica al dispositivo de la ruta con su token Bearer.

    Devuelve la fila `Device` si el token es válido; si no, `401`. Se usa como
    dependencia en las rutas device-facing bajo `/devices/{device_id}/...`.
    """
    if creds is None or not creds.credentials:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Falta el token de dispositivo",
            headers={"WWW-Authenticate": "Bearer"},
        )

    result = await db.execute(select(Device).where(Device.id == device_id))
    device = result.scalar_one_or_none()

    # Mismo 401 tanto si el dispositivo no existe como si el token no verifica:
    # no se revela la existencia de un id a quien no trae el token correcto.
    if device is None or not device.token_hash or not verify_secret(creds.credentials, device.token_hash):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token de dispositivo inválido",
            headers={"WWW-Authenticate": "Bearer"},
        )

    return device


async def get_current_user(
    creds: HTTPAuthorizationCredentials | None = Depends(_bearer),
    db: AsyncSession = Depends(get_db),
) -> User:
    """Autentica al usuario de la app con su access JWT (`Authorization: Bearer`).

    Se usa en las rutas app-facing (§4.2). Valida la firma y que sea un token de
    tipo `access` (no un `refresh`), resuelve `sub` → `users.id` y devuelve la
    fila `User`. Cualquier fallo responde `401` uniforme, sin distinguir causa.
    """
    if creds is None or not creds.credentials:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Falta el token de acceso",
            headers={"WWW-Authenticate": "Bearer"},
        )

    try:
        payload = jwt.decode(
            creds.credentials,
            settings.JWT_SECRET_KEY,
            algorithms=[settings.JWT_ALGORITHM],
        )
    except JWTError:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token de acceso inválido",
            headers={"WWW-Authenticate": "Bearer"},
        )

    # Un refresh token no debe servir para autenticar peticiones normales.
    if payload.get("type") != "access":
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token no es de tipo access",
            headers={"WWW-Authenticate": "Bearer"},
        )

    sub = payload.get("sub")
    if sub is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token sin sujeto",
            headers={"WWW-Authenticate": "Bearer"},
        )

    result = await db.execute(select(User).where(User.id == int(sub)))
    user = result.scalar_one_or_none()
    if user is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Usuario no existe",
            headers={"WWW-Authenticate": "Bearer"},
        )

    return user
