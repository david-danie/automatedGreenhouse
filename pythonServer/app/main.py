from fastapi import FastAPI

from app.routers import auth

app = FastAPI(
    title="SmartPlant API",
    version="0.1.0",
    description="Backend para control de cultivo IoT",
)

app.include_router(auth.router)


@app.get("/health")
async def health():
    return {"status": "ok"}
