import jwt
#import time
from datetime import datetime, timedelta, timezone
from fastapi import APIRouter, HTTPException, Depends, status
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from jwt.exceptions import InvalidTokenError
from passlib.context import CryptContext
from models import *

ALGORITHM = "HS256"
ACCESS_TOKEN_DURATION = 1
SECRET_KEY = "09d25e094faa6ca2556c818166b7a9563b93f7099f6f0f4caa6cf63b88e8d3e7"

router = APIRouter(tags=["Login for devices"])
oauth2_shceme = OAuth2PasswordBearer(tokenUrl="login")
crypt = CryptContext(schemes=["bcrypt"])

def searchUser(username: str):
    if userData := exampleDB.get(username):
        return UserDB(userData)
    return None

async def authDevice(token: str = Depends(oauth2_shceme)):
    try:
        username = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM]).get("sub")
        if (deviceData := exampleDB.get(username)) is None:
            raise HTTPException(status_code = status.HTTP_401_UNAUTHORIZED, 
                                detail = "Credenciales inválidas", 
                                headers={"WWW-Authenticate": "Bearer"})
        return DeviceData(**deviceData)

    except InvalidTokenError:
        raise HTTPException(status_code = status.HTTP_401_UNAUTHORIZED, 
                            detail = "Credenciales inválidas", 
                            headers={"WWW-Authenticate": "Bearer"})

async def currentDevice(user: DeviceData = Depends(authDevice)):
    if not user.status:
        raise HTTPException(status_code = status.HTTP_400_BAD_REQUEST, 
                            detail = "Usuario inactivo", 
                            headers={"WWW-Authenticate": "Bearer"})
    return user

@router.post("/login", summary="Autenticar usuario y contraseña")      # Aqui se lanzará los usuarios existentes
async def autenticateUser(form: OAuth2PasswordRequestForm = Depends()):   
    if not (userDB := exampleDB.get(form.username)):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail = "El usuario no existe")
    #user = searchUser(form.username)
    user = UserDB(**userDB)

    #crypt.verify(form.password, user.password)

    if not crypt.verify(form.password, user.password):
        raise HTTPException(status_code = status.HTTP_400_BAD_REQUEST, 
                            detail = "La contraseña no es correcta")
    
    #accessTokenExpiration = timedelta(minutes=ACCESS_TOKEN_DURATION)

    expiration = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_DURATION)
    access_token = {"sub": user.userName, "exp": expiration}

    return {"accessToken": jwt.encode(access_token, SECRET_KEY, algorithm=ALGORITHM), "tokenType": "bearer"}



@router.get("/users/me", summary="Ruta para actualizaciones OTA")    
async def postAuth(device: DeviceData = Depends(currentDevice)):   
    return device
