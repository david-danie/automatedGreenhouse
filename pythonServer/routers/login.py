from fastapi import APIRouter, HTTPException, Depends
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from models import *

router = APIRouter(tags=["Login for devices"])

oauth2 = OAuth2PasswordBearer(tokenUrl="login")

def searchUser(username: str):
    if userData := exampleDB.get(username):
        return UserDB(userData)
    return None

async def currentUser(token: str = Depends(oauth2)):
    pass

@router.post("/login", summary="Autenticar usuario y contraseña")      # Aqui se lanzará los usuarios existentes
async def autenticateUser(form: OAuth2PasswordRequestForm = Depends()):   
    if not (userDB := exampleDB.get(form.username)):
        raise HTTPException(status_code = 400, detail = "El usuario no existe")
    #user = searchUser(form.username)
    user = UserDB(**userDB)
    if not form.password == user.password:
        raise HTTPException(status_code = 400, detail = "La contraseña no es correcta")
    
    return {"accessToken": "token_{user.name}", "tokenType": "bearer"}



@router.get("/users/me", summary="auth")    
async def postAuth():   
    pass