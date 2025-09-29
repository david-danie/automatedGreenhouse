from fastapi import HTTPException, Depends, status
from datetime import datetime, timedelta, timezone
from fastapi.security import OAuth2PasswordBearer
import jwt
from jwt.exceptions import InvalidTokenError
from passlib.context import CryptContext
from models import DeviceData, exampleDB
from sensible import *

ALGORITHM = "HS256"
ACCESS_TOKEN_DURATION = 3
SECRET_KEY = "09d25e094faa6ca2556c18166b7a9593b93f7099f6f0f6caa6cf63b88e8d3e7"

oauth2 = OAuth2PasswordBearer(tokenUrl="/device/login")
crypt = CryptContext(schemes=["bcrypt"])

def createToken(data: dict) -> str:
    payload = data.copy()
    payload["exp"] = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_DURATION)
    return jwt.encode(payload, XDKEY, algorithm=ALGORITHM)

def decodeToken(token: str) -> dict:
    return jwt.decode(token, XDKEY, algorithms=[ALGORITHM])

async def getAuthDevice(token: str = Depends(oauth2)):
    try:
        payload = decodeToken(token)
        username = payload.get("sub")
        deviceId = payload.get("deviceId")
        if (deviceData := exampleDB.get(username)) is None:             #   Aqui solo se está validando el usuario, no se valida deviceId
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, 
                                detail="Credenciales inválidas", 
                                headers={"WWW-Authenticate": "Bearer"})
        return DeviceData(**deviceData)

    except InvalidTokenError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, 
                            detail="Usuaario no autenticado", 
                            headers={"WWW-Authenticate": "Bearer"})



    