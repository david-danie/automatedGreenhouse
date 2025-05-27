from pydantic import BaseModel

class DeviceData(BaseModel):
    userName: str
    deviceId: str
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

class UserDB(DeviceData):
    password: str

class OTARequest(BaseModel):
    user: str
    deviceId: str
    deviceVersion: str

def searchDevice(deviceId: str):
    user = filter(lambda d: d.deviceId == deviceId, usersDataList)
    try:
        return list(user)[0]
    except:
        return None

usersDataList = [DeviceData(userName = "David", deviceId = "124EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 2, day = 3),
                 DeviceData(userName = "Daniel", deviceId = "134EW698AA", deviceVer = "1.0.0", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 2),
                 DeviceData(userName = "Estefanny", deviceId = "144EW698AA", deviceVer = "1.0.0", status = False, photoperiod = 6, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 8, week = 0, day = 1),
                 DeviceData(userName = "Tania", deviceId = "154EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 9, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 6, day = 4),
                 DeviceData(userName = "Juan", deviceId = "164EW698AA", deviceVer = "1.0.0", status = True, photoperiod = 18, blueLed = 75, redLed = 80, whiteLed = 100, irriTimes = 4, irriMinute = 5, ventTimes = 7, ventMinute = 5, week = 0, day = 6)]



exampleDB = {
    "Davzig": {
        "userName": "Davzig",
        "password": "claveDavid",
        "deviceId": "124EW698AA",
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
        "userName": "Daniel",
        "password": "claveDaniel",
        "deviceId": "134EW698AA",
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