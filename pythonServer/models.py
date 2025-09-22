from pydantic import BaseModel

class DeviceData(BaseModel):
    username: str
    device_id: str
    deviceVer: str
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

class DeviceMeasurements(BaseModel):
    #userName: str
    #deviceId: str
    deviceVer: str
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

class LoginData(BaseModel):
    username: str
    #password: str
    client_id: str
    client_secret: str 

class UserDB(DeviceData):
    password: str

def searchDevice(deviceId: str):
    user = filter(lambda d: d.deviceId == deviceId, usersDataList)
    try:
        return list(user)[0]
    except:
        return None

usersDataList = [DeviceData(username = "David", device_id = "124EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 2, day = 3),
                 DeviceData(username = "Daniel", device_id = "134EW698AA", deviceVer = "1.0.0", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 2),
                 DeviceData(username = "Estefanny", device_id = "144EW698AA", deviceVer = "1.0.0", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 8, week = 0, day = 1),
                 DeviceData(username = "Tania", device_id = "154EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 9, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 6, day = 4),
                 DeviceData(username = "Juan", device_id = "164EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 6)]

exampleDB_users = {
    "Davzig": {
        "user_id": 1,
        "username": "Davzig",
        "password": "$2a$12$YNLqLotD8tDrao4qi7C8XOuRkS2OkQ8vu084GVMjYkrY/YjkzvUwa", #claveDavzig
        "device_id": "10061C81EE7C"
    },
    "Daniel": {
        "user_id": 2,
        "username": "Daniel",
        "password": "$2a$12$zWB0i46fzosrzDhncpDDzu1cbBtJ82PStQI3JPsuj.59GxUmMAwK6", #claveDaniel
        "device_id": "134EW698AA"
    }
}

exampleDB_devices = {
    "Davzig": {
        "username": "Davzig",
        "password": "$2a$12$YNLqLotD8tDrao4qi7C8XOuRkS2OkQ8vu084GVMjYkrY/YjkzvUwa", #claveDavzig
        "device_id": "10061C81EE7C",
        "deviceVer": "1.0.0",
        "status": True,
        "photoperiod": 18,
        "blueLed": 75,
        "redLed": 80,
        "whiteLed": 100,
        "irriTimes": 4,
        "irriMinute": 5,
        "ventTimes": 7,
        "ventMinute": 5,
        "week": 2,
        "day": 3
    },
    "Daniel": {
        "username": "Daniel",
        "password": "$2a$12$zWB0i46fzosrzDhncpDDzu1cbBtJ82PStQI3JPsuj.59GxUmMAwK6", #claveDaniel
        "device_id": "134EW698AA",
        "deviceVer": "1.0.0",
        "status": False,
        "photoperiod": 6,
        "blueLed": 75,
        "redLed": 80,
        "whiteLed": 100,
        "irriTimes": 4,
        "irriMinute": 5,
        "ventTimes": 7,
        "ventMinute": 5,
        "week": 0,
        "day": 2
    }
}
exampleDB_measurements = {
    "Davzig": {
        "username": "Davzig",
        "password": "$2a$12$YNLqLotD8tDrao4qi7C8XOuRkS2OkQ8vu084GVMjYkrY/YjkzvUwa", #claveDavzig
        "device_id": "10061C81EE7C",
        "deviceVer": "1.0.0",
        "status": True,
        "photoperiod": 18,
        "blueLed": 75,
        "redLed": 80,
        "whiteLed": 100,
        "irriTimes": 4,
        "irriMinute": 5,
        "ventTimes": 7,
        "ventMinute": 5,
        "week": 2,
        "day": 3
    },
    "Daniel": {
        "username": "Daniel",
        "password": "$2a$12$zWB0i46fzosrzDhncpDDzu1cbBtJ82PStQI3JPsuj.59GxUmMAwK6", #claveDaniel
        "device_id": "134EW698AA",
        "deviceVer": "1.0.0",
        "status": False,
        "photoperiod": 6,
        "blueLed": 75,
        "redLed": 80,
        "whiteLed": 100,
        "irriTimes": 4,
        "irriMinute": 5,
        "ventTimes": 7,
        "ventMinute": 5,
        "week": 0,
        "day": 2
    }
}