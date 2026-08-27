"""Modelos SQLAlchemy — espeja pythonServer/db/schema.sql."""

from datetime import datetime

from sqlalchemy import (
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Integer,
    String,
    Text,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.database import Base


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, nullable=False)
    password_hash: Mapped[str] = mapped_column(String(255), nullable=False)
    is_admin: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default="now()"
    )

    devices: Mapped[list["Device"]] = relationship(back_populates="user", cascade="all, delete-orphan")


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    mac: Mapped[str] = mapped_column(String(17), unique=True, nullable=False)
    user_id: Mapped[int] = mapped_column(Integer, ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    name: Mapped[str | None] = mapped_column(String(255))
    token_hash: Mapped[str | None] = mapped_column(String(255))
    firmware_version: Mapped[str | None] = mapped_column(String(50))
    last_seen_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default="now()"
    )

    user: Mapped["User"] = relationship(back_populates="devices")
    configs: Mapped[list["DeviceConfig"]] = relationship(back_populates="device", cascade="all, delete-orphan")
    telemetry: Mapped[list["DeviceTelemetry"]] = relationship(back_populates="device", cascade="all, delete-orphan")


class DeviceConfig(Base):
    __tablename__ = "device_configs"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    device_id: Mapped[int] = mapped_column(Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    planta: Mapped[str | None] = mapped_column(String(255))
    enable: Mapped[bool | None] = mapped_column(Boolean)
    fp_on: Mapped[int | None] = mapped_column(Integer)
    fp_off: Mapped[int | None] = mapped_column(Integer)
    led_a: Mapped[int | None] = mapped_column(Integer)
    led_r: Mapped[int | None] = mapped_column(Integer)
    led_b: Mapped[int | None] = mapped_column(Integer)
    irr_h: Mapped[int | None] = mapped_column(Integer)
    irr_m: Mapped[int | None] = mapped_column(Integer)
    vent_h: Mapped[int | None] = mapped_column(Integer)
    vent_m: Mapped[int | None] = mapped_column(Integer)
    crop_start_day: Mapped[int | None] = mapped_column(Integer)
    applied_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default="now()"
    )

    device: Mapped["Device"] = relationship(back_populates="configs")


class DeviceTelemetry(Base):
    __tablename__ = "device_telemetry"

    # PK compuesta (id, ts) — Timescale exige que la columna de partición esté en la PK
    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    ts: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), primary_key=True, server_default="now()"
    )
    device_id: Mapped[int] = mapped_column(Integer, ForeignKey("devices.id", ondelete="CASCADE"), nullable=False)
    temp: Mapped[float | None] = mapped_column(Float)
    humidity: Mapped[float | None] = mapped_column(Float)
    firmware_version: Mapped[str | None] = mapped_column(String(50))
    wifi_rssi: Mapped[int | None] = mapped_column(Integer)
    uptime: Mapped[int | None] = mapped_column(Integer)

    device: Mapped["Device"] = relationship(back_populates="telemetry")


class FirmwareRelease(Base):
    __tablename__ = "firmware_releases"

    version: Mapped[str] = mapped_column(String(50), primary_key=True)
    url: Mapped[str] = mapped_column(Text, nullable=False)
    sha256: Mapped[str] = mapped_column(String(64), nullable=False)
    signature: Mapped[str] = mapped_column(Text, nullable=False)
    min_version: Mapped[str] = mapped_column(String(50), nullable=False)
    published_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default="now()"
    )
