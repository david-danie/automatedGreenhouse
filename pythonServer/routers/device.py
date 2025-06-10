from fastapi import APIRouter, HTTPException
from fastapi.responses import FileResponse
#from routers import login
from models import *
from database import *

router = APIRouter(prefix="/device", tags=["device"])

@router.post("/", summary="Actualiza información del Dispositivo")              # Aquí se recibirán los datos de los dispositivos
async def uploadData(data: DeviceData):
    print(data.model_dump())
    return {"status": "success"}

@router.get("/", status_code = 200, summary="Solicita actualización OTA")
async def otaUpdate(data: OTARequest):     # Aqui se responderá si hay nuevas actualizaciones OTA
    if searchDevice(data.deviceId) is None:
        print(searchDevice(data.deviceId))
        raise  HTTPException(status_code = 400, detail = "El dispositivo no existe")
    print(data.model_dump())
    latest_version = "v1.2.0"
    firmware_url = "https://127.0.0.1:8000/firmwares/esp32_firmware_v1.2.0.bin"

    return {
        "update_available": True,
        "version": latest_version,
        "url": firmware_url
    }

'''
@router.get("/firmware/{deviceId}", response_class=FileResponse, status_code=200, summary="Descarga binario de la actualización OTA")
async def getFirmware(deviceId: str):
    print(f"Device: {deviceId}")
    if (device := searchDevice(deviceId)) is None:
        raise  HTTPException(status_code=403, detail="No autorizado")
    return FileResponse("firmware/newVer.bin", filename="newVer.bin", media_type="application/octet-stream")
'''

    
