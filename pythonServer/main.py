from fastapi import FastAPI

#from auth import login
from routers import device, user
from models import *

app = FastAPI()

app.include_router(device.router)
app.include_router(user.router)
#app.include_router(login.router)

@app.get("/", response_model=str, status_code=200)      # Default
async def hello():     
    return "wellcome to the crop server"

# uvicorn main:app --reload
# uvicorn main:app --host 0.0.0.0 --port 8000 --reload


