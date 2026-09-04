"""Endpoints de autenticación: register, login, refresh."""

from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, Depends, HTTPException, status
from jose import JWTError, jwt
from pydantic import BaseModel, EmailStr
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.database import get_db
from app.models import User
from app.security import hash_secret, verify_secret
from app.settings import settings

router = APIRouter(prefix="/auth", tags=["auth"])


# --- Schemas ---


class AuthRequest(BaseModel):
    email: EmailStr
    password: str


class RefreshRequest(BaseModel):
    refresh_token: str


class TokenResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"
    expires_in: int


# --- Helpers ---


def _create_token(data: dict, expires_delta: timedelta) -> str:
    to_encode = data.copy()
    to_encode["exp"] = datetime.now(timezone.utc) + expires_delta
    return jwt.encode(to_encode, settings.JWT_SECRET_KEY, algorithm=settings.JWT_ALGORITHM)


def _create_tokens(user_id: int, is_admin: bool) -> TokenResponse:
    access_delta = timedelta(minutes=settings.ACCESS_TOKEN_EXPIRE_MINUTES)
    refresh_delta = timedelta(days=settings.REFRESH_TOKEN_EXPIRE_DAYS)

    payload = {"sub": str(user_id), "admin": is_admin}

    access_token = _create_token({**payload, "type": "access"}, access_delta)
    refresh_token = _create_token({**payload, "type": "refresh"}, refresh_delta)

    return TokenResponse(
        access_token=access_token,
        refresh_token=refresh_token,
        expires_in=int(access_delta.total_seconds()),
    )


# --- Endpoints ---


@router.post("/register", response_model=TokenResponse, status_code=status.HTTP_201_CREATED)
async def register(body: AuthRequest, db: AsyncSession = Depends(get_db)):
    """Crea un usuario nuevo y devuelve tokens."""
    result = await db.execute(select(User).where(User.email == body.email))
    if result.scalar_one_or_none():
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail="Email ya registrado")

    user = User(
        email=body.email,
        password_hash=hash_secret(body.password),
    )
    db.add(user)
    await db.commit()
    await db.refresh(user)

    return _create_tokens(user.id, user.is_admin)


@router.post("/login", response_model=TokenResponse)
async def login(body: AuthRequest, db: AsyncSession = Depends(get_db)):
    """Autentica con email+password y devuelve tokens."""
    result = await db.execute(select(User).where(User.email == body.email))
    user = result.scalar_one_or_none()

    if not user or not verify_secret(body.password, user.password_hash):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Credenciales inválidas")

    return _create_tokens(user.id, user.is_admin)


@router.post("/refresh", response_model=TokenResponse)
async def refresh(body: RefreshRequest, db: AsyncSession = Depends(get_db)):
    """Emite un access_token nuevo a partir de un refresh_token válido."""
    try:
        payload = jwt.decode(body.refresh_token, settings.JWT_SECRET_KEY, algorithms=[settings.JWT_ALGORITHM])
    except JWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Refresh token inválido")

    if payload.get("type") != "refresh":
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Token no es de tipo refresh")

    user_id = int(payload["sub"])
    result = await db.execute(select(User).where(User.id == user_id))
    user = result.scalar_one_or_none()

    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Usuario no existe")

    return _create_tokens(user.id, user.is_admin)
