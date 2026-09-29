#!/usr/bin/env python3
"""Genera HTML/mainForm.preview.html desde HTML/mainForm.html.

El preview es el MISMO portal con un mock de `fetch` inyectado: responde los
endpoints con datos simulados y no valida token, así que el portal funciona sin
dispositivo. Se GENERA en vez de mantenerse a mano por el mismo motivo que
`mainForm.h`: para que no pueda desincronizarse del fuente. Nunca se edita a mano.

El escenario simulado se controla con las banderas del bloque MOCK del archivo
generado (o de este script, si el cambio debe persistir entre regeneraciones).

Uso, desde la raíz del proyecto:
    python3 scripts/gen_preview.py
"""
import sys
from pathlib import Path

SRC = Path("HTML/mainForm.html")
DST = Path("HTML/mainForm.preview.html")

# El mock se inyecta ANTES de </head> para que window.fetch quede parcheado antes
# de que corra el <script> del portal (que vive en el <body>).
MOCK = r"""
    <!-- ==================================================================
         MOCK DE PREVIEW  ·  ARCHIVO GENERADO, NO EDITAR A MANO
         Generado por scripts/gen_preview.py desde HTML/mainForm.html.
         Intercepta fetch() para que el portal funcione sin dispositivo:
         no se valida ningún token y los endpoints devuelven datos simulados.
         ================================================================== -->
    <script>
    (function () {
        "use strict";

        const MOCK = {
            // Estado del dispositivo simulado. Edita esto para probar escenarios.
            params: {
                hasRegisteredUser: true,   // false -> arranca en bienvenida/registro
                sessionValid: true,        // true -> "Editar parámetros" entra directo, sin login
                wifiConnected: false,
                wifiSsid: "",
                firmwareVersion: "1.0.0-preview",
                rtcValid: true,            // false -> pastilla "En espera" y dia/semana en "—"
                planta: "Albahaca",
                enable: true,              // false -> pastilla "Inactivo"
                fpOn: 18, fpOff: 6,
                ledAzul: 70, ledRojo: 45, ledBlanco: 1,
                irrH: 3, irrM: 15, ventH: 6, ventM: 20,
                dia: 34, semana: 5
            },
            networks: [
                { ssid: "MiRed",   rssi: -48, secure: true },
                { ssid: "Vecino",  rssi: -71, secure: false },
                { ssid: "IoT_2G",  rssi: -80, secure: true }
            ]
        };

        // El portal guarda el token en localStorage y algunos botones (p. ej. "Iniciar
        // cultivo nuevo") abortan si está vacío. Con sessionValid en true se siembra
        // uno para que esos flujos se puedan probar sin pasar por el login.
        if (MOCK.params.sessionValid) {
            try { localStorage.setItem("spToken", "preview-token"); } catch (e) {}
        }

        const json = (obj, status = 200) => Promise.resolve({
            ok: status >= 200 && status < 300,
            status,
            json: () => Promise.resolve(obj),
            text: () => Promise.resolve(JSON.stringify(obj))
        });

        const realFetch = window.fetch;
        window.fetch = function (url, opts) {
            const u = String(url);
            const okMsg = (message) => json({ status: true, message });
            const body = () => { try { return JSON.parse(opts.body); } catch (e) { return {}; } };

            if (u.startsWith("/getparams")) return json(MOCK.params);

            if (u.startsWith("/usercredentials")) {
                MOCK.params.hasRegisteredUser = true;
                MOCK.params.sessionValid = true;
                return json({ status: true, message: "Usuario registrado.", token: "preview-token" });
            }

            if (u.startsWith("/authusercredentials")) {
                const b = body();
                // El reset de fábrica: en el firmware solo se acepta por la interfaz del
                // AP; aquí se simula el caso permitido.
                if (b.pass === "**reset**") {
                    Object.assign(MOCK.params, {
                        hasRegisteredUser: false, sessionValid: false,
                        wifiConnected: false, wifiSsid: "", planta: "",
                        enable: false, dia: 0, semana: 0
                    });
                    return json({ status: true, message: "Factory reset ejecutado." });
                }
                MOCK.params.sessionValid = true;
                return json({ status: true, message: "Acceso concedido.", token: "preview-token" });
            }

            if (u.startsWith("/newparams")) {
                Object.assign(MOCK.params, body());
                // El firmware deriva la edad del cultivo del RTC; al guardar por primera
                // vez ancla el cultivo, así que aquí arranca en el día 1.
                if (!MOCK.params.dia) { MOCK.params.dia = 1; MOCK.params.semana = 1; }
                return okMsg("Parámetros actualizados correctamente.");
            }

            if (u.startsWith("/newcrop")) {
                // Igual que el firmware: borra los datos del cultivo y CONSERVA la cuenta
                // y la red Wi-Fi.
                Object.assign(MOCK.params, {
                    planta: "", enable: false, fpOn: 0, fpOff: 0,
                    ledAzul: 0, ledRojo: 0, ledBlanco: 0,
                    irrH: 0, irrM: 0, ventH: 0, ventM: 0,
                    dia: 0, semana: 0
                });
                return json({ status: true, message: "Cultivo reiniciado. Configura los parámetros del nuevo cultivo para comenzar." });
            }

            if (u.startsWith("/wifiscan")) {
                // Emula el escaneo ASÍNCRONO: la primera llamada responde scanning=true
                // y la siguiente entrega la lista, para ver el spinner y el polling.
                MOCK._scanCalls = (MOCK._scanCalls || 0) + 1;
                if (MOCK._scanCalls % 2 === 1) return json({ scanning: true, networks: [] });
                return json({ scanning: false, networks: MOCK.networks });
            }

            if (u.startsWith("/wificredentials")) {
                const b = body();
                MOCK.params.wifiConnected = true;
                MOCK.params.wifiSsid = b.ssid || "MiRed";
                return okMsg("Conexión iniciada (simulado).");
            }

            if (u.startsWith("/exit")) {
                MOCK.params.sessionValid = false;
                try { localStorage.removeItem("spToken"); } catch (e) {}
                return okMsg("Desconectado correctamente. Ya puedes cerrar esta ventana y desconectarte de la red SmartPlant.");
            }

            return realFetch ? realFetch(url, opts) : json({}, 404);
        };

        // OTA usa XMLHttpRequest (fetch no reporta progreso de subida), así que
        // fetch mockeado no basta: se parchea también XHR para /otaupdate. Emula
        // el avance de la subida y responde 200 tras "escribir" el firmware. El
        // resto de URLs caen al XHR real.
        const RealXHR = window.XMLHttpRequest;
        window.XMLHttpRequest = function () {
            const real = new RealXHR();
            let isOta = false;
            const upListeners = {};
            const selfListeners = {};
            const api = {
                upload: {
                    addEventListener: (t, cb) => { (upListeners[t] = upListeners[t] || []).push(cb); }
                },
                addEventListener: (t, cb) => { (selfListeners[t] = selfListeners[t] || []).push(cb); },
                setRequestHeader: () => {},
                open: (method, url) => {
                    isOta = String(url).startsWith("/otaupdate");
                    if (!isOta) real.open(method, url);
                },
                get status() { return isOta ? api._status : real.status; },
                get responseText() { return isOta ? api._responseText : real.responseText; },
                _status: 0,
                _responseText: "",
                send: (bodyData) => {
                    if (!isOta) return real.send(bodyData);
                    const emitUp = (t, e) => (upListeners[t] || []).forEach((cb) => cb(e));
                    const emit = (t, e) => (selfListeners[t] || []).forEach((cb) => cb(e));
                    let pct = 0;
                    const timer = setInterval(() => {
                        pct += 20;
                        emitUp("progress", { lengthComputable: true, loaded: pct, total: 100 });
                        if (pct >= 100) {
                            clearInterval(timer);
                            setTimeout(() => {
                                api._status = 200;
                                api._responseText = JSON.stringify({ status: true, message: "Firmware instalado (simulado)." });
                                emit("load", {});
                            }, 600);
                        }
                    }, 300);
                }
            };
            return api;
        };

        // Banda visible: que nunca se confunda un preview con el equipo real.
        document.addEventListener("DOMContentLoaded", () => {
            const b = document.createElement("div");
            b.textContent = "MODO PREVIEW · datos simulados, sin dispositivo";
            b.style.cssText = "position:fixed;top:0;left:0;right:0;z-index:9999;" +
                "background:#ff9800;color:#000;font:600 12px system-ui;" +
                "text-align:center;padding:4px;letter-spacing:.3px";
            document.body.appendChild(b);
        });
    })();
    </script>
"""


def main() -> int:
    if not SRC.exists():
        print(f"No se encontró {SRC}", file=sys.stderr)
        return 1
    html = SRC.read_text(encoding="utf-8")

    if "</head>" not in html:
        print(f"{SRC} no tiene </head>: no sé dónde inyectar el mock", file=sys.stderr)
        return 1

    # Inyecta una sola vez, justo antes de cerrar el head.
    preview = html.replace("</head>", MOCK + "</head>", 1)
    DST.write_text(preview, encoding="utf-8")

    print(f"Regenerado {DST} ({len(preview.encode('utf-8'))} bytes) desde {SRC}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
