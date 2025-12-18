import os
import re
from fastapi import HTTPException, Form,status
from models import *

FIRMWARE_DIR = "firmware"
pattern = re.compile(r"^[A-Za-z0-9]{3}ver(\d+\.\d+\.\d+).bin$")


current_firmware_info = {
    "version": None,
    "filename": None,
    "filepath": None
}

def set_firmware_info(version: str, filename: str, path: str):
    current_firmware_info["version"] = version
    current_firmware_info["filename"] = filename
    current_firmware_info["filepath"] = path

def get_firmware_info():
    return current_firmware_info

def load_firmware_on_startup():
    for filename in os.listdir(FIRMWARE_DIR):
        match = pattern.match(filename)
        if match:
            version = match.group(1)
            filepath = os.path.join(FIRMWARE_DIR, filename).replace("\\", "/")
            set_firmware_info(version, filename, filepath)
            print(f"   ✅ . Firmware cargado: {filename}  v{version}")
            return
    print("⚠️ No se encontró firmware válido al iniciar.")

def available_OTA_update(deviceVersion: str) -> dict | None: 
    """
    Busca un archivo con formato XXXverX.Y.Z en /firmware y compara contra la versión del dispositivo.
    Devuelve True si la versión del archivo es mayor que la del dispositivo.
    """
    #pattern = re.compile(r"^[A-Za-z0-9]{3}ver(\d+\.\d+\.\d+).bin$")
    
    firmware = get_firmware_info()

    print(f"filename: {firmware["filename"]}  v{firmware["version"]} path: {firmware["filepath"]}")

    if tuple(map(int, firmware["version"].split("."))) > tuple(map(int, deviceVersion.split("."))):
        #return True
        return {"filename": firmware["filename"],
                "filepath": firmware["filepath"],
                "headers": firmware["version"]}

    # No se encontró archivo válido → no hay actualización
    return None

def validate_form(credentials: Form) -> dict:
    """
    Se validan los 4 campos del formulario. Validación alfanumérica, de tamaño, caracteres repetitivos,
    carácteres especiales.
    """
    validate_username(credentials.username)
    validate_password(credentials.password)
    validate_client_id(credentials.client_id)
    validate_password(credentials.client_secret)

    user_data = exampleDB_users.get(credentials.username)      
    #linked_device = 5                                                      
    #  Solo debe regresar info si el USUARIO EXISTE y tiene el DEVICEID VINCULADO 
    if user_data and user_data.get("device_id") == credentials.client_id:      
        return user_data
    raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="1 Dispositivo no existe")

def validate_username(value: str):
    if value is None:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="2 Informacion incorrecta")
    if len(value) > 32:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="3 Informacion incorrecta")
    if not re.fullmatch(r"[A-Za-z0-9]+", value):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="4 Informacion incorrecta")
    if re.search(r"(.)\1{3,}", value):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="5 Informacion incorrecta")

def validate_password(value: str):
    if value is None:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="6 Informacion incorrecta")
    if len(value) > 64:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="7 Informacion incorrecta")
    if not re.fullmatch(r"[A-Za-z0-9@#%&*!$._-]+", value):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="8 Informacion incorrecta")
    if re.search(r"(.)\1{3,}", value):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="9 Informacion incorrecta")

def validate_client_id(value: str):
    if value is None:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="10 Iinformacion incorrecta")
    if len(value) != 12:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="11 Informacion incorrecta")
    if not re.fullmatch(r"[A-Za-z0-9]+", value):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="12 Informacion incorrecta")
    
def validate_version(value: str):
    if value is None:
        raise HTTPException(status_code=status.HTTP_204_NO_CONTENT, detail="Versión del dispositivo no proporcionada")
    if len(value) != 8:
        raise HTTPException(status_code=status.HTTP_204_NO_CONTENT, detail="14 Informacion incorrecta")
    if not re.fullmatch(r"[0-9.]+", value):
        raise HTTPException(status_code=status.HTTP_204_NO_CONTENT, detail="15 Informacion incorrecta")
