from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    # --- Database ---
    DATABASE_URL: str = "postgresql+asyncpg://smartplant:smartplant_secret@db:5432/smartplant_db"

    # --- JWT ---
    JWT_SECRET_KEY: str = "change-me-in-production"
    JWT_ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 30
    REFRESH_TOKEN_EXPIRE_DAYS: int = 7

    # --- MQTT ---
    MQTT_HOST: str = "mosquitto"
    MQTT_PORT: int = 1883

    # --- S3 / MinIO ---
    S3_ENDPOINT_URL: str = "http://minio:9000"
    S3_ACCESS_KEY: str = "admin"
    S3_SECRET_KEY: str = "admin123456"
    S3_BUCKET_FIRMWARE: str = "firmware"

    class Config:
        env_file = ".env"
        env_file_encoding = "utf-8"
        # El .env es compartido con los otros servicios del compose (Postgres,
        # MinIO): contiene llaves como POSTGRES_USER o MINIO_ROOT_PASSWORD que
        # este modelo no declara. Sin esto, pydantic v2 (extra="forbid" por
        # defecto) aborta el arranque con "Extra inputs are not permitted".
        extra = "ignore"


settings = Settings()
