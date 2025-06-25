from fastapi import APIRouter, HTTPException, Response, status, Depends, Header
from fastapi.responses import FileResponse
from fastapi.security import OAuth2PasswordRequestForm
from auth.login import crypt, createToken, getAuthDevice
from models import *
from database import *

currentFirmwareVersion = "1.0.0"  

router = APIRouter(prefix="/device", tags=["Devices"])

@router.post("/data", summary="Actualiza información del Dispositivo")              # Aquí se recibirán los datos de los dispositivos
async def uploadData(data: DeviceData):
    print(data.model_dump())
    return {"status": "success"}


@router.post("/login", summary="Autenticar usuario y contraseña")       #   Aqui se lanzará los usuarios existentes
async def autenticateUser(form: OAuth2PasswordRequestForm = Depends()):
    print(f"User:{form.username} Device:{form.client_id}")  
    userDataDB = exampleDB.get(form.username)
    if not userDataDB or userDataDB.get("deviceId") != form.client_id:  #   Aquí se debe validar que tanto username como deviceID, estén en BD 
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="El usuario o disposivo no existen")
    user = UserDB(**userDataDB)

    if not crypt.verify(form.password, user.password):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, 
                            detail="La contraseña no es correcta")
    if not user.status:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, 
                        detail="Usuario inactivo", 
                        headers={"WWW-Authenticate": "Bearer"})

    token = createToken({"sub": user.userName, "deviceId": user.deviceId})
    return {"accessToken": token, "tokenType": "bearer"}


@router.get("/firmware", response_class=FileResponse, status_code=200, summary="Descarga binario de la actualización OTA")
async def getFirmware(version: str = Header(None),  authDevice: DeviceData = Depends(getAuthDevice)):
    print(f" *** Device:{authDevice.deviceId} Version:{version} *** ")
    if version == currentFirmwareVersion:
        raise HTTPException(status_code=status.HTTP_204_NO_CONTENT, detail="No hay actualización disponible")
    try: 
        header = {"version": currentFirmwareVersion}
        return FileResponse("firmware/newVer.bin", filename="newVer.bin", media_type="application/octet-stream", headers=header)
    except Exception:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Firmware no encontrado")


    
    