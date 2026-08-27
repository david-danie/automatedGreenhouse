# SmartPlant — Controlador de Cultivo IoT

<div align="justify">

SmartPlant es un sistema embebido para controlar un cultivo indoor: iluminación por espectro, riego y ventilación, todo configurable desde el celular a través de un portal Wi-Fi que sirve el propio microcontrolador. Nada de apps, nada de nube obligatoria, nada de frameworks de terceros corriendo por debajo. Lo empecé pensando en hortalizas, con la idea de ir ganando algo de independencia respecto a lo que como día a día, y de ahí se fue armando el resto.

</div>

<br>
<div align="center">
  <img src="./docs/img/descripcion.png" alt="Diagrama general del sistema" width="675" height="345"/>
</div>
<br>

<div align="justify">

Arrancó como una tarjeta con un ATmega328P, BLE para comunicarse y SSR para mover los contactores. De ahí migré a un **ESP32-C3** (RISC-V, 1 núcleo a 160 MHz), sumé el portal captivo y un frontend embebido, y ahora estoy encarando el backend con telemetría y OTA segura.

</div>

<br>

`C++` · `ESP32-C3` · `Arduino / ESP-IDF` · `Vanilla JS` · `HTML/CSS` · `mbedTLS (planned)` · `FastAPI + PostgreSQL (in progress)`

---

## CONTENIDO

* [QUÉ HAY DETRÁS DE ESTE PROYECTO.](#qué-hay-detrás-de-este-proyecto)
* [DECISIONES DE INGENIERÍA.](#decisiones-de-ingeniería)
* [CARACTERÍSTICAS.](#características)
* [ARQUITECTURA.](#arquitectura)
* [EL PORTAL.](#el-portal)
* [DISPOSITIVOS A CONTROLAR.](#dispositivos-a-controlar)
  - [Lámparas de cultivo.](#lámparas-de-cultivo)
  - [Bomba para irrigación.](#bomba-para-irrigación)
  - [Ventilación/Extracción.](#ventilaciónextracción)
* [TARJETA DE CONTROL.](#tarjeta-de-control)
* [STACK TECNOLÓGICO.](#stack-tecnológico)
* [ESTRUCTURA DEL REPOSITORIO.](#estructura-del-repositorio)
* [COMPILAR Y FLASHEAR.](#compilar-y-flashear)
* [ROADMAP.](#roadmap)
* [DOCUMENTACIÓN.](#documentación)

---

## QUÉ HAY DETRÁS DE ESTE PROYECTO

<div align="justify">

Es un sistema embebido de punta a punta, hecho con las restricciones reales de un microcontrolador chico: un núcleo RISC-V a 160 MHz, RAM y flash limitadas. Nada de eso es teórico, se nota en cada decisión de diseño.

- El firmware está en C++ para el ESP32-C3: PWM multicanal, relés, RTC por I²C, persistencia en NVS, servidor HTTP y DNS captivo corriendo todo junto.
- El frontend es una SPA completa metida en un solo archivo HTML con JS vanilla, servida directo desde el flash del dispositivo. No hay build ni dependencias externas involucradas.
- Las decisiones que no son triviales —scheduling, manejo de sesión, conectividad, validación— están explicadas en el código y en la documentación técnica, no escondidas.
- Traté de cuidar también la parte de producto: transiciones, estados de error, algo de accesibilidad, seguridad en capas, y un roadmap de backend/OTA pensado en función del hardware que tengo, no al revés.

</div>

---

## DECISIONES DE INGENIERÍA

<div align="justify">

Lo interesante de un proyecto embebido casi nunca es qué hace, sino cómo resolvió los problemas que el hardware te obliga a enfrentar. Estas son las decisiones que más peso tuvieron:

</div>

| Reto | Solución |
|------|----------|
| Que el riego sobreviva a un corte de luz | Scheduling stateless: el encendido se recalcula desde un contador continuo de horas que sale del RTC (`epochHours % intervalo`). El mismo modelo cubre riego sub-diario y espaciado de varios días, sin depender de un contador que se pierde al apagarse el equipo. |
| Que la edad del cultivo sea correcta después de estar apagado | `día` y `semana` se derivan del calendario real (con `cropStart` anclado en NVS), no de un contador. El cultivo sigue "envejeciendo" aunque el equipo haya estado apagado, y no gasto flash escribiendo cada noche. |
| Conectarse a Wi-Fi sin congelar el portal | Conexión STA no bloqueante más polling desde el front: el `POST` solo dispara el intento y responde al instante, y el cliente confirma después vía `/getparams`. Así evito bloquear `handleClient()` varios segundos. |
| No guardar una contraseña Wi-Fi que ni siquiera sirve | Persistencia diferida: las credenciales se escriben en NVS recién cuando la conexión llega a `WL_CONNECTED`. Nada de reintentos en bucle después de reiniciar con credenciales que estaban mal. |
| Autenticarse sin reenviar credenciales todo el tiempo | Token de sesión de 128 bits (`esp_random()`), con TTL fijo en RAM —muere solo con el reboot— y expiración por `millis()` a prueba de wrap-around. El AP además va cifrado con WPA2-PSK. |
| Que la validación sea idéntica en el navegador y en el dispositivo | Las mismas reglas replicadas bit a bit entre front y firmware, contando por carácter UTF-8 y no por byte, para que los acentos y la ñ no rompan los límites entre JS y C++. |
| Servir 50 KB de HTML sin quedarme sin heap | `send_P` manda el portal en trozos directo desde flash. Con `send()` y un `const char*` se armaba un `String` temporal del tamaño completo, y ese pico de memoria a veces fallaba en modo AP+STA dejando el formulario sin cargar. |
| Que el portal pese menos y cargue más rápido | El HTML legible, que es la fuente de verdad, se regenera sin comentarios al artefacto que sirve el ESP32 (de ~57 a ~49 KB). El siguiente paso es `gzip`, que debería bajarlo a algo entre 12 y 18 KB. |

---

## CARACTERÍSTICAS

<div align="justify">

- Portal captivo Wi-Fi (AP "SmartPlant" con WPA2): se configura desde cualquier navegador, sin instalar nada.
- Iluminación por espectro (azul y rojo por PWM, blanco ON/OFF), con fotoperiodo que puede cruzar la medianoche.
- Riego y ventilación programables por intervalo, desde cada hora hasta semanal.
- RTC externo DS3231, con la hora sincronizada desde el navegador del usuario.
- Conectividad Wi-Fi del usuario (AP+STA condicional) pensada para habilitar nube y telemetría más adelante.
- Sesión con token y reset de fábrica protegido.
- Los parámetros se aplican en vivo, sin reiniciar el dispositivo.

</div>

---

## ARQUITECTURA

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

<div align="justify">

El frontend es una máquina de estados (`welcome · register · view · auth · edit · wifi`) que vive entera en un archivo HTML; el firmware expone una API JSON y aplica la configuración al instante. Toda la comunicación entre navegador y microcontrolador queda dentro de la red local del AP, así que no hace falta internet para que esto funcione.

Cada decisión está explicada con más detalle en [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), y el contrato completo de la API en [`docs/API.md`](docs/API.md).

</div>

---

## EL PORTAL

<div align="justify">

Al conectarte a la red **SmartPlant**, el sistema operativo detecta el portal captivo y abre el formulario solo. Todo el frontend vive en un único HTML con JS vanilla, servido desde el flash del microcontrolador: sin build, sin CDN, todo autocontenido.

</div>

<br>
<table align="center">
  <tr>
    <th>Dashboard.</th>
    <th>Edición de parámetros.</th>
    <th>Configuración de red.</th>
  </tr>
  <tr>
    <td><img src="./docs/img/portal-dashboard.png" alt="Dashboard del portal" width="240"/></td>
    <td><img src="./docs/img/portal-edit.png" alt="Edición de parámetros" width="240"/></td>
    <td><img src="./docs/img/portal-wifi.png" alt="Configuración de red Wi-Fi" width="240"/></td>
  </tr>
</table>
<br>

<div align="justify">

El dashboard muestra el estado del cultivo en modo lectura; para editar algo hay que autenticarse, y eso emite un token de sesión que dura 30 minutos. La vista de red escanea las señales disponibles y te deja conectar el dispositivo a tu Wi-Fi sin perder el acceso al portal en el proceso.

</div>

---

## DISPOSITIVOS A CONTROLAR

<div align="justify">

Un invernadero chico, o incluso un espacio dedicado dentro de la ciudad, puede aprovechar bastante de lo que ya se usa en agricultura protegida a mayor escala: automatizar riego, ventilación o extracción de aire, controlar el fotoperiodo y medir cosas como temperatura, humedad o pH. La idea de SmartPlant es cubrir esa parte, empezando por lo esencial.

</div>

#### LÁMPARAS DE CULTIVO.

<div align="justify">

El sol es, por lejos, la fuente de luz más potente que hay, y dentro de su espectro la radiación fotosintéticamente activa (PAR) es la que realmente mueve el desarrollo de la planta, sobre todo en los tonos azules y rojos. Hoy en día los LED dan una alternativa razonable para reemplazar eso en interior. En la implementación actual hay 2 canales PWM (azul y rojo) que controlan la intensidad de cada espectro de forma granular, de 0 a 100%, más un canal digital ON/OFF para el blanco, con un fotoperiodo configurable que admite cruzar la medianoche.

</div>

<br>
<table align="center">
  <tr>
    <th>LEDs rojos y azules.</th>
    <th>LEDs blancos.</th>
    <th>Otras lámparas.</th>
  </tr>
</table>
<div align="center">
  <img src="./docs/img/hyd_l.jpg" alt="LEDs rojos y azules" width="100" height="150"/>&emsp;&emsp;&emsp;&emsp;
  <img src="./docs/img/phi_lw.jpg" alt="LEDs blancos" width="100" height="150"/>&emsp;&emsp;&emsp;&emsp;
  <img src="./docs/img/phi_so.jpg" alt="Otras lámparas" width="100" height="150"/>
</div>
<br>

#### BOMBA PARA IRRIGACIÓN.

<div align="justify">

Hay varios métodos de riego —aspersión, goteo, gravedad, película de nutrientes, entre otros— pero en el fondo casi todos se reducen a lo mismo: prender y apagar una bomba de agua o una electroválvula en el momento justo. Por eso el sistema generaliza esa tarea. El scheduling es stateless y recalcula los ciclos de riego directamente desde el RTC, así que el riego sigue funcionando bien incluso después de un corte de energía, sin necesidad de guardar contadores en ningún lado.

</div>

<br>
<table align="center">
  <tr>
    <th>&emsp;Goteo.&emsp;</th>
    <th>&emsp;Gravedad.&emsp;</th>
    <th>&emsp;Método NFT.&emsp;</th>
    <th>&emsp;Válvula E.&emsp;</th>
  </tr>
</table>
<div align="center">
  <img src="./docs/img/goteo.jpg" alt="Riego por goteo" width="100" height="150"/>&emsp;&emsp;&emsp;&emsp;
  <img src="./docs/img/grav1.jpg" alt="Riego por gravedad" width="100" height="150"/>&emsp;&emsp;&emsp;&emsp;
  <img src="./docs/img/NFT.jpg" alt="Método NFT" width="100" height="150"/>&emsp;&emsp;&emsp;&emsp;
  <img src="./docs/img/valve.jpg" alt="Electroválvula" width="100" height="150"/>
</div>
<br>

#### VENTILACIÓN/EXTRACCIÓN.

<div align="justify">

El aire dentro del espacio de cultivo afecta directo la temperatura, y esa temperatura afecta cómo se desarrolla la planta. Cuando hace falta forzar la circulación de aire limpio o sacar el aire viciado, entra este módulo: un relé de lógica invertida que prende el ventilador o extractor en intervalos que se configuran desde el portal.

</div>

---

## TARJETA DE CONTROL

<div align="justify">

Arrancó con un ATmega328P de Microchip/Atmel (8 bits, 32 KB de flash, I²C, UART, PWM), usando el bootloader de Arduino UNO y BLE por UART para actualizar variables del cultivo. La versión actual pasó a un ESP32-C3 (RISC-V, 1 núcleo a 160 MHz), que trae Wi-Fi nativo, más memoria, y permite servir un portal web completo directamente desde el microcontrolador sin depender de nada externo.

</div>

| Componente | GPIO | Notas |
|-----------|------|-------|
| LED blanco | 0 | Salida digital (lógica invertida) |
| LED azul | 1 | PWM canal 1 |
| LED rojo | 2 | PWM canal 2 |
| Buzzer | 3 | Señalización sonora |
| Ventilador | 7 | Relé (lógica invertida) |
| Bomba de agua | 10 | Relé (lógica invertida) |
| RTC DS3231 | I²C `0x68` | Reloj externo para scheduling |

<div align="justify">

El PWM va a 1 kHz con resolución de 8 bits (0–255). Los valores de espectro se mandan desde el portal como porcentaje (0–100%) y el firmware se encarga de escalarlos internamente.

El detalle completo —esquemáticos, salidas SSR, RTC, instalación eléctrica y la tarjeta original— está en [`docs/HARDWARE.md`](docs/HARDWARE.md).

</div>

<br>
<table align="center">
  <tr>
    <th>&emsp;&emsp;Vista superior.&emsp;&emsp;</th>
    <th>&emsp;&emsp;Vista inferior.&emsp;&emsp;</th>
    <th>&emsp;&emsp;Tarjeta electrónica.&emsp;&emsp;</th>
  </tr>
</table>
<div align="center">
  <img src="./docs/img/picTop.png" alt="Vista superior del PCB" width="180" height="300"/>&emsp;
  <img src="./docs/img/picBottom.png" alt="Vista inferior del PCB" width="180" height="300"/>&emsp;
  <img src="./docs/img/picBoard.jpg" alt="Tarjeta electrónica" width="180" height="300"/>
</div>
<br>

---

## STACK TECNOLÓGICO

<div align="justify">

- **MCU:** ESP32-C3 (RISC-V, 1 núcleo @160 MHz)
- **Firmware:** C++ (Arduino-ESP32 / ESP-IDF) — `WiFi`, `WebServer`, `DNSServer`, `Wire`, `Preferences`, `ArduinoJson` (v6)
- **Frontend:** HTML + CSS + JS vanilla, sin frameworks ni build
- **Persistencia local:** NVS (Preferences)
- **Backend** (diseño ya cerrado, implementación en curso): Python con FastAPI, PostgreSQL + TimescaleDB, MQTT, almacenamiento compatible con S3
- **Pendiente:** TLS con mbedTLS y OTA firmada

</div>

---

## ESTRUCTURA DEL REPOSITORIO

```
.
├── ESP32_controller/          # Firmware (sketch de Arduino)
│   ├── ESP32_controller.ino   # AP Wi-Fi, DNS captivo, rutas HTTP y handlers
│   ├── Plant.h / .cpp         # Lógica del cultivo: validación, PWM/relés, RTC, sesión, Wi-Fi
│   ├── Constants.h            # Pines, PWM, límites de validación, enums de estado/error
│   ├── utils.h / .cpp         # Helpers: validación de strings/UTF-8, BCD, fecha
│   ├── sensible.h             # Secretos del AP (no versionado)
│   └── mainForm.h             # Artefacto generado: el HTML que sirve el ESP32
├── HTML/
│   └── mainForm.html          # Fuente de verdad del portal (legible y comentada)
├── ESP32_Board/               # Diseño de la tarjeta (KiCad)
├── pythonServer/              # Backend: FastAPI + Alembic + Docker (auth funcional)
├── docs/                      # Documentación técnica
│   ├── README.md              # Índice de la documentación
│   ├── ARCHITECTURE.md        # Diseño del firmware y del portal
│   ├── API.md                 # Referencia de la API HTTP
│   ├── HARDWARE.md            # Pines, periféricos, instalación eléctrica
│   ├── BACKEND.md             # Diseño del backend
│   └── img/                   # Imágenes y diagramas
├── README.md
└── README.en.md
```

<div align="justify">

Convención del portal: se edita `HTML/mainForm.html` (la fuente legible y comentada) y de ahí se regenera `ESP32_controller/mainForm.h`, que es lo que termina sirviendo el dispositivo. Nunca al revés — si editás el `.h` directamente, se pierde en el próximo regenerado.

</div>

---

## COMPILAR Y FLASHEAR

```bash
# Requisitos: Arduino IDE / arduino-cli con el core ESP32 y ArduinoJson (v6).
# 1. Crear sensible.h con las credenciales del AP (hay una plantilla en el código).
# 2. Compilar y flashear el sketch ESP32_controller/ a la placa ESP32-C3.
# 3. Conectarse a la red Wi-Fi "SmartPlant" → el portal captivo abre el formulario solo.
```

<div align="justify">

No hay test runner automatizado, la validación real se hace directo en hardware. Lo único que corro aparte es `node --check` sobre el JS del portal, después de regenerar `mainForm.h` desde el HTML fuente.

</div>

---

## ROADMAP

- [x] Portal captivo + dashboard + edición de parámetros en vivo
- [x] Sesión con token (sin reenviar credenciales)
- [x] Conectividad Wi-Fi del usuario (AP+STA, escaneo, conexión + polling)
- [ ] Servir el portal con gzip (`Content-Encoding: gzip`) para que pese menos y cargue más rápido
- [ ] Backend (FastAPI + Postgres/Timescale): cuentas, telemetría y consulta entre dispositivos. El esqueleto ya funciona con auth implementado ([ver estado](pythonServer/README.md)); falta provisión de dispositivos, telemetría, OTA y lecturas para la app
- [ ] OTA segura sobre TLS (CA pinning + firmware firmado)
- [ ] Medición de temperatura y humedad (el DS18B20 ya estaba contemplado en el diseño original)

---

## DOCUMENTACIÓN

<div align="justify">

Toda la documentación técnica vive en [`docs/`](docs/README.md):

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — el diseño técnico a fondo: decisiones, trade-offs, caveats, la máquina de estados del portal, el modelo de BD y la estrategia de TLS pensada para el C3.
- **[docs/API.md](docs/API.md)** — referencia completa de la API HTTP: rutas, payloads, validaciones, catálogo de errores y ejemplos con `curl`.
- **[docs/HARDWARE.md](docs/HARDWARE.md)** — mapa de pines, PWM, RTC, SSR, conectividad e instalación eléctrica.
- **[docs/BACKEND.md](docs/BACKEND.md)** — diseño del backend (FastAPI + Postgres/Timescale + MQTT + S3): decisiones, modelo de datos y contratos de API. Es la referencia para cuando lo termine de implementar.
- **[pythonServer/README.md](pythonServer/README.md)** — cómo levantar el backend local (FastAPI + Postgres/Timescale, MinIO, Mosquitto).

</div>

---

<div align="center">

**SmartPlant v1.0** · Firmware embebido + portal captivo para cultivo IoT

</div>