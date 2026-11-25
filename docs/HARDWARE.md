# Hardware

Tarjeta de control, mapa de pines, periféricos y instalación eléctrica.

El controlador es un **ESP32-C3** (RISC-V, 1 núcleo @160 MHz) con Wi-Fi nativo, que sirve el portal captivo desde su propia flash. Hoy se usa una tarjeta **ESP32-C3 Super Mini**; más adelante se migrará al módulo **WROOM**. En la etapa de potencia, un MOSFET conmuta la bobina en CD de cada contactor, y los contactos del contactor conmutan la carga en CA.

---

## Contenido

- [Mapa de pines (ESP32-C3)](#mapa-de-pines-esp32-c3)
- [Control de potencia PWM](#control-de-potencia-pwm)
- [Temporización con RTC](#temporización-con-rtc)
- [Etapa de potencia: acoplamiento control ↔ potencia](#etapa-de-potencia-acoplamiento-control--potencia)
- [Conectividad](#conectividad)
- [Instalación eléctrica](#instalación-eléctrica)
- [Medición de temperatura (DS18B20)](#medición-de-temperatura-ds18b20)

---

## Mapa de pines (ESP32-C3)

Definido en `ESP32_controller/Constants.h`.

| Componente | GPIO | Tipo | Notas |
|---|---|---|---|
| Lámpara | 0 | Digital | Control principal de iluminación, ON/OFF |
| LED azul | 1 | PWM canal 1 | 0–100 % desde el portal |
| LED rojo | 2 | PWM canal 2 | 0–100 % desde el portal |
| Buzzer | 3 | Digital | Señalización sonora no bloqueante. Ver [vocabulario de pitidos](#vocabulario-de-pitidos) |
| Ventilador / extractor | 7 | Contactor | ON/OFF |
| Bomba de agua | 10 | Contactor | ON/OFF |
| I²C SDA | 8 | I²C | Datos del bus I²C (DS3231) |
| I²C SCL | 9 | I²C | Reloj del bus I²C (DS3231) |
| RTC DS3231 | I²C `0x68` | — | Reloj externo para el scheduling (bus I²C en GPIO 8/9) |

<div align="center">
  <img src="./img/esp32-c3_connection.png" alt="Conexión de pines del ESP32-C3 Super Mini"/>
</div>

**Polaridad centralizada:** el nivel que enciende vive en **un solo sitio**, las constantes `deviceOn`/`deviceOff` de `Constants.h`, hoy `LOW`/`HIGH` (**activo en bajo**: el pin en bajo enciende). Antes eran `HIGH`/`LOW` (lógica directa); el cambio llegó con la versión 1.0.1 del firmware Arduino. Si el hardware fuese activo en alto, basta intercambiar esas dos líneas y todo el firmware queda coherente. Las usan las cuatro salidas ON/OFF (lámpara, bomba, ventilador **y buzzer**); los canales PWM azul y rojo no dependen de ellas. Detalle en [Polaridad de las salidas ON/OFF](#polaridad-de-las-salidas-onoff-activa-en-bajo).

**Estado inicial seguro:** `Plant::begin()` llama a `allDevicesOff()` antes de leer cualquier configuración, para que ningún actuador arranque energizado. Con la polaridad activa en bajo, "apagado" es nivel **alto**. Durante el reset y el arranque del chip los GPIO están en alta impedancia y los circuitos están pensados para quedar apagados en ese estado (ver [polaridad](#polaridad-de-las-salidas-onoff-activa-en-bajo)). Lo que el firmware no garantiza es el instante dentro de `begin()` entre el `pinMode(OUTPUT)` de un pin y su primera escritura de `deviceOff`: el registro de salida arranca en 0 (nivel de "encendido"), así que el pin puede quedar brevemente en bajo; no se ha medido.

**La inicialización de hardware NO va en el constructor:** `Plant planta;` es un objeto global, así que su constructor corre durante la inicialización estática de C++, antes de que el framework de Arduino termine de preparar los periféricos. Configurar GPIO o LEDC ahí puede fallar en silencio o quedar sobrescrito, dejando un pin como entrada flotante en lugar de salida firme; el síntoma típico es un relé o LED con **brillo débil** que no conmuta bien. Por eso `pinMode`, `ledcAttachChannel` y el estado inicial se hacen en `begin()`, invocado desde `setup()`.

**Un solo canal digital para la lámpara:** la lámpara no tiene PWM asignado, así que solo admite ON/OFF; por eso sirve con lámparas de cultivo comerciales que se manejen con un control ON/OFF simple. Los canales PWM azul y rojo son opcionales, para quien quiera controlar esos espectros con más precisión. El portal envía `0` o `1` y el firmware evalúa `> 0`. Los espectros azul y rojo sí son regulables, que es donde la granularidad importa para el PAR.

### Polaridad de las salidas ON/OFF (activa en bajo)

Desde la versión 1.0.1 del firmware, las cuatro salidas ON/OFF (lámpara, bomba, ventilador y
buzzer) son **activas en bajo**: el GPIO en **bajo** (`deviceOn = LOW`) enciende y en **alto**
apaga. Los canales PWM azul y rojo no dependen de esto (duty alto = más luz).

| Salida | Circuito | Estado |
|---|---|---|
| Lámpara, bomba y ventilador (GPIO 0, 10, 7) — lado de control | Opto PC817 y LED indicador en **sumidero**: `+3.3 V → ánodo`, `cátodo → R → GPIO` | **Verificado en la placa**: el LED indicador enciende con el GPIO en bajo |
| Lámpara, bomba y ventilador — lado de potencia | Fototransistor como seguidor de emisor + Zener de 10 V + AO3400A (ver [esquemático](#esquemático--salida-digital-opto--mosfet--contactor)) | **Diseño, aún sin montar** |
| Buzzer (GPIO 3) | TMB12A05 con dos AO3400A (inversor + interruptor) | **Diseño recomendado, aún sin montar** (ver [esquemático](#esquemático--buzzer-gpio-3)) |

Con el GPIO en alta impedancia (reset y arranque), las salidas con opto quedan apagadas solas:
sin camino de retorno no circula corriente por el LED del opto, así que no hace falta
pull-down.

---

## Control de potencia PWM

La modulación por ancho de pulso se usa aquí para regular la intensidad de cada canal LED de forma independiente, permitiendo adaptar el espectro a la etapa del cultivo — más azul en vegetativo, más rojo en floración. Es una función **opcional**, pensada para usuarios con más experiencia: la iluminación principal es la lámpara ON/OFF (GPIO 0), que no usa PWM.

| Parámetro | Valor |
|---|---|
| Frecuencia | 1 kHz |
| Resolución | 8 bits (duty 0–255) |
| Canales | 2 (azul, rojo) |

Los valores viajan del portal al firmware como **porcentaje 0–100** y se escalan internamente con `map(valor, 0, 100, 0, 255)`. Se eligió exponer porcentaje en la API para que el contrato no dependa de la resolución del PWM: si se sube a 10 bits, el frontend no cambia.

---

## Temporización con RTC

Para temporizar el encendido y apagado de los equipos se usa un **DS3231** por I²C (dirección `0x68`), compensado por temperatura y de baja deriva. Va montado directamente en la tarjeta como IC de 8 pines (DS3231M), no como módulo.

El firmware lee el RTC una vez por segundo desde `loop()` (único punto de I²C periódico, para no compartir el bus `Wire` con otras tareas) y con esa lectura decide qué salidas deben estar activas.

La hora **no** se configura con botones: se sincroniza desde el navegador del usuario en el `POST /newparams`, que incluye fecha y hora completas. El primer `/newparams` con fecha válida ancla `cropStart` en NVS, y a partir de ahí la edad del cultivo se **deriva** del calendario en vez de contarse, de modo que el cultivo sigue envejeciendo aunque el equipo estuviera apagado.

<table align="center">
  <tr>
    <th>Diagrama típico de conexión.</th>
    <th>Diagrama de conexión final.</th>
  </tr>
  <tr>
    <th><img src="./img/ds3231_wiri.png" alt="Diagrama típico RTC"/></th>
    <th><img src="./img/ds3231_conecction.png" alt="Diagrama de conexión final del RTC"/></th>
  </tr>
</table>

En el diagrama de conexión final, el DS3231 se alimenta a 3.3 V, SDA y SCL llevan pull-ups de 10 kΩ a 3.3 V y el pin VBAT toma una pila CR2032 que mantiene la hora sin alimentación.

---

## Etapa de potencia: acoplamiento control ↔ potencia

Cómo se acopla la lógica del ESP32-C3 (3.3 V) con la potencia. Esta sección recoge las
decisiones de rediseño que sustituyen al SSR AQH2213 de la primera iteración.

### Decisiones tomadas

| Tema | Decisión | Por qué |
|---|---|---|
| **Fuente única** | **24 VCD** | A 120 W, 24 V tira ~5 A vs ~10 A a 12 V → mitad de corriente, ¼ de pérdidas (I²R), pistas y conectores más chicos, menos calor. A >60 W las fuentes de 24 V son tan comunes como las de 12 V. Una sola fuente alimenta LEDs, bobinas de contactor y (vía buck) el MCU. |
| **Dimensionado LED (canales PWM, opcional)** | Referencia de diseño: 60–120 W para 1–2 m², relación **5:1 azul:rojo, con azul predominante** | Azul ≈ 100 W (~4.2 A @ 24 V, canal fuerte); rojo ≈ 20 W (~0.85 A). Los canales se dimensionan al **peor caso: cada canal al 100 %**. Es solo una referencia: usar los canales PWM, y con qué potencia de LED, queda **a criterio del usuario**; la iluminación principal es la lámpara ON/OFF (GPIO 0). |
| **Salidas digitales (vent/bomba/lámpara)** | **Opto PC817 en sumidero (activo en bajo) + MOSFET lógico** acciona un **relé/contactor con bobina CD (5/12/24 V, a elección del usuario)**; el fototransistor trabaja como **seguidor de emisor** (no invierte) y un **Zener de 10 V** protege el gate; **flyback** en la bobina y **snubber RC** en los contactos de CA | Aislamiento galvánico entre control (3.3 V) y potencia: el opto separa `GND_control` de `GND_potencia` (jumper opcional para fuente única). Mismo circuito para los tres voltajes de bobina. |
| **PWM LED (azul, rojo)** | **MOSFET lógico N-channel directo** (sin opto) | A 1 kHz un optoacoplador común (PC817) deforma la señal. El MOSFET lógico conmuta directo desde 3.3 V. |
| **MCU 3.3 V** | **Buck 24→5 V (→3.3 V)**, no lineal | Un lineal 24→3.3 disiparía demasiado; se usa un convertidor conmutado (p. ej. MP1584). |

### Pendiente de definir

- **Tipo de arreglo LED:** *voltaje constante (24 V)* vs *driver de corriente constante*.
  Decide la topología del canal PWM (ver abajo). **Mientras no se defina, el MOSFET exacto
  del canal azul queda tentativo.**
- **Bobina del contactor:** confirmar que se consiguen contactores con **bobina 24 VCD** y su
  consumo (corriente de retención y pico de arranque / inrush).

### Componentes propuestos (todos comunes / JLCPCB Basic)

| Función | Componente | Notas |
|---|---|---|
| MOSFET canal PWM (azul/rojo) | **IRLZ44N** (TO-220) o **AOD4184** (DPAK, SMD) | Logic-level, margen de V y RDS(on) para el canal azul ~4.2 A @ 24 V. El AO3400A (30 V, ~5.7 A) se queda **justo** a 24 V y a 3.3 V de gate su RDS(on) sube: solo para cargas pequeñas. |
| MOSFET bobina de contactor | **AO3400A** (SOT-23) | La bobina tira poco (<0.2 A típico); aquí el AO3400A sobra. Lo excita el opto como seguidor de emisor. Su Vgs máximo es **±12 V**, por eso el Zener del gate. Con bobina de 5 V el gate queda en ≈ 4.4 V, donde su RDS(on) está especificada. |
| Optoacoplador (salidas digitales) | **PC817** (DIP-4 / SOP-4) | Aísla el GPIO (3.3 V) del dominio de potencia (bobina 5/12/24 VCD). **Activo en bajo (sumidero)**: `+3.3 V → ánodo`, `cátodo → R 330Ω → GPIO`; sin pull-down (con el GPIO en alta impedancia, en reset, el opto no conduce: apagado seguro). Salida: fototransistor como **seguidor de emisor** (`colector → +V_bobina`, `emisor → 10 kΩ → gate`). **Tierras separadas** (jumper opcional para fuente única sin aislar). Corriente del LED ≈ 6 mA (`(3.3 − 1.2) / 330`); el PC817 admite 35 V (VCEO), con margen a 24 V. |
| Diodo flyback (bobina) | **1N4007** | **Obligatorio** en antiparalelo con la bobina CD: absorbe el pico inductivo (V=−L·di/dt) al cortar. Igual para 5/12/24 V. |
| Snubber RC (contactos CA) | **100 Ω ≥2 W + 0.1 µF X2 275 VCA** (en serie) | En paralelo con los contactos del relé/contactor / la carga. Protege los contactos del arco al abrir un motor inductivo (≤1 HP, 120 VCA) y reduce EMI. |
| Resistencia serie del gate (lado potencia) | **10 kΩ** emisor→gate | Limita la corriente hacia el Zener (~1.4 mA a 24 V, dentro de lo que entrega el fototransistor) y, con el pull-down, fija ≈ 4.4 V de gate con bobina de 5 V. |
| Pull-down del gate (lado potencia) | **100 kΩ** gate→`GND_potencia` | Descarga el gate y mantiene el MOSFET apagado cuando el opto no conduce. |
| Zener de protección del gate | **MM3Z10VT1G** (10 V) | Cátodo al gate, ánodo a `GND_potencia`. Limita Vgs a ~10 V con bobinas de 12/24 V (AO3400A: máx. ±12 V). A 5 V no actúa. |
| Convertidor MCU | Buck **24 V → 5 V** (MP1584 o equiv.) | De ahí al regulador 3.3 V. |
| Fuente | **24 VCD, 150 W** (o 100 W si tope 60 W) | ~125 % de la carga máxima. |
| LED indicador de salida | **LED + 680 Ω** | Uno por salida digital, en el **lado de control**, en sumidero igual que el opto (`+3.3 V → 680 Ω → LED → GPIO`: enciende con el GPIO en bajo, ~2–3 mA @ 3.3 V; **verificado en la placa**). R fija: **independiente del voltaje de bobina** (5/12/24 V). Indica lo que pide el firmware, no que la bobina reciba corriente. Opcional: LED "+Vb" en el lado de potencia (680 Ω / 2.2 kΩ / 4.7 kΩ para 5/12/24 V). |
| LED indicador de sistema | **LED + 680 Ω** | "24 V presente" en la fuente y, opcional, "3.3 V OK" tras el buck (~2 mA @ 3.3–5 V). |
| Buzzer | **TMB12A05** (activo, 5 V, 12 mm, DIP) | 2300 ± 300 Hz, ≤ 30 mA, ≥ 85 dB a 10 cm. Alimentado a 5 V, conmutado por **dos AO3400A** (inversor + interruptor, activo en bajo; mismo MOSFET que las bobinas) + flyback 1N4007, desde GPIO 3 (ver esquemático). Control no bloqueante (`buzzerBeep`/`buzzerUpdate`). |

### Esquemático — canal PWM de LED (voltaje constante)

<!-- TODO: reemplazar el diagrama ASCII de abajo por la captura del esquemático real.
     ![Canal PWM de LED](./img/pwm_led_sch.png)
     NOTA: sección aún no cerrada — el usuario podrá conectar sus propios LEDs/arreglos
     (punto de conexión flexible, como las bobinas). Capturar cuando se fijen límites
     de voltaje/corriente del canal. -->

Aplica si las tiras/arreglos son de **voltaje constante a 24 V**. Un canal por color
(GPIO 1 = azul, GPIO 2 = rojo):

```
            +24 V
              │
          [ ARREGLO LED ]        (azul o rojo)
              │
              │  Drain
 GPIO ──[100Ω]──┤ IRLZ44N / AOD4184 (N-ch, logic-level)
   │          │  Source
 [100k]       │
   │         GND
  GND
```

- `GPIO` = pin PWM (1 kHz, 8 bits). `100Ω` protege el pin; `100k` a GND mantiene el MOSFET
  apagado en el arranque.
- Si el arreglo lleva **driver de corriente constante**, el PWM NO va a este MOSFET: va a la
  **entrada de dimming (PWM/0–10 V)** del driver. Topología distinta — a definir.
- **Sin LED indicador dedicado** en los canales PWM a propósito: el propio arreglo LED ya es el
  indicador visual, y un LED colgado del PWM parpadearía a 1 kHz (brillo según el duty),
  confuso como indicador de estado.

> **Nota de pines (strapping):** en el ESP32-C3 los pines de *strapping* de boot son
> **GPIO 2, 8 y 9**, y el diseño usa los tres: el **PWM rojo (GPIO 2)** y el **bus I²C del
> DS3231 (SDA GPIO 8 / SCL GPIO 9)**.
>
> - **GPIO 9** decide el arranque: en alto al reiniciar, el chip ejecuta el programa; en bajo,
>   entra al bootloader serie y no arranca la aplicación. Tiene un pull-up interno (~45 kΩ), y
>   los pull-ups de 10 kΩ del bus I²C a 3.3 V también lo mantienen en alto. Hay que comprobar
>   que nada del bus lo fuerce a bajo durante el reset.
> - **GPIO 8** solo importa para entrar al bootloader: en arranque normal (GPIO 9 en alto)
>   se ignora. La combinación GPIO 8 = 0 y GPIO 9 = 0 es inválida.
> - **GPIO 2**: un pull-down de 100 kΩ (alta impedancia) no suele afectar el arranque, pero
>   conviene verificarlo (ver "Strapping Pins" en el datasheet del ESP32-C3).
>
> Si alguno da problemas de arranque, reubicar esa señal a un GPIO no-strapping. Esto **no se
> ha verificado en la placa**; la descripción de GPIO 8/9 sale de la documentación de
> Espressif (boot mode selection).

### Esquemático — salida digital (opto + MOSFET + contactor)

<div align="center">
  <img src="./img/relay_output.png" alt="Esquemático de una salida digital: opto PC817, MOSFET AO3400A y bobina de relé"/>
</div>

> Salida **activa en bajo** (opto en sumidero), verificada en la placa. Ver [Polaridad de las salidas ON/OFF](#polaridad-de-las-salidas-onoff-activa-en-bajo).

Aplica a ventilador, bomba y lámpara (GPIO 7, 10, 0). El opto separa el dominio de control
(3.3 V, `GND_control`) del de potencia (`GND_potencia`). Con `GPIO = 0` circula corriente por el
LED del opto (pines 1-2) y por el LED indicador. El fototransistor (pines 3-4) trabaja como
**seguidor de emisor**: sube el gate del MOSFET a través de 10 kΩ y la bobina se energiza. Con
`GPIO = 1`, el pull-down de 100 kΩ descarga el gate y la bobina se apaga. La señal no se invierte.
**Estado:** lado de control verificado en la placa; lado de potencia es diseño, aún sin montar.

- **Bobina de 5, 12 o 24 VCD:** el circuito es idéntico; solo cambia el `+V_bobina` del borne
  de potencia. El relé/contactor lo elige el usuario. Si conmuta un motor de CA (p. ej. 1 HP a
  120 VCA), sus contactos deben dimensionarse para ese inrush (AC-3).
- **Tierras separadas:** por defecto `GND_control` y `GND_potencia` no se unen, y el aislamiento
  es real. Un jumper opcional permite usar una sola fuente (p. ej. 5 V para control y bobinas),
  pero al cerrarlo se pierde el aislamiento galvánico.

### Esquemático — buzzer (GPIO 3)

Buzzer **TMB12A05**: activo (oscilador integrado, suena con solo aplicarle DC), 5 V nominal,
≤ 30 mA, 2300 ± 300 Hz, ≥ 85 dB a 10 cm. Control ON/OFF desde el GPIO 3.

> **Activo en bajo** (GPIO en bajo → suena), igual que las demás salidas ON/OFF.
> **Diseño recomendado, aún sin montar.** Ver [Polaridad de las salidas ON/OFF](#polaridad-de-las-salidas-onoff-activa-en-bajo).

<div align="center">
  <img src="./img/buzzer_output.png" alt="Esquemático del buzzer TMB12A05 con dos AO3400A (inversor e interruptor)"/>
</div>

El GPIO 3 maneja a Q4 (inversor) a través de R16 (100 Ω), y Q4 gobierna a Q5, que conmuta el
lado bajo del buzzer. R17 (100 kΩ a +3.3 V) mantiene a Q4 conduciendo, y al buzzer callado,
mientras el GPIO está en alta impedancia durante el reset y el arranque. D7 (1N4007, igual que
en las salidas a relevador) absorbe el pico inductivo al apagar.

- **Dos etapas:** con un solo MOSFET N el buzzer sería activo en alto, y un P-MOSFET o PNP de
  lado alto no se apaga de forma fiable con un GPIO de 3.3 V contra 5 V (Vgs ≈ −1.7 V).
- **Alimentación a 5 V:** a 3.3 V queda por debajo del rango publicado (4–7 V). Va sin opto
  porque es parte del circuito de control.
- **Al energizar:** mientras el riel de 3.3 V no sube, el buzzer podría sonar unos milisegundos.
  No se ha medido.
- **Firmware:** ver el [vocabulario de pitidos](#vocabulario-de-pitidos).

#### Vocabulario de pitidos

El sonido comunica **qué** pasó, no solo "algo pasó". Los patrones se eligieron para que un
error no se confunda con una confirmación. Implementado con la máquina no bloqueante
(`buzzerBeep`/`buzzerUpdate`), salvo los eventos que reinician el equipo o son caminos de
error, que suenan síncronos en su handler (ver `ESP32_controller.ino`). Conteos en
`Constants.h` (`buzzerBeeps*`).

| Evento | Patrón | Disparo | Bloqueante |
|---|---|---|---|
| **Sistema listo** (boot) | 1 corto | fin de `setup()` | No |
| **Arranque sin RTC fiable** | 4 cortos | fin de `setup()` si `!isRtcValid()` | No |
| **Parámetros aplicados** | 3 cortos (330/330) | `/newparams` → `STATUS_OK` | No |
| **Login fallido / bloqueado** | 2 cortos | `MISMATCH_CREDENTIALS` / `TOO_MANY_ATTEMPTS` | No |
| **OTA correcta** | 1 largo (~600 ms) | `/otaupdate` OK, antes del reinicio | Sí (reinicia) |
| **OTA fallida** | 2 cortos | error 401/403/500 de `/otaupdate` | Sí (breve) |
| **Reset de fábrica** | 1 largo (~600 ms) | `HARD_RESET`, antes del reinicio | Sí (reinicia) |

**Deliberadamente SIN sonido:** las conmutaciones rutinarias de riego/luz/ventilación (muy
frecuentes, serían molestas) y el polling de Wi-Fi. El inicio de OTA tampoco pita, para no
interferir con la escritura del primer bloque en flash. El buzzer se reserva para **eventos de
usuario y de seguridad**.

### Diagrama de bloques

```
                         Fuente 24 VCD  ──[680Ω]──▶|  LED "24 V"
          ┌──────────────────┼───────────────────────────┐
          │                  │                            │
      buck 24→5→3.3     arreglos LED (24 V)        bobinas relé/contactor
          │             azul / rojo                  (5/12/24 VCD, +Vb externo)
       ESP32-C3            ▲   ▲                         ▲   ▲   ▲
          │  PWM 1 kHz ────┘   │     GPIO 7,10,0 ─[opto]──┘   │   │  cada salida:
          │  (GPIO 1,2)        │       │  (aísla tierras)     │   │  flyback ∥ bobina
          └── MOSFET directo ──┘       └── MOSFET (GND_potencia) ─┘  LED ind. en control
                                                   │
                                   contactos CA → motores ≤1 HP (120 VCA) + snubber RC

  GND_control (3.3 V)  ⟂  GND_potencia (+Vb)   — separadas (jumper opcional para unir)
  PWM de LED: MOSFET directo (sin opto, 1 kHz).  Digitales: vía opto (aisladas).
  LED indicador de cada salida digital: en el lado de control (R fija, independiente de Vb).
```

Los canales PWM no llevan LED indicador dedicado (el arreglo LED ya es su indicador).

---

## Conectividad

El ESP32-C3 aprovecha su Wi-Fi integrado en **modo AP+STA**:

- **AP** (`SmartPlant`, WPA2-PSK): sirve el portal captivo. Siempre arriba.
- **STA**: se conecta a la red del usuario, solo si hay credenciales guardadas. Habilita telemetría y OTA a futuro.

Hay una sola radio, así que AP y STA comparten canal. Dos detalles que costaron depuración y quedan documentados en el firmware:

1. El modo dual se fija **explícitamente** con `WiFi.mode(WIFI_AP_STA)` antes de crear el SoftAP, para que `WiFi.begin()` del STA no reconfigure el modo por debajo.
2. Hay que **desactivar el modem-sleep** con `WiFi.setSleep(false)`. Con el STA habilitado, el ESP32 duerme la radio según el ciclo del STA (`WIFI_PS_MIN_MODEM` por defecto), lo que deja al SoftAP sin responder y el portal no carga.

<div align="center"><img src="./img/wirelessI.png" alt="Interfaz inalámbrica"/></div>

---

## Instalación eléctrica

Instalación propuesta para la conexión de los actuadores al gabinete de control. Las salidas del microcontrolador no accionan las cargas directamente: pasan por la [etapa de potencia](#etapa-de-potencia-acoplamiento-control--potencia) (MOSFET lógico) y de ahí a las bobinas de los contactores, que son los que conmutan la potencia en CA.

<div align="center"><img src="./img/gabinete1.jpg" alt="Gabinete eléctrico" width="425" height="516"/></div>

La selección de contactores, los diagramas eléctricos y la conexión final se documentarán conforme avance la implementación en hardware.

---

## Medición de temperatura (DS18B20)

**Planeada, aún no implementada** en el firmware. Medir la temperatura del área de cultivo es una labor preventiva: conocer su comportamiento dentro de ciertos rangos ayuda a anticipar problemas. Queda para una futura actualización que además pueda ejecutar acciones correctivas.

Los archivos de diseño de la tarjeta están en [`ESP32_Board/`](../ESP32_Board) (KiCad).
