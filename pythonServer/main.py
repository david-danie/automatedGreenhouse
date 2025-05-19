from fastapi import FastAPI, UploadFile, File, HTTPException
from pydantic import BaseModel

app = FastAPI()

class DeviceData(BaseModel):
    userName: str
    deviceId: str
    status: bool
    photoperiod: int
    blueLed: int
    redLed: int
    whiteLed: int
    irriTimes: int
    irriMinute: int
    ventTimes: int
    ventMinute: int
    week: int
    day: int

class OTARequest(BaseModel):
    user: str
    deviceId: str
    currentVersion: str

usersDataList = [DeviceData(userName = "David", deviceId = "124EW698AA", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 2, day = 3),
                 DeviceData(userName = "Daniel", deviceId = "134EW698AA", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 2),
                 DeviceData(userName = "Estefanny", deviceId = "144EW698AA", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 8, week = 0, day = 1),
                 DeviceData(userName = "Tania", deviceId = "154EW698AA", status = True, photoperiod = 9, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 6, day = 4),
                 DeviceData(userName = "Juan", deviceId = "164EW698AA", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 6)]

@app.post("/data")              # Aquí se recibirán los datos de los dispositivos
async def upload_data(data: DeviceData):
    print(data.dict())
    return {"status": "success"}

@app.get("/users")              # Aqui se lanzará los usuarios existentes
async def users():     
    return usersDataList

@app.get("/user/{deviceId}")    # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def user(deviceId: str):    
    return searchUser(deviceId)
    
@app.get("/userquery/")    # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def user(deviceId: str):    
    return searchUser(deviceId)
    
@app.post("/device/")
async def user(newDevice: DeviceData):
    print(newDevice.dict())
    if type(searchUser(newDevice.deviceId)) == DeviceData:
        return {"error": "El usuario ya existe"}
    else:
        usersDataList.append(newDevice)
        return newDevice
    
@app.put("/device/")
async def user(updatedDevice: DeviceData):
    userFound = False
    for index, currentDevices in enumerate(usersDataList):
        if currentDevices.deviceId == updatedDevice.deviceId:
            usersDataList[index] = updatedDevice
            userFound = True
            return updatedDevice
    if not userFound:
        return {"error": "No se ha encontrado al usuario"}
    
@app.delete("/user/{deviceId}")    # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def user(deviceId: str):    
    for index, currentDevices in enumerate(usersDataList):
        if currentDevices.deviceId == deviceId:
            del usersDataList[index]
            return deviceId

# uvicorn main:app --reload

@app.get("/ota")
async def ota_latest(data: OTARequest):     # Aqui se responderá si hay nuevas actualizaciones OTA
    print(data.dict())
    latest_version = "v1.2.0"
    firmware_url = "https://tuservidor.com/firmwares/esp32_firmware_v1.2.0.bin"

    return {
        "update_available": True,
        "version": latest_version,
        "url": firmware_url
    }

def searchUser(deviceId: str):
    user = filter(lambda d: d.deviceId == deviceId, usersDataList)
    try:
        return list(user)[0]
    except:
        return {"Error" : "Ussuario no encontrado"}
