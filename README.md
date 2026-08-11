<div align="center">

# 🌱 SmartPlant

### Controlador de cultivo IoT sobre ESP32-C3 — firmware + portal captivo

*Un dispositivo que controla iluminación por espectro, riego y ventilación de un
cultivo, y se configura desde el celular vía un portal Wi-Fi servido por el propio
microcontrolador — sin app, sin nube obligatoria, sin frameworks.*

`C++` · `ESP32-C3` · `Arduino / ESP-IDF` · `Vanilla JS` · `HTML/CSS` · `mbedTLS (planned)` · `FastAPI + PostgreSQL (planned)`

**[English](README.en.md)** · **Español**

<br>

<!-- TODO: reemplazar por una captura/hero real del portal (dashboard en un móvil) -->
![Portal SmartPlant](docs/img/portal-hero.png)

</div>

---

## ✨ Qué demuestra este proyecto

Un sistema **embebido de extremo a extremo** construido bajo restricciones reales
de hardware (1 núcleo RISC-V @160 MHz, RAM y flash limitadas):

- **Firmware en C++** para ESP32-C3: PWM multicanal, relés, RTC por I²C, persistencia en NVS, servidor HTTP y DNS captivo.
- **Frontend embebido**: una SPA completa (máquina de estados) en **un solo HTML con vanilla JS**, servida desde el flash del dispositivo — sin build, sin dependencias.
- **Diseño con trade-offs documentados**: cada decisión no trivial (scheduling, sesión, conectividad, validación) está razonada en el código y en la doc técnica.
- **Mentalidad de producto**: UX cuidada (transiciones, estados de error, accesibilidad), seguridad por capas y un roadmap de backend/OTA pensado para el hardware.

---

## 🧠 Decisiones de ingeniería destacadas

> La parte interesante no es *qué* hace, sino *cómo* resuelve los problemas difíciles del mundo embebido.

| Reto | Solución |
|------|----------|
| **Que el riego sobreviva cortes de luz** | Scheduling **stateless**: en vez de un contador a medianoche (que se perdería al apagarse), el encendido se recalcula desde un contador continuo de horas derivado del RTC (`epochHours % intervalo`). El mismo modelo cubre riego sub-diario y espaciado de varios días. |
| **Edad del cultivo correcta tras estar apagado** | `día`/`semana` se **derivan del calendario real** (ancla `cropStart` en NVS), no de un contador — el cultivo "sigue envejeciendo" aunque el equipo estuviera off, sin desgastar flash cada noche. |
| **Conectar a Wi-Fi sin congelar el portal** | Conexión STA **no bloqueante** + **polling** desde el front: el `POST` solo *arranca* el intento y responde al instante; el cliente confirma por `/getparams`. Evita bloquear `handleClient()` varios segundos. |
| **No guardar una contraseña Wi-Fi que no sirve** | **Persistencia diferida**: las credenciales se escriben en NVS *solo* cuando la conexión llega a `WL_CONNECTED` — nada de bucles de reintento tras reiniciar. |
| **Autenticación sin reenviar credenciales** | **Token de sesión** de 128 bits (`esp_random()`), TTL fijo, en RAM (muere con el reboot, expiración por `millis()` a prueba de wrap-around). El AP va cifrado con **WPA2-PSK**. |
| **Validación idéntica en navegador y dispositivo** | Reglas replicadas **bit a bit** front↔firmware, contando por **carácter UTF-8** (no bytes), para que acentos y ñ no descuadren los límites entre JS y C++. |
| **Menos flash y carga más rápida del portal** | El HTML legible (fuente de verdad) se **regenera sin comentarios** al artefacto que sirve el ESP32 (~73 → ~56 KB), con `gzip` como siguiente paso (~12–18 KB). |

---

## 🔌 Características

- 📶 **Portal captivo Wi-Fi** (AP "SmartPlant" con WPA2): se configura desde cualquier navegador, sin instalar nada.
- 💡 **Iluminación por espectro** (azul / rojo / blanco) vía PWM, con fotoperiodo que puede cruzar medianoche.
- 💧 **Riego y ventilación** programables por intervalo (de cada hora a semanal).
- 🕒 **RTC externo DS3231**: la hora se sincroniza desde el navegador.
- 🌐 **Conectividad Wi-Fi del usuario** (modo AP+STA condicional) para habilitar nube/telemetría a futuro.
- 🔐 **Sesión con token** + reset de fábrica protegido.
- ⚡ **Aplicación de parámetros en vivo** (sin reiniciar el dispositivo).

---

## 📸 Capturas

<!-- TODO: reemplazar los placeholders por capturas reales (docs/img/). -->
| Dashboard | Editar parámetros | Configurar Wi-Fi |
|:---:|:---:|:---:|
| ![Dashboard](docs/img/dashboard.png) | ![Edición](docs/img/edit.png) | ![Wi-Fi](docs/img/wifi.png) |

<div align="center">

<!-- TODO: foto del montaje real (ESP32-C3 + LEDs + bomba/ventilador). -->
![Montaje](docs/img/hardware.jpg)

</div>

---

## 🏗️ Arquitectura

```
┌──────────────┐   Wi-Fi (WPA2)   ┌─────────────────────────────┐
│  📱 Cliente   │ ────────────────▶│   ESP32-C3  "SmartPlant"    │
│ (navegador)  │   HTTP / JSON    │ ┌─────────────────────────┐ │
└──────────────┘                  │ │ Portal captivo (AP)     │ │
                                  │ │ SPA en 1 HTML (vanilla) │ │
                                  │ └─────────────────────────┘ │
   red del usuario   ◀────────────│ ┌─────────────────────────┐ │
   (Internet, futuro)             │ │ STA  (AP+STA condicional)│ │
                                  │ └─────────────────────────┘ │
                                  │  PWM LEDs · Bomba · Fan ·   │
                                  │  RTC DS3231 · NVS · Token   │
                                  └─────────────────────────────┘
```

El front es una **máquina de estados** (`welcome · register · view · auth · edit · wifi`)
en un único HTML; el firmware expone una API JSON y aplica la configuración al instante.

---

## 🛠️ Hardware

| Componente | GPIO | Notas |
|-----------|------|-------|
| LED blanco | 0 | salida digital (lógica invertida) |
| LED azul | 1 | PWM canal 1 |
| LED rojo | 2 | PWM canal 2 |
| Buzzer | 3 | |
| Ventilador | 7 | relé (lógica invertida) |
| Bomba de agua | 10 | relé (lógica invertida) |
| RTC DS3231 | I²C `0x68` | reloj externo |

PWM a 1 kHz, 8 bits (0–255); los espectros se envían 0–100 % y se escalan internamente.

---

## 📚 Stack tecnológico

- **MCU:** ESP32-C3 (RISC-V, 1 núcleo @160 MHz)
- **Firmware:** C++ (Arduino-ESP32 / ESP-IDF) — `WiFi`, `WebServer`, `DNSServer`, `Wire`, `Preferences`, `ArduinoJson` (v6)
- **Frontend:** HTML + CSS + JavaScript vanilla (sin frameworks, sin build)
- **Persistencia local:** NVS (Preferences)
- **Planeado (backend):** Python (FastAPI), PostgreSQL / TimescaleDB, TLS (mbedTLS), OTA firmada

---

## 📁 Estructura

```
.
├── ESP32_controller/
│   ├── ESP32_controller.ino   # AP Wi-Fi, DNS captivo, rutas HTTP y handlers
│   ├── Plant.h / .cpp      # Lógica del cultivo: validación, PWM/relés, RTC, sesión, Wi-Fi
│   ├── Constants.h         # Pines, PWM, límites de validación, enums de estado/error
│   ├── utils.h / .cpp      # Helpers libres: validación de strings/UTF-8, BCD, fecha
│   ├── sensible.h          # Secretos del AP (no versionado)
│   └── mainForm.h          # Artefacto generado: el HTML que sirve el ESP32
├── HTML/mainForm.html      # Fuente de verdad del portal (legible y comentado)
├── HTML/test/rutas_y_parametros.txt  # Referencia de la API HTTP (rutas, payloads, ejemplos)
└── docs/ARCHITECTURE.md    # Documentación técnica profunda (decisiones y caveats)
```

---

## 🚀 Compilar y flashear

```bash
# Requisitos: Arduino IDE / arduino-cli con el core ESP32 y ArduinoJson (v6).
# 1. Crear sensible.h con las credenciales del AP (ver plantilla).
# 2. Compilar y flashear el sketch ESP32_controller/ a la placa ESP32-C3.
# 3. Conectarse a la red Wi-Fi "SmartPlant" → el portal captivo abre el formulario.
```

> No hay test runner: la validación se hace en hardware. El JS del portal se
> verifica con `node --check` tras regenerar `mainForm.h` desde el HTML fuente.

---

## 🗺️ Roadmap

- [x] Portal captivo + dashboard + edición de parámetros en vivo
- [x] Sesión con token (sin reenviar credenciales)
- [x] **Conectividad Wi-Fi del usuario** (AP+STA, escaneo, conexión + polling)
- [ ] Servir el portal **gzip** (`Content-Encoding: gzip`) para menos flash y carga más rápida
- [ ] **Backend** (FastAPI + Postgres): cuentas, telemetría y consulta entre dispositivos — *diseño cerrado en [docs/BACKEND.md](docs/BACKEND.md), pendiente de implementar*
- [ ] **OTA segura** sobre TLS (CA pinning + firmware firmado)

---

## 📖 Documentación

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — diseño técnico a fondo: decisiones, trade-offs, caveats, modelo de BBDD y estrategia TLS para el C3.
- **[docs/BACKEND.md](docs/BACKEND.md)** — diseño y guía de arranque del backend (FastAPI + Postgres/Timescale + MQTT + S3): decisiones confirmadas, modelo de datos, contratos de API y checklist para empezar a implementar.
- **[HTML/test/rutas_y_parametros.txt](HTML/test/rutas_y_parametros.txt)** — referencia completa de la API HTTP (rutas, payloads y ejemplos).

---

<div align="center">

**SmartPlant v1.0** · Firmware embebido + portal captivo para cultivo IoT

</div>
