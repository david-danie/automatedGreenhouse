from fastapi import APIRouter, HTTPException
from models import *

router = APIRouter(prefix="/user", tags=["user"])

@router.get("/{deviceId}", response_model=DeviceData, status_code=200, summary="Obtener dispositivo por ID")  # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def getDevice(deviceId: str):    
    deviceExists = searchDevice(deviceId)
    if deviceExists is not None:
        return deviceExists
    raise  HTTPException(status_code = 404, detail = "No se ha encontrado el dispositivo")
    
@router.get("/", response_model=DeviceData, status_code=200, summary="Obtener dispositivo por Query")       # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def getDeviceByQuery(deviceId: str):    
    deviceExists = searchDevice(deviceId)
    if deviceExists is not None:
        return deviceExists
    raise  HTTPException(status_code = 404, detail = "No se ha encontrado el dispositivo")

@router.get("/", response_model=list[DeviceData], status_code=200, summary="Obtener todos los dispositivos")      # Aqui se lanzará los usuarios existentes
async def getAllDevices():   
    if len(usersDataList) >= 1:  
        return usersDataList
    raise  HTTPException(status_code = 400, detail = "No hay usuarios existentes")
    
@router.post("/", response_model=DeviceData, status_code = 201, summary="Crear un nuevo dispositivo")
async def createDevice(newDevice: DeviceData):
    print(newDevice.model_dump())
    #deviceExists = searchDevice(newDevice.deviceId)
    if deviceExists := searchDevice(newDevice.deviceId) is not None:
        raise  HTTPException(status_code = 409, detail = "El usuario ya existe")
    usersDataList.append(newDevice)
    return newDevice
    
@router.put("/", response_model=DeviceData, status_code = 200, summary="Actualizar un dispositivo existente")
async def updateDevice(updatedDevice: DeviceData):
    for index, currentDevices in enumerate(usersDataList):
        if currentDevices.deviceId == updatedDevice.deviceId:
            usersDataList[index] = updatedDevice
            return updatedDevice
    raise  HTTPException(status_code = 400, detail = "No se ha encontrado el dispositivo")
    
@router.delete("/{deviceId}", response_model=DeviceData, status_code = 201, summary="Eliminar dispositivo por ID")   # Aqui se lanzará los usuarios existentes que coincidan con el Id
async def deleteDevice(deviceId: str): 
    for index, currentDevices in enumerate(usersDataList):
        if currentDevices.deviceId == deviceId:
            deleted_device = usersDataList.pop(index)
            return deleted_device                                               # Devolver el objeto eliminado
    raise  HTTPException(status_code = 404, detail = "No se ha encontrado el dispositivo")