from fastapi import FastAPI, UploadFile, File
from pydantic import BaseModel

app = FastAPI()

class DeviceData(BaseModel):
    user: str
    device_id: str
    status: bool
    photoperiod: int
    blueLed: int
    redLed: int
    whiteLed: int
    irrigationTimes: int
    irrigationMinute: int
    ventilationTimes: int
    ventilationMinute: int
    week: int
    day: int

class OTARequest(BaseModel):
    user: str
    deviceId: str
    currentVersion: str

@app.post("/data")
async def upload_data(data: DeviceData):
    # Aquí guardas los datos
    print(data.dict())
    return {"status": "success"}

@app.get("/ota")
async def ota_latest(data: OTARequest):
    # Lógica para saber si ese device necesita actualizar
    # Ejemplo simple:
    print(data.dict())
    latest_version = "v1.2.0"
    firmware_url = "https://tuservidor.com/firmwares/esp32_firmware_v1.2.0.bin"

    return {
        "update_available": True,
        "version": latest_version,
        "url": firmware_url
    }
