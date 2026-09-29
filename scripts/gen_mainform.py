#!/usr/bin/env python3
"""Regenera ESP32_controller/mainForm.h desde HTML/mainForm.html.

El artefacto que sirve el ESP32 es el HTML fuente SIN comentarios de línea
completa, envuelto en un raw string literal de C++:

    static const char mainForm[] = R"===(
    ...HTML sin comentarios...
    )===";

Reglas de limpieza (derivadas de comparar el fuente con el artefacto vigente):
- Se ELIMINAN las líneas que son un comentario completo:
    * comentarios de línea JS que empiezan con //
    * comentarios de bloque CSS/JS  /* ... */  (incluye multilínea)
    * comentarios HTML  <!-- ... -->  (incluye multilínea)
- Se CONSERVAN los comentarios inline (código seguido de // ...), las líneas
  en blanco y todo lo demás, byte a byte.
"""
import sys
from pathlib import Path

SRC = Path("HTML/mainForm.html")
DST = Path("ESP32_controller/mainForm.h")


def strip_full_line_comments(text: str) -> str:
    out = []
    in_block = None  # None | "css" (/* */) | "html" (<!-- -->)
    for line in text.split("\n"):
        stripped = line.strip()

        if in_block == "css":
            if "*/" in stripped:
                in_block = None
            continue
        if in_block == "html":
            if "-->" in stripped:
                in_block = None
            continue

        # Comentario de línea completa //
        if stripped.startswith("//"):
            continue

        # Comentario de bloque CSS/JS que ocupa toda la línea
        if stripped.startswith("/*"):
            if "*/" not in stripped:
                in_block = "css"
            continue

        # Comentario HTML que ocupa toda la línea
        if stripped.startswith("<!--"):
            if "-->" not in stripped:
                in_block = "html"
            continue

        out.append(line)
    return "\n".join(out)


def main() -> int:
    if not SRC.exists():
        print(f"No se encontró {SRC}", file=sys.stderr)
        return 1
    html = SRC.read_text(encoding="utf-8")
    cleaned = strip_full_line_comments(html)
    artifact = 'static const char mainForm[] = R"===(\n' + cleaned + ')===";\n'
    DST.write_text(artifact, encoding="utf-8")
    print(f"Regenerado {DST} ({len(artifact)} bytes) desde {SRC}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
