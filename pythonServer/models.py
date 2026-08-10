from sqlalchemy import Column, Integer, String, Boolean, Float, DateTime, ForeignKey, text
from sqlalchemy.orm import relationship
from database import Base
import datetime

# --- SQLAlchemy Models ---

class User(Base):
    __tablename__ = "users"
    id = Column(Integer, primary_key=True, index=True)
    email = Column(String(255), unique=True, nullable=False, index=True)
    password_hash = Column(String(255), nullable=False)
    is_admin = Column(Boolean, default=False)
    created_at = Column(DateTime(timezone=True), server_default=text("CURRENT_TIMESTAMP"))

    devices = relationship("Device", back_populates="owner", cascade="all, delete-orphan")

class Device(Base):
    __tablename__ = "devices"
    id = Column(Integer, primary_key=True, index=True)
    mac = Column(String(17), unique=True, nullable=False, index=True)
    user_id = Column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    name = Column(String(255))
    token_hash = Column(String(255))
    firmware_version = Column(String(50))
    last_seen_at = Column(DateTime(timezone=True))
    created_at = Column(DateTime(timezone=True), server_default=text("CURRENT_TIMESTAMP"))

    owner = relationship("User", back_populates="devices")
    configs = relationship("DeviceConfig", back_populates="device", cascade="all, delete-orphan")
    telemetry = relationship("DeviceTelemetry", back_populates="device", cascade="all, delete-orphan")

class DeviceConfig(Base):
    __tablename__ = "device_configs"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    planta = Column(String(255))
    enable = Column(Boolean)
    fp_on = Column(Integer)
    fp_off = Column(Integer)
    led_a = Column(Integer)
    led_r = Column(Integer)
    led_b = Column(Integer)
    irr_h = Column(Integer)
    irr_m = Column(Integer)
    vent_h = Column(Integer)
    vent_m = Column(Integer)
    crop_start_day = Column(Integer)
    applied_at = Column(DateTime(timezone=True), server_default=text("CURRENT_TIMESTAMP"))

    device = relationship("Device", back_populates="configs")

class DeviceTelemetry(Base):
    __tablename__ = "device_telemetry"
    # Note: id and ts form the composite primary key for TimescaleDB
    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    ts = Column(DateTime(timezone=True), primary_key=True, server_default=text("CURRENT_TIMESTAMP"))
    temp = Column(Float)
    humidity = Column(Float)
    firmware_version = Column(String(50))
    wifi_rssi = Column(Integer)
    uptime = Column(Integer)

    device = relationship("Device", back_populates="telemetry")

class FirmwareRelease(Base):
    __tablename__ = "firmware_releases"
    version = Column(String(50), primary_key=True)
    url = Column(String, nullable=False)
    sha256 = Column(String(64), nullable=False)
    signature = Column(String, nullable=False)
    min_version = Column(String(50), nullable=False)
    published_at = Column(DateTime(timezone=True), server_default=text("CURRENT_TIMESTAMP"))

# --- Pydantic Schemas ---
from pydantic import BaseModel
from typing import Optional
from datetime import datetime

class Token(BaseModel):
    access_token: str
    token_type: str

class TokenData(BaseModel):
    username: Optional[str] = None
    device_id: Optional[str] = None

class DeviceProvisionRequest(BaseModel):
    user: str
    password: str
    mac: str

class DeviceConfigCreate(BaseModel):
    planta: str
    enable: bool
    fp_on: int
    fp_off: int
    led_a: int
    led_r: int
    led_b: int
    irr_h: int
    irr_m: int
    vent_h: int
    vent_m: int
    crop_start_day: int