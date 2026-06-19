# SmartPlant — Controlador ESP32 (firmware + portal captivo)

Firmware para un **ESP32-C3** que controla un cultivo (iluminación LED por espectro,
riego y ventilación) y se configura desde el celular/PC vía un **portal captivo
Wi-Fi**. El dispositivo levanta un Access Point, sirve un formulario HTML embebido y
recibe la configuración por HTTP en JSON.

Este directorio es la variante **"HTML sin comprimir"**: contiene tanto el firmware
(`ESP32_Controller/`) como una copia legible de los formularios (`HTML/`) para editar
cómodamente antes de embeberlos.

---

## Estructura

```
ESP32_Controller/
  CaptiveCrop.ino     # Punto de entrada: AP Wi-Fi, DNS captivo, rutas HTTP y handlers
  Plant.h / Plant.cpp # Lógica del cultivo: validación de payloads, persistencia, PWM/relés, RTC
  Constants.h         # Pines, canales PWM, límites de validación, enums de estado y de error
  dashboardForm.h     # *** El formulario HTML embebido (string C R"===(...)===") que sirve el ESP32 ***
  ci.json             # Config de build (FQBN / placa)
HTML/
  params.html         # Copia legible (sin el wrapper C) de dashboardForm.h — solo referencia
```

### Convención importante (leer antes de tocar formularios)
- **El dispositivo solo sirve los `.h`.** Cualquier cambio de UI debe hacerse en
  `ESP32_Controller/dashboardForm.h`. La carpeta `HTML/` es una copia de referencia.
- `HTML/params.html` se genera a partir de `dashboardForm.h` quitando la primera línea
  (`static const char dashboardForm[] = R"===(`) y la última (`)===";`). Manténlas en
  sintonía si editas el `.h`.

---

## Hardware (ESP32-C3, ver `Constants.h`)
- LED blanco → GPIO 0 (canal PWM 0), LED azul → GPIO 1 (canal 1), LED rojo → GPIO 2 (canal 2)
- Buzzer → GPIO 3, ventilador → GPIO 7, bomba de agua → GPIO 10
- PWM: 1 kHz, 8 bits (duty 0–255). Los espectros se envían como 0–100 % y se escalan internamente.
- RTC externo **DS3231** por I²C (dirección `0x68`); la hora se sincroniza desde el navegador en `/newparams`.

---

## Arranque y red
1. `planta.begin()` carga estado y credenciales desde **Preferences** (NVS), namespaces `config` y `system`.
2. Se crea el AP Wi-Fi **`SmartPlant`** con DHCP + portal captivo.
3. `DNSServer` responde la IP del AP a todo dominio (detección de portal captivo).
4. El `WebServer` (puerto 80) registra las rutas y entra al loop `handleClient()`.
5. Cualquier ruta desconocida hace `302` a `/` (`handleNotFound`).

---

## Flujo de la aplicación (página única)

Todo vive en **un solo HTML** (`dashboardForm.h`). No hay página de registro aparte;
el formulario es una máquina de estados que decide qué mostrar según el dispositivo.

Al cargar, el cliente hace `GET /getparams`. La respuesta incluye `hasRegisteredUser`:

- **`hasRegisteredUser = false`** (dispositivo nuevo) → estado **`register`**: se reusa
  el bloque de credenciales como alta. `POST /usercredentials` guarda usuario+contraseña
  (sin reiniciar) y se pasa directo al dashboard.
- **`hasRegisteredUser = true`** → estado **`view`**: dashboard de solo lectura. El botón
  "Editar parámetros" lleva al estado **`auth`** (login → `POST /authusercredentials`), y
  si las credenciales son válidas, al estado **`edit`** (formulario editable →
  `POST /newparams`).

Estados del front: `register` · `view` · `auth` · `edit`.

### Reset de fábrica
En la pantalla de **login** (`auth`), escribir como contraseña la palabra mágica
**`**reset**`** dispara `hardReset()` en el backend (borra los namespaces `config` y
`system` de Preferences) y reinicia el ESP32. Se intercepta en `authUserCredentials()`
**antes** de comparar credenciales, así que funciona aunque se haya olvidado la
contraseña. El registro **no** acepta reset (no hay nada previo que borrar).

---

## Endpoints HTTP
Ver el detalle de payloads en **`rutas_y_parametros.txt`**.

| Método | Ruta                  | Handler                  | Propósito |
|--------|-----------------------|--------------------------|-----------|
| GET    | `/`                   | `handleRoot`             | Sirve el HTML único (`dashboardForm`) |
| GET    | `/getparams`          | `handleGetParameters`    | Estado del dispositivo en JSON (incluye `hasRegisteredUser`) |
| POST   | `/usercredentials`    | `handleUserCredentials`  | Alta de usuario (primer arranque). No reinicia |
| POST   | `/authusercredentials`| `handleAuthUserCredentials` | Login para desbloquear edición; intercepta `**reset**` |
| POST   | `/newparams`          | `handleNewParameters`    | Guarda parámetros del cultivo + hora; reinicia al guardar OK |
| GET    | `/exit`               | `handleExit`             | Pantalla "Desconectado correctamente" |
| *      | (cualquier otra)      | `handleNotFound`         | `302 → /` (portal captivo) |

Respuestas: siempre JSON `{"status": <bool>, "message": "<texto>"}`. `200` en éxito,
`400` en error de validación. El mapeo estado→mensaje está en `buildHttpResponse()`
(`Plant.cpp`).

---

## Reglas de validación (deben coincidir front ↔ firmware)
Definidas en `Constants.h` y replicadas en JS dentro de `dashboardForm.h`
(`isValidReadableString`, `hasTooManyRepeatedChars`, etc.).

- **Usuario:** 4–32 caracteres. **Contraseña:** 8–64. **Nombre de planta:** 3–20.
- **Charset permitido:** letras (incl. acentos y ñ/Ñ), dígitos y `_-.@!#$%&*?+=`.
  El nombre de planta admite espacios; usuario/contraseña no.
- No se permiten **4+ caracteres idénticos consecutivos**.
- Nombre de planta: sin espacios dobles y no puede ser solo dígitos.
- Longitudes contadas por **carácter UTF-8** (`utf8Len`), no por bytes.

---

## Compilar y flashear
- IDE: Arduino IDE / arduino-cli. Placa: ESP32-C3 (ver `ci.json`).
- Dependencias: core **ESP32 (WiFi/WebServer/DNSServer)**, **ArduinoJson** (v6, API
  `StaticJsonDocument`), `Wire`, `Preferences`.
- Tras flashear, conectarse al Wi-Fi `SmartPlant`; el portal captivo abre el formulario.
- **No hay test runner.** La validación se hace en hardware / a mano.

---

## Frecuencias de riego/ventilación (`irrH` / `ventH`)
Los `<select>` envían la **frecuencia real en veces/día**, no un índice. Los valores
ofrecidos replican `validFrequencies` de `Constants.h` (`{0,1,2,3,4,6,8,12,24}`, todos
divisores de 24). El front se alinea al firmware: la constante JS `VALID_FREQUENCIES` en
`dashboardForm.h` debe mantenerse igual a `validFrequencies` si esta cambia.

## Propuesta: servir el HTML comprimido (gzip) para mayor performance

Hoy el HTML se embebe **en crudo** (`dashboardForm.h`, ~60 KB) y se sirve como texto.
Comprimirlo con gzip suele reducirlo a ~12–18 KB, lo que significa **menos flash
ocupado**, menos chunks por el AP y carga más rápida del portal. Los navegadores
descomprimen gzip de forma transparente; solo hay que declarar el encabezado
`Content-Encoding: gzip`. Esto está **pendiente de implementar** (este directorio es la
variante "sin comprimir"); se documenta aquí la vía recomendada.

### Flujo propuesto
1. **Editar siempre el HTML sin comprimir** (`dashboardForm.h` / `HTML/params.html`) como
   fuente de verdad. El `.gz` es un artefacto generado, nunca se edita a mano.
2. **Comprimir** el HTML:
   ```bash
   gzip -9 -c HTML/params.html > params.html.gz
   ```
3. **Convertir a arreglo de bytes** en un header (PROGMEM):
   ```bash
   xxd -i params.html.gz > ESP32_Controller/dashboardForm_gz.h
   ```
   Genera algo como `unsigned char params_html_gz[] = {...};` y
   `unsigned int params_html_gz_len = NNNN;`. Conviene marcarlo `PROGMEM` y, si se quiere,
   renombrar el símbolo a `dashboardForm_gz`.
4. **Servir con el encabezado de codificación** en `handleRoot` (`CaptiveCrop.ino`).
   Como es binario (no una cadena terminada en nulo), debe enviarse con longitud explícita
   vía `send_P`:
   ```cpp
   void handleRoot() {
     server.sendHeader("Content-Encoding", "gzip");
     server.send_P(200, "text/html", (const char*)dashboardForm_gz, dashboardForm_gz_len);
   }
   ```

### Consideraciones
- **Regenerar el `.gz` en cada cambio de UI**, idealmente como paso de build (script o
  target), para que no quede desincronizado del HTML fuente. Es el mismo riesgo de
  sincronía que ya existe entre `dashboardForm.h` y `HTML/params.html`.
- Mantener `dashboardForm.h` (crudo) o sustituirlo por el `_gz.h` es decisión de tamaño vs.
  conveniencia de depurar; lo habitual es **dejar solo el `.gz` en producción** y conservar
  el crudo para desarrollo.
- No afecta a las rutas POST (JSON) ni a `/getparams`: solo cambia cómo se entrega el HTML.
- `send_P` requiere longitud explícita porque el contenido gzip contiene bytes nulos.

## Caveats conocidos
- **`StaticJsonDocument` ajustado.** `/newparams` recibe ~20 claves; el buffer se subió a
  1024 B para evitar `NoMemory` con credenciales/planta largas.
- `params.html` (en `HTML/`) es solo referencia; la fuente de verdad es `dashboardForm.h`.
