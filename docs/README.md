# Documentación de SmartPlant

Índice de la documentación técnica. Para la presentación general del proyecto, empieza por el [README de la raíz](../README.md).

---

## Por documento

| Documento | Contenido | Cuándo consultarlo |
|---|---|---|
| [**ARCHITECTURE.md**](ARCHITECTURE.md) | Diseño del firmware y del frontend a fondo: decisiones, trade-offs, caveats, máquina de estados del portal, modelo de BD y estrategia TLS para el C3 | Para entender *por qué* el código está escrito así |
| [**API.md**](API.md) | Referencia completa de la API HTTP **del dispositivo**: rutas, payloads, validaciones, catálogo de errores y ejemplos con `curl` | Para integrar algo contra el dispositivo |
| [**API-backend.md**](API-backend.md) | Referencia de la API HTTP **del backend** (nube): auth, provisión, config/telemetría del dispositivo y lecturas de la app; qué está implementado y qué no | Para integrar la app o el dispositivo contra la nube |
| [**HARDWARE.md**](HARDWARE.md) | Mapa de pines, PWM, RTC, SSR, conectividad e instalación eléctrica | Para montar o modificar el hardware |
| [**BACKEND.md**](BACKEND.md) | Diseño del backend y **referencia autoritativa** para implementarlo: stack, modelo de datos, contratos de API, despliegue | Para retomar el desarrollo del backend |
| [**ROADMAP.md**](ROADMAP.md) | Plan de migración del firmware de Arduino-ESP32 a ESP-IDF, por fases entregables: motivación, clasificación del código, mapeo de APIs y criterios de verificación | Para planear o retomar la migración a ESP-IDF |

## Por tarea

**Quiero flashear el dispositivo y usarlo** → [README § Compilar y flashear](../README.md#compilar-y-flashear), luego [HARDWARE.md](HARDWARE.md) para el conexionado.

**Quiero modificar el portal web** → [ARCHITECTURE.md](ARCHITECTURE.md) explica la máquina de estados y la convención de regenerar `mainForm.h` desde `HTML/mainForm.html`.

**Quiero agregar o cambiar una ruta HTTP** → [API.md](API.md) es el contrato; mantenlo sincronizado con `ESP32_controller.ino` y `Plant.cpp`.

**Quiero integrar la app o el dispositivo contra la nube** → [API-backend.md](API-backend.md) es la referencia de la API del backend (lo implementado y lo pendiente); mantenla sincronizada con `pythonServer/app/routers/`.

**Quiero trabajar en el backend** → [BACKEND.md](BACKEND.md) es el diseño a implementar (referencia única); [pythonServer/README.md](../pythonServer/README.md) explica cómo levantar la infraestructura local.

**Quiero cambiar un pin o un periférico** → [HARDWARE.md](HARDWARE.md), y `ESP32_controller/Constants.h` es la fuente de verdad.

---

## Convenciones

**`Constants.h` es la fuente de verdad del hardware y de los límites de validación.** Pines, canales PWM, longitudes de campo y `validFrequencies` se definen ahí. La documentación los refleja; si divergen, el código gana.

**El portal tiene dos archivos y una dirección.** `HTML/mainForm.html` es la fuente legible y comentada; `ESP32_controller/mainForm.h` es el artefacto que sirve el dispositivo, generado a partir del anterior quitando comentarios y envolviéndolo en un raw string de C. **Se edita el `.html` y se regenera el `.h`**, nunca al revés.

**Las validaciones están duplicadas a propósito.** El navegador y el firmware aplican las mismas reglas (charset, longitudes, repetidos) contando por carácter UTF-8. Si cambias una, cambia la otra: `HTML/mainForm.html` y `Plant.cpp` / `utils.cpp`.

**Las imágenes viven en [`img/`](img).** Referéncialas con rutas relativas (`./img/archivo.png`) para que funcionen tanto en GitHub como en visores locales de Markdown.

---

## Estado del proyecto

| Área | Estado |
|---|---|
| Firmware ESP32-C3 | Funcional: portal captivo, scheduling, RTC con modo seguro, sesión, Wi-Fi AP+STA con escaneo asíncrono |
| Portal web embebido | Funcional: dashboard, edición en vivo, configuración de red |
| Hardware | Prototipo; PCB de la versión ESP32 en [`ESP32_Board/`](../ESP32_Board) (KiCad) |
| Seguridad del portal | Los tres huecos resueltos: contraseña con PBKDF2, reset restringido al AP y límite de intentos de login — [ARCHITECTURE.md](ARCHITECTURE.md#huecos-de-seguridad-identificados-y-su-resolución) |
| OTA local | Cliente listo (Portal V2); falta el endpoint y **fijar `partitions.csv`** con dos slots — [ARCHITECTURE.md](ARCHITECTURE.md#requisito-crítico-tabla-de-particiones-con-dos-slots-ota) |
| Portal gzip | Propuesta con flujo definido; sin implementar — [ARCHITECTURE.md](ARCHITECTURE.md) |
| Backend | Auth + provisión + config/telemetría + lecturas app-facing **implementados y verificados** end-to-end; faltan OTA/admin y MQTT — [BACKEND.md](BACKEND.md) / [API-backend.md](API-backend.md) |
| OTA segura | No iniciada; estrategia analizada en [ARCHITECTURE.md](ARCHITECTURE.md) |
