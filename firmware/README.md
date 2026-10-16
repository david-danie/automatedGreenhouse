# SmartPlant — Firmware ESP-IDF

Firmware del controlador SmartPlant reescrito en **ESP-IDF puro** (ESP32-C3),
migrando desde el sketch Arduino de [`../ESP32_controller/`](../ESP32_controller/),
que se conserva como **especificación de referencia** durante la migración.

> Rama de trabajo: `esp-idf-migration`. En `main` sigue viviendo el firmware
> Arduino funcional.

## Requisitos

- ESP-IDF (ver versión en el entorno; probado con 5.5).
- Target: `esp32c3`.

## Compilar y flashear

```bash
cd firmware
idf.py set-target esp32c3
idf.py build

# Primer flasheo tras cambiar el layout de particiones: borra el otadata residual.
idf.py -p <puerto> erase-flash

idf.py -p <puerto> flash monitor
```

`sensible.h` (credenciales del AP) **no** se versiona; se recrea en cada máquina
(ver plantilla en el firmware Arduino de referencia).

## Estado de la migración

Hito 1 — **esqueleto que arranca**: `app_main` inicializa, registra info del
sistema por el log y entra en un latido. Sin periféricos ni red todavía.

Próximos hitos (ver [`../docs/ROADMAP.md`](../docs/ROADMAP.md)):

1. ✅ Esqueleto IDF que compila y arranca.
2. Periféricos: NVS, I²C/DS3231, LEDC/GPIO + portar la lógica de negocio
   (`Plant`, `utils`, `Constants`).
3. Wi-Fi AP+STA (`esp_wifi` / `esp_netif`).
4. HTTP (`esp_http_server`) + DNS captivo + servir el portal gzip + OTA local.
5. OTA segura sobre TLS (pinning + firma + rollback).

## Estructura

```
firmware/
├── CMakeLists.txt        # proyecto IDF raíz
├── sdkconfig.defaults    # config reproducible (partición, flash 4MB, consola)
├── partitions.csv        # doble slot OTA (4 MB)
└── main/
    ├── CMakeLists.txt     # fuentes + REQUIRES del componente principal
    └── main.cpp           # app_main (punto de entrada)
```
