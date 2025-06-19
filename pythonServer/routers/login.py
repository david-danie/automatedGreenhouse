import jwt
from fastapi.responses import FileResponse
from datetime import datetime, timedelta, timezone
from fastapi import APIRouter, HTTPException, Depends, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from jwt.exceptions import InvalidTokenError
from passlib.context import CryptContext
from models import *

ALGORITHM = "HS256"
ACCESS_TOKEN_DURATION = 5
SECRET_KEY = "09d25e094faa6ca2556c818166b7a9563b93f7099f6f0f4caa6cf63b88e8d3e7"

router = APIRouter(tags=["Login for devices"])
oauth2 = OAuth2PasswordBearer(tokenUrl="login")
crypt = CryptContext(schemes=["bcrypt"])

def searchUser(username: str):
    if userData := exampleDB.get(username):
        return UserDB(userData)
    return None

async def authDevice(token: str = Depends(oauth2)):
    try:
        username = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM]).get("sub")
        if (deviceData := exampleDB.get(username)) is None:
            raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, 
                                detail="Credenciales inválidas", 
                                headers={"WWW-Authenticate": "Bearer"})
        return DeviceData(**deviceData)

    except InvalidTokenError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, 
                            detail="Usuaario no autenticado", 
                            headers={"WWW-Authenticate": "Bearer"})

@router.post("/login", summary="Autenticar usuario y contraseña")      # Aqui se lanzará los usuarios existentes
async def autenticateUser(form: OAuth2PasswordRequestForm = Depends()):  
    userDB = exampleDB.get(form.username)
    if not userDB:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="El usuario no existe")
    user = UserDB(**userDB)

    if not crypt.verify(form.password, user.password):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="La contraseña no es correcta")
    if not user.status:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, 
                        detail="Usuario inactivo", 
                        headers={"WWW-Authenticate": "Bearer"})

    expiration = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_DURATION)
    access_token = {"sub": user.userName, "exp": expiration}
    token = jwt.encode(access_token, SECRET_KEY, algorithm=ALGORITHM)
    return {"accessToken": token, "tokenType": "bearer"}

@router.get("/firmware/{deviceId}", response_class=FileResponse, status_code=200, summary="Descarga binario de la actualización OTA")
async def getFirmware(deviceId: str, authDevice: DeviceData = Depends(authDevice)):
    print(f"Device: {deviceId}")
    if not deviceId == authDevice.deviceId:
        raise  HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="No autorizado")
    try:    
        return FileResponse("firmware/newVer.bin", filename="newVer.bin", media_type="application/octet-stream")
    except Exception:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Firmware no encontrado")