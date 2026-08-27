"""initial schema

Revision ID: 001
Revises:
Create Date: 2026-08-25

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

revision: str = "001"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    # --- Extensión TimescaleDB ---
    op.execute("CREATE EXTENSION IF NOT EXISTS timescaledb")

    # --- users ---
    op.create_table(
        "users",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("email", sa.String(255), unique=True, nullable=False),
        sa.Column("password_hash", sa.String(255), nullable=False),
        sa.Column("is_admin", sa.Boolean, server_default="false"),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()")),
    )

    # --- devices ---
    op.create_table(
        "devices",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("mac", sa.String(17), unique=True, nullable=False),
        sa.Column("user_id", sa.Integer, sa.ForeignKey("users.id", ondelete="CASCADE"), nullable=False),
        sa.Column("name", sa.String(255)),
        sa.Column("token_hash", sa.String(255)),
        sa.Column("firmware_version", sa.String(50)),
        sa.Column("last_seen_at", sa.DateTime(timezone=True)),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()")),
    )

    # --- device_configs ---
    op.create_table(
        "device_configs",
        sa.Column("id", sa.Integer, primary_key=True, autoincrement=True),
        sa.Column("device_id", sa.Integer, sa.ForeignKey("devices.id", ondelete="CASCADE"), nullable=False),
        sa.Column("planta", sa.String(255)),
        sa.Column("enable", sa.Boolean),
        sa.Column("fp_on", sa.Integer),
        sa.Column("fp_off", sa.Integer),
        sa.Column("led_a", sa.Integer),
        sa.Column("led_r", sa.Integer),
        sa.Column("led_b", sa.Integer),
        sa.Column("irr_h", sa.Integer),
        sa.Column("irr_m", sa.Integer),
        sa.Column("vent_h", sa.Integer),
        sa.Column("vent_m", sa.Integer),
        sa.Column("crop_start_day", sa.Integer),
        sa.Column("applied_at", sa.DateTime(timezone=True), server_default=sa.text("now()")),
    )

    # --- device_telemetry (hypertable) ---
    op.create_table(
        "device_telemetry",
        sa.Column("id", sa.Integer, autoincrement=True),
        sa.Column("device_id", sa.Integer, sa.ForeignKey("devices.id", ondelete="CASCADE"), nullable=False),
        sa.Column("ts", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
        sa.Column("temp", sa.Float),
        sa.Column("humidity", sa.Float),
        sa.Column("firmware_version", sa.String(50)),
        sa.Column("wifi_rssi", sa.Integer),
        sa.Column("uptime", sa.Integer),
        # PK compuesta: Timescale exige que la columna de partición esté en la PK
        sa.PrimaryKeyConstraint("id", "ts"),
    )

    # Convertir en hypertable
    op.execute("SELECT create_hypertable('device_telemetry', 'ts')")

    # --- firmware_releases ---
    op.create_table(
        "firmware_releases",
        sa.Column("version", sa.String(50), primary_key=True),
        sa.Column("url", sa.Text, nullable=False),
        sa.Column("sha256", sa.String(64), nullable=False),
        sa.Column("signature", sa.Text, nullable=False),
        sa.Column("min_version", sa.String(50), nullable=False),
        sa.Column("published_at", sa.DateTime(timezone=True), server_default=sa.text("now()")),
    )

    # --- Índices ---
    op.create_index("ix_device_telemetry_device_id_ts", "device_telemetry", ["device_id", sa.text("ts DESC")])
    op.create_index("ix_device_configs_device_id_applied_at", "device_configs", ["device_id", sa.text("applied_at DESC")])


def downgrade() -> None:
    op.drop_index("ix_device_configs_device_id_applied_at", table_name="device_configs")
    op.drop_index("ix_device_telemetry_device_id_ts", table_name="device_telemetry")
    op.drop_table("firmware_releases")
    op.drop_table("device_telemetry")
    op.drop_table("device_configs")
    op.drop_table("devices")
    op.drop_table("users")
    op.execute("DROP EXTENSION IF EXISTS timescaledb")
