<div align="center">

# 🌱 SmartPlant

### IoT crop controller on ESP32-C3 — firmware + captive portal

*A device that controls spectrum lighting, irrigation and ventilation for a crop,
configured from your phone through a Wi-Fi portal served by the microcontroller
itself — no app, no mandatory cloud, no frameworks.*

`C++` · `ESP32-C3` · `Arduino / ESP-IDF` · `Vanilla JS` · `HTML/CSS` · `mbedTLS (planned)` · `FastAPI + PostgreSQL (planned)`

**English** · **[Español](README.md)**

<br>

<!-- TODO: replace with a real hero/screenshot of the portal (dashboard on a phone) -->
![SmartPlant portal](docs/img/portal-hero.png)

</div>

---

## ✨ What this project shows

An **end-to-end embedded system** built under real hardware constraints
(single-core RISC-V @160 MHz, limited RAM and flash):

- **C++ firmware** for the ESP32-C3: multi-channel PWM, relays, I²C RTC, NVS persistence, HTTP server and captive DNS.
- **Embedded frontend**: a full SPA (state machine) in **a single HTML file with vanilla JS**, served from the device's flash — no build, no dependencies.
- **Design with documented trade-offs**: every non-trivial decision (scheduling, session, connectivity, validation) is reasoned out in the code and the technical docs.
- **Product mindset**: polished UX (transitions, error states, accessibility), layered security and a backend/OTA roadmap designed around the hardware.

---

## 🧠 Engineering highlights

> The interesting part isn't *what* it does, but *how* it solves the hard problems of the embedded world.

| Challenge | Solution |
|-----------|----------|
| **Make irrigation survive power outages** | **Stateless** scheduling: instead of a midnight counter (lost on power-off), each activation is recomputed from a continuous hour counter derived from the RTC (`epochHours % interval`). The same model covers sub-daily and multi-day spacing. |
| **Keep crop age correct after being off** | `day`/`week` are **derived from the real calendar** (`cropStart` anchor in NVS), not a counter — the crop "keeps aging" even if the device was off, without wearing out flash every night. |
| **Connect to Wi-Fi without freezing the portal** | **Non-blocking** STA connection + **polling** from the front end: the `POST` only *starts* the attempt and replies instantly; the client confirms via `/getparams`. Avoids blocking `handleClient()` for several seconds. |
| **Never store a Wi-Fi password that doesn't work** | **Deferred persistence**: credentials are written to NVS *only* once the connection reaches `WL_CONNECTED` — no failed-retry loops after a reboot. |
| **Authenticate without resending credentials** | 128-bit **session token** (`esp_random()`), fixed TTL, in RAM (dies on reboot, expiration via `millis()`, wrap-around safe). The AP is encrypted with **WPA2-PSK**. |
| **Identical validation in browser and device** | Rules replicated **bit for bit** front↔firmware, counting by **UTF-8 character** (not bytes), so accents and ñ don't desync the limits between JS and C++. |
| **Less flash and faster portal load** | The readable HTML (source of truth) is **regenerated without comments** into the artifact the ESP32 serves (~73 → ~56 KB), with `gzip` as the next step (~12–18 KB). |

---

## 🔌 Features

- 📶 **Wi-Fi captive portal** (AP "SmartPlant" with WPA2): configure it from any browser, nothing to install.
- 💡 **Spectrum lighting** (blue / red / white) via PWM, with a photoperiod that can cross midnight.
- 💧 **Irrigation and ventilation** scheduled by interval (from hourly to weekly).
- 🕒 **External DS3231 RTC**: time is synced from the browser.
- 🌐 **User Wi-Fi connectivity** (conditional AP+STA mode) to enable cloud/telemetry later.
- 🔐 **Token-based session** + protected factory reset.
- ⚡ **Live parameter application** (no device reboot).

---

## 📸 Screenshots

<!-- TODO: replace the placeholders with real screenshots (docs/img/). -->
| Dashboard | Edit parameters | Wi-Fi setup |
|:---:|:---:|:---:|
| ![Dashboard](docs/img/dashboard.png) | ![Edit](docs/img/edit.png) | ![Wi-Fi](docs/img/wifi.png) |

<div align="center">

<!-- TODO: photo of the real build (ESP32-C3 + LEDs + pump/fan). -->
![Build](docs/img/hardware.jpg)

</div>

---

## 🏗️ Architecture

```
┌──────────────┐   Wi-Fi (WPA2)   ┌─────────────────────────────┐
│  📱 Client    │ ────────────────▶│   ESP32-C3  "SmartPlant"    │
│  (browser)   │   HTTP / JSON    │ ┌─────────────────────────┐ │
└──────────────┘                  │ │ Captive portal (AP)     │ │
                                  │ │ SPA in 1 HTML (vanilla) │ │
                                  │ └─────────────────────────┘ │
   user's network   ◀─────────────│ ┌─────────────────────────┐ │
   (Internet, future)             │ │ STA  (conditional AP+STA)│ │
                                  │ └─────────────────────────┘ │
                                  │  PWM LEDs · Pump · Fan ·    │
                                  │  RTC DS3231 · NVS · Token   │
                                  └─────────────────────────────┘
```

The front end is a **state machine** (`welcome · register · view · auth · edit · wifi`)
in a single HTML file; the firmware exposes a JSON API and applies config instantly.

---

## 🛠️ Hardware

| Component | GPIO | Notes |
|-----------|------|-------|
| White LED | 0 | PWM channel 0 |
| Blue LED | 1 | PWM channel 1 |
| Red LED | 2 | PWM channel 2 |
| Buzzer | 3 | |
| Fan | 7 | relay |
| Water pump | 10 | relay |
| DS3231 RTC | I²C `0x68` | external clock |

PWM at 1 kHz, 8-bit (0–255); spectra are sent 0–100 % and scaled internally.

---

## 📚 Tech stack

- **MCU:** ESP32-C3 (RISC-V, single core @160 MHz)
- **Firmware:** C++ (Arduino-ESP32 / ESP-IDF) — `WiFi`, `WebServer`, `DNSServer`, `Wire`, `Preferences`, `ArduinoJson` (v6)
- **Frontend:** HTML + CSS + vanilla JavaScript (no frameworks, no build)
- **Local persistence:** NVS (Preferences)
- **Planned (backend):** Python (FastAPI), PostgreSQL / TimescaleDB, TLS (mbedTLS), signed OTA

---

## 📁 Structure

```
.
├── ESP32_controller/
│   ├── ESP32_controller.ino   # Wi-Fi AP, captive DNS, HTTP routes and handlers
│   ├── Plant.h / .cpp      # Crop logic: validation, PWM/relays, RTC, session, Wi-Fi
│   ├── Constants.h         # Pins, PWM, validation limits, state/error enums
│   ├── utils.h / .cpp      # Free helpers: string/UTF-8 validation, BCD, date
│   ├── sensible.h          # AP secrets (unversioned)
│   └── mainForm.h          # Generated artifact: the HTML served by the ESP32
├── HTML/mainForm.html      # Portal source of truth (readable and commented)
├── HTML/test/rutas_y_parametros.txt  # HTTP API reference (routes, payloads, examples)
└── docs/ARCHITECTURE.md    # Deep technical documentation (decisions and caveats)
```

---

## 🚀 Build and flash

```bash
# Requirements: Arduino IDE / arduino-cli with the ESP32 core and ArduinoJson (v6).
# 1. Create sensible.h with the AP credentials (see template).
# 2. Build and flash the ESP32_controller/ sketch to the ESP32-C3 board.
# 3. Connect to the "SmartPlant" Wi-Fi network → the captive portal opens the form.
```

> There is no test runner: validation is done on hardware. The portal's JS is
> checked with `node --check` after regenerating `mainForm.h` from the source HTML.

---

## 🗺️ Roadmap

- [x] Captive portal + dashboard + live parameter editing
- [x] Token-based session (no credential resending)
- [x] **User Wi-Fi connectivity** (AP+STA, scan, connect + polling)
- [ ] Serve the portal **gzip** (`Content-Encoding: gzip`) for less flash and faster load
- [ ] **Backend** (FastAPI + Postgres): accounts, telemetry and cross-device queries
- [ ] **Secure OTA** over TLS (CA pinning + signed firmware)

---

## 📖 Documentation

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — in-depth technical design: decisions, trade-offs, caveats, DB model and TLS strategy for the C3.
- **[HTML/test/rutas_y_parametros.txt](HTML/test/rutas_y_parametros.txt)** — complete HTTP API reference (routes, payloads and examples).

---

<div align="center">

**SmartPlant v1.0** · Embedded firmware + captive portal for IoT crop growing

</div>
