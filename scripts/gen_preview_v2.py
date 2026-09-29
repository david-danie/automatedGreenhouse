#!/usr/bin/env python3
"""Genera HTML/portal-v2/mainForm.preview.html desde HTML/portal-v2/mainForm.html.

Es el equivalente de scripts/gen_preview.py pero para el Portal V2. El preview es
el MISMO portal con un mock de `fetch` y `XMLHttpRequest` inyectado antes de
`</head>`, para que el portal funcione en el navegador sin ESP32:
  - `fetch` responde los endpoints con datos simulados y sin validar token.
  - `XMLHttpRequest` se intercepta para la subida OTA (`/otaupdate`), simulando el
    progreso de subida y el éxito, para ver la vista `flashing`.

Se GENERA en vez de mantenerse a mano para que no pueda desincronizarse del
fuente (`mainForm.html`). Nunca se edita a mano.

El escenario simulado se controla con las banderas del bloque `MOCK.params` (edita
este script si el cambio debe persistir entre regeneraciones).

Uso, desde la raíz del proyecto:
    python3 scripts/gen_preview_v2.py
"""
import sys
from pathlib import Path

SRC = Path("HTML/portal-v2/mainForm.html")
DST = Path("HTML/portal-v2/mainForm.preview.html")

# El mock se inyecta ANTES de </head> para que window.fetch/XMLHttpRequest queden
# parcheados antes de que corra el <script> del portal (que vive en el <body>).
MOCK = r"""    <!-- ===================================================================
         MODO PREVIEW (mock) — SOLO PARA PROBAR EN EL NAVEGADOR SIN ESP32
         ===================================================================
         ARCHIVO GENERADO por scripts/gen_preview_v2.py — NO EDITAR A MANO.
         Es una COPIA de mainForm.html con un simulador del firmware inyectado.
         NO se sirve desde el dispositivo. La fuente de verdad es mainForm.html.

         Qué simula:
           - /getparams  -> cultivo de ejemplo, hasRegisteredUser + sessionValid
                            en true, así arranca en el dashboard y la compuerta
                            de auth se cruza sola (sin pedir login).
           - /wifiscan, /wificredentials, /newparams, /exit -> respuestas OK.
           - /otaupdate  -> simula progreso de subida y éxito.
         Cambia MOCK.params abajo para probar otros escenarios (p.ej.
         hasRegisteredUser:false para ver welcome/registro).
    -->
    <script>
    (function () {
        "use strict";

        const MOCK = {
            // Estado del dispositivo simulado. Edítalo para probar escenarios.
            params: {
                hasRegisteredUser: true,   // false -> arranca en welcome/registro
                sessionValid: true,       // false -> dashboard en modo lectura (muestra el banner); true -> se salta el login
                wifiConnected: false,
                wifiSsid: "",
                firmwareVersion: "1.0.0-preview",
                rtcValid: true,            // false -> la pastilla pasa a "En espera" y dia/semana a "—"
                planta: "Albahaca",
                enable: true,
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
        // cultivo nuevo") abortan si está vacío. Con sessionValid en true se siembra uno
        // para que esos flujos se puedan probar sin pasar por el login.
        if (MOCK.params.sessionValid) {
            try { localStorage.setItem("spToken", "preview-token"); } catch (e) {}
        }

        const json = (obj, status = 200) => Promise.resolve({
            ok: status >= 200 && status < 300,
            status,
            json: () => Promise.resolve(obj),
            text: () => Promise.resolve(JSON.stringify(obj))
        });

        // ---- Intercepta fetch ----
        const realFetch = window.fetch;
        window.fetch = function (url, opts) {
            const u = String(url);
            const okMsg = (message) => json({ status: true, message });

            if (u.startsWith("/getparams")) return json(MOCK.params);
            if (u.startsWith("/usercredentials")) {
                MOCK.params.hasRegisteredUser = true;
                MOCK.params.sessionValid = true;
                return json({ status: true, message: "Usuario registrado.", token: "preview-token" });
            }
            if (u.startsWith("/authusercredentials")) {
                const body = opts && opts.body ? JSON.parse(opts.body) : {};
                if (body.pass === "**reset**") return json({ status: true, message: "Equipo restaurado (simulado)." });
                MOCK.params.sessionValid = true;
                return json({ status: true, message: "Acceso concedido.", token: "preview-token" });
            }
            if (u.startsWith("/newparams")) {
                try { Object.assign(MOCK.params, JSON.parse(opts.body)); } catch (e) {}
                return okMsg("Parámetros actualizados (simulado).");
            }
            if (u.startsWith("/newcrop")) {
                // Emula el firmware: borra datos del cultivo, conserva cuenta y Wi-Fi.
                Object.assign(MOCK.params, {
                    planta: "", enable: false, fpOn: 0, fpOff: 0,
                    ledAzul: 0, ledRojo: 0, ledBlanco: 0,
                    irrH: 0, irrM: 0, ventH: 0, ventM: 0,
                    dia: 0, semana: 0
                });
                return json({ status: true, message: "Cultivo reiniciado. Configura los parámetros del nuevo cultivo para comenzar." });
            }
            if (u.startsWith("/wifiscan")) {
                // Emula el escaneo asincrono del firmware: la primera llamada responde
                // scanning=true y la siguiente ya entrega la lista.
                MOCK._scanCalls = (MOCK._scanCalls || 0) + 1;
                if (MOCK._scanCalls % 2 === 1) return json({ scanning: true, networks: [] });
                return json({ scanning: false, networks: MOCK.networks });
            }
            if (u.startsWith("/wificredentials")) {
                try {
                    const b = JSON.parse(opts.body);
                    MOCK.params.wifiConnected = true;
                    MOCK.params.wifiSsid = b.ssid || "MiRed";
                } catch (e) {}
                return okMsg("Conexión iniciada (simulado).");
            }
            if (u.startsWith("/exit")) return okMsg("Desconectado (simulado). Puedes cerrar esta ventana.");
            return realFetch ? realFetch(url, opts) : json({}, 404);
        };

        // ---- Intercepta XMLHttpRequest (lo usa la subida OTA) ----
        const RealXHR = window.XMLHttpRequest;
        window.XMLHttpRequest = function () {
            const u = { method: "GET", url: "" };
            const listeners = {};
            const uploadListeners = {};
            const api = {
                upload: { addEventListener: (t, cb) => { (uploadListeners[t] = uploadListeners[t] || []).push(cb); } },
                addEventListener: (t, cb) => { (listeners[t] = listeners[t] || []).push(cb); },
                open: (m, url) => { u.method = m; u.url = url; },
                setRequestHeader: () => {},
                status: 0,
                responseText: "",
                send: function () {
                    // Simula progreso de subida y luego éxito, para ver la vista flashing.
                    const emitUp = (t, e) => (uploadListeners[t] || []).forEach((cb) => cb(e));
                    const emit = (t, e) => (listeners[t] || []).forEach((cb) => cb(e));
                    let pct = 0;
                    const timer = setInterval(() => {
                        pct += 20;
                        emitUp("progress", { lengthComputable: true, loaded: pct, total: 100 });
                        if (pct >= 100) {
                            clearInterval(timer);
                            setTimeout(() => {
                                api.status = 200;
                                api.responseText = JSON.stringify({ status: true, message: "Firmware instalado (simulado)." });
                                emit("load", {});
                            }, 600);
                        }
                    }, 400);
                }
            };
            return api;
        };

        // Aviso visible de que esto es preview.
        document.addEventListener("DOMContentLoaded", () => {
            const b = document.createElement("div");
            b.textContent = "MODO PREVIEW — datos simulados, sin dispositivo";
            b.style.cssText = "position:fixed;top:0;left:0;right:0;z-index:9999;" +
                "background:#ff9800;color:#000;font:600 12px system-ui;text-align:center;padding:4px";
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
