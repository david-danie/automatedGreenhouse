from fastapi import FastAPI

from routers import device, user, login
from models import *

app = FastAPI()

app.include_router(device.router)
app.include_router(user.router)
app.include_router(login.router)


@app.get("/", response_model=str, status_code=200)      # Aqui se lanzará los usuarios existentes
async def hello():     
    return "wellcome to the crop server"

# uvicorn main:app --reload


