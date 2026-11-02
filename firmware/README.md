# SmartPlant — Firmware ESP-IDF

Firmware del controlador SmartPlant reescrito en **ESP-IDF puro** (ESP32-C3),
migrando desde el sketch Arduino de [`../ESP32_controller/`](../ESP32_controller/),
que se conserva como **especificación de referencia**: el objetivo es **paridad de
comportamiento** con él (mismo contrato de [`../docs/API.md`](../docs/API.md), mismas
reglas de seguridad y validación).

> Rama de trabajo: `esp-idf-migration`. En `main` sigue viviendo el firmware
> Arduino funcional. Este archivo **solo existe en esta rama** y es el único lugar
> donde se lleva el avance de la migración; el plan (fases, criterios de
> verificación) está en [`../docs/ROADMAP.md`](../docs/ROADMAP.md), común a ambas ramas.

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

`main/sensible.h` (credenciales del AP) **no** se versiona; se crea en cada máquina
a partir de [`main/sensible.h.example`](main/sensible.h.example).

### Portal embebido (`main/mainForm.h`)

`main/mainForm.h` es el mismo artefacto gzip que genera `scripts/gen_mainform.py`
para el firmware Arduino (`ESP32_controller/mainForm.h`), con un bloque que define
`PROGMEM` vacío (no existe en IDF). Tras regenerar el portal hay que actualizar
también esta copia; si no, el firmware IDF sirve una versión anterior.

## Estado de la migración

Fases según [`../docs/ROADMAP.md`](../docs/ROADMAP.md). Los commits usan la
numeración de *hitos* (hito 1 = fase 0, hito 2 = fase 1, hito 3 = fase 2,
hito 4 = fase 3).

| Fase | Descripción | Código | Build / hardware |
|---|---|---|---|
| 0 | Esqueleto IDF + `partitions.csv` doble slot | ✅ | ⬜ Pendiente |
| 1 | Periféricos (NVS, I²C/DS3231, LEDC/GPIO) + lógica de control | ✅ | ⬜ Pendiente |
| 2 | Wi-Fi AP+STA (`esp_wifi`/`esp_netif`), escaneo asíncrono, persistencia diferida, sin modem-sleep | ✅ | ⬜ Pendiente |
| 3 | HTTP (`esp_http_server`, 11 rutas), portal gzip, DNS captivo, OTA local | 🟨 Falta paridad (ver abajo) | ⬜ Pendiente |
| 4 | Cierre sin Arduino (sin `String`/`arduino-esp32`) + recorte `menuconfig` + medir flash/RAM | 🟨 El código ya nace sin `String`; falta recorte y medición | ⬜ Pendiente |
| 5 | OTA segura sobre TLS (pinning, firma, rollback) | ⬜ | ⬜ |

> La lógica portable (fechas, UTF-8, escalado PWM, fotoperiodo y scheduling,
> incluido el intervalo de 336 h = cada 2 semanas) está verificada con tests en el
> host. No consta en el repositorio un `idf.py build` ni pruebas en hardware del
> firmware IDF completo.

### Pendiente para la paridad con Arduino (fase 3)

- **Portal solo por la interfaz del AP.** Arduino liga el servidor a `192.168.4.1`;
  `esp_http_server` escucha en todas las interfaces, así que en AP+STA el portal
  queda accesible desde la red del usuario. Hoy solo `/otaupdate` y el reset
  comprueban la interfaz (`fromAP`).
- **Catch-all del portal captivo solo para `GET`.** Arduino (`onNotFound`) redirige
  cualquier método; aquí un método distinto a `GET` en una ruta desconocida recibe
  el 404 por defecto.
- **Rollback de OTA.** `sdkconfig.defaults` activa
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, pero el firmware no llama a
  `esp_ota_mark_app_valid_cancel_rollback()`: tras un OTA, la imagen nueva queda
  pendiente de verificar y el bootloader volvería a la anterior en el siguiente
  reinicio.
- **Copia manual de `mainForm.h`** (ver arriba).

## Estructura

```
firmware/
├── CMakeLists.txt        # proyecto IDF raíz
├── sdkconfig.defaults    # config reproducible (partición, flash 4MB, consola, rollback)
├── partitions.csv        # doble slot OTA (4 MB)
└── main/
    ├── CMakeLists.txt       # fuentes + REQUIRES del componente principal
    ├── idf_component.yml    # dependencias gestionadas (ArduinoJson v6, header-only)
    ├── main.cpp             # app_main: init + Wi-Fi + HTTP + DNS + bucle de control
    ├── Constants.h          # pines, PWM, enums, límites (sin Arduino)
    ├── sensible.h.example   # plantilla de credenciales del AP (sensible.h no se versiona)
    ├── mainForm.h           # portal gzip embebido (copia del artefacto de gen_mainform.py)
    ├── utils.h/.cpp         # helpers puros (BCD, UTF-8, PBKDF2, hex, fechas)
    ├── storage.h/.cpp       # persistencia NVS (CropConfig versionado)
    ├── rtc.h/.cpp           # DS3231 por i2c_master (modo seguro OSF)
    ├── actuators.h/.cpp     # PWM (LEDC) + salidas digitales (GPIO)
    ├── control.h/.cpp       # fotoperiodo + scheduling stateless
    ├── wifi.h/.cpp          # AP+STA, escaneo asíncrono, persistencia diferida
    ├── httpserver.h/.cpp    # esp_http_server: portal y endpoints de la API
    ├── httpresp.h/.cpp      # requestStatus -> respuesta HTTP
    ├── captivedns.h/.cpp    # DNS captivo (responde la IP del AP)
    ├── session.h/.cpp       # token de sesión + límite de intentos de login
    ├── auth.h/.cpp          # registro, login y reset de fábrica
    ├── cropparams.h/.cpp    # validación de /newparams y /newcrop
    └── ota.h/.cpp           # OTA local (esp_ota_ops)
```
