CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Tabla de Usuarios (1 usuario = 1 cuenta)
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Tabla de Dispositivos (1:N con usuarios)
CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    mac VARCHAR(17) UNIQUE NOT NULL,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(255),
    token_hash VARCHAR(255),
    firmware_version VARCHAR(50),
    last_seen_at TIMESTAMP WITH TIME ZONE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Histórico de Configuración del Dispositivo
CREATE TABLE device_configs (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    planta VARCHAR(255),
    enable BOOLEAN,
    fp_on INTEGER,
    fp_off INTEGER,
    led_a INTEGER,
    led_r INTEGER,
    led_b INTEGER,
    irr_h INTEGER,
    irr_m INTEGER,
    vent_h INTEGER,
    vent_m INTEGER,
    crop_start_day INTEGER,
    applied_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Telemetría (Serie Temporal con TimescaleDB)
CREATE TABLE device_telemetry (
    id SERIAL,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    ts TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    temp REAL,
    humidity REAL,
    firmware_version VARCHAR(50),
    wifi_rssi INTEGER,
    uptime INTEGER,
    PRIMARY KEY (id, ts)
);

-- Convertir device_telemetry en una hypertable
SELECT create_hypertable('device_telemetry', 'ts');

-- Índices recomendados para la telemetría y configs
CREATE INDEX ix_device_telemetry_device_id_ts ON device_telemetry (device_id, ts DESC);
CREATE INDEX ix_device_configs_device_id_applied_at ON device_configs (device_id, applied_at DESC);

-- Catálogo de Firmware para OTA
CREATE TABLE firmware_releases (
    version VARCHAR(50) PRIMARY KEY,
    url TEXT NOT NULL,
    sha256 VARCHAR(64) NOT NULL,
    signature TEXT NOT NULL,
    min_version VARCHAR(50) NOT NULL,
    published_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);
