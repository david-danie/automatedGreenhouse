"""Helpers de seguridad compartidos: hashing de contraseñas y tokens de dispositivo.

Una sola fuente de verdad para bcrypt, usada tanto por la auth de usuarios
(`routers/auth.py`) como por la provisión de dispositivos (`routers/devices.py`).
"""

import secrets

from passlib.context import CryptContext

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


def hash_secret(plain: str) -> str:
    """Hashea una contraseña o token con bcrypt."""
    return pwd_context.hash(plain)


def verify_secret(plain: str, hashed: str) -> bool:
    """Verifica un valor en claro contra su hash bcrypt."""
    return pwd_context.verify(plain, hashed)


def generate_device_token() -> str:
    """Genera un token de dispositivo de 64 caracteres hexadecimales (32 bytes)."""
    return secrets.token_hex(32)
