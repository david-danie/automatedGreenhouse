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

Ver [`../docs/ROADMAP.md`](../docs/ROADMAP.md) para el detalle.

1. ✅ Esqueleto IDF que compila y arranca.
2. ✅ Periféricos + lógica de control: NVS (`storage`), I²C/DS3231 (`rtc`),
   LEDC/GPIO (`actuators`) y control (`control`, con `utils`/`Constants` portados).
   El `app_main` ya corre el bucle de control periódico (luces/riego/ventilación).
   Pendiente: build/flash y prueba en hardware.
3. ⬜ Wi-Fi AP+STA (`esp_wifi` / `esp_netif`).
4. ⬜ HTTP (`esp_http_server`) + DNS captivo + servir el portal gzip + OTA local.
5. ⬜ OTA segura sobre TLS (pinning + firma + rollback).

> La lógica portable (fechas, UTF-8, escalado PWM, fotoperiodo y scheduling,
> incluido el nuevo intervalo de 336 h = cada 2 semanas) está verificada con
> tests en el host; el build C++ completo se valida con `idf.py build`.

## Estructura

```
firmware/
├── CMakeLists.txt        # proyecto IDF raíz
├── sdkconfig.defaults    # config reproducible (partición, flash 4MB, consola)
├── partitions.csv        # doble slot OTA (4 MB)
└── main/
    ├── CMakeLists.txt     # fuentes + REQUIRES del componente principal
    ├── main.cpp           # app_main: init + bucle de control
    ├── Constants.h        # pines, PWM, enums, límites (sin Arduino)
    ├── utils.h/.cpp       # helpers puros (BCD, UTF-8, PBKDF2, hex, fechas)
    ├── storage.h/.cpp     # persistencia NVS (CropConfig versionado)
    ├── rtc.h/.cpp         # DS3231 por i2c_master (modo seguro OSF)
    ├── actuators.h/.cpp   # PWM (LEDC) + salidas digitales (GPIO)
    └── control.h/.cpp     # fotoperiodo + scheduling stateless
    └── main.cpp           # app_main (punto de entrada)
```
