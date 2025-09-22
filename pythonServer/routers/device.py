from fastapi import APIRouter, HTTPException, status, Depends, Header
from fastapi.responses import FileResponse
from fastapi.security import OAuth2PasswordRequestForm
from auth.auth import crypt, createToken, getAuthDevice
from utils.utils import available_OTA_update, validate_form
from models import *
from database import *

router = APIRouter(prefix="/device", tags=["Devices"])

@router.post("/data", summary="Actualiza información del Dispositivo")              # Aquí se recibirán los datos de los dispositivos
async def uploadData(data: DeviceMeasurements):
    print(data.model_dump())
    return {"status": "success"}


@router.post("/login", summary="Autenticar usuario, dispositivo y contraseña")       #   
async def autenticateDevice(form: OAuth2PasswordRequestForm = Depends()):

    print(f"User:{form.username} [{form.password}] Device:{form.client_id} [{form.client_secret}]")  
    user_data_auth = validate_form(form)    #   Si alguna validación no se cumple, lanza un código 400
    user = UserDB(**user_data_auth)

    if not crypt.verify(form.password, user.password):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="La contraseña no es correcta")
    if not user.status:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, 
                        detail="Usuario inactivo", 
                        headers={"WWW-Authenticate": "Bearer"})

    token = createToken({"sub": user.username, "deviceId": user.device_id})
    return {"accessToken": token, "tokenType": "bearer"}


@router.get("/firmware", response_class=FileResponse, status_code=200, summary="Descarga binario de la actualización OTA")
async def getFirmware(version: str = Header(None),  authDevice: DeviceData = Depends(getAuthDevice)):

    confirmation = available_OTA_update(version)
    
    if confirmation is None:
        raise HTTPException(status_code=status.HTTP_204_NO_CONTENT, detail="No hay actualización disponible")
    try: 
        print(f"Path:{confirmation["filepath"]}  filename:{confirmation["filename"]}  headers:{confirmation["headers"]}")
        return FileResponse(confirmation["filepath"], filename=confirmation["filename"], 
                            media_type="application/octet-stream", headers={"version": confirmation["headers"]})
    except Exception:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Firmware no encontrado")


    
    