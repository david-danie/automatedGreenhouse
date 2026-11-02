#!/usr/bin/env python3
"""Regenera ESP32_controller/mainForm.h (HTML del portal GZIP-comprimido) desde
una de las dos PISTAS del formulario.

El proyecto mantiene dos pistas permanentes del formulario:
  - ESTABLE (producción):   HTML/mainForm.html
  - DESARROLLO (pruebas):   HTML/portal-dev/mainForm.html

Ambas pueden promoverse al MISMO artefacto embebido (ESP32_controller/mainForm.h);
el firmware solo sirve uno a la vez, así que se elige cuál promover:

    python3 scripts/gen_mainform.py          # promueve la ESTABLE (por defecto)
    python3 scripts/gen_mainform.py --dev     # promueve la de DESARROLLO (portal-dev)

El artefacto que sirve el ESP32 es el HTML fuente SIN comentarios de línea
completa y COMPRIMIDO con gzip, como arreglo de bytes en PROGMEM + su longitud:

    static const unsigned char mainForm_gz[] PROGMEM = { 0x1f, 0x8b, ... };
    static const unsigned int  mainForm_gz_len = NNNN;

Si existe `firmware/main/` (firmware ESP-IDF, rama esp-idf-migration), se escribe
también `firmware/main/mainForm.h`: el mismo contenido con un preámbulo que define
PROGMEM vacío, para que ambos firmwares sirvan siempre el mismo portal.

El navegador descomprime gzip de forma transparente; el firmware solo declara el
encabezado `Content-Encoding: gzip` al servir (ver handleRoot). gzip reduce el
HTML (~73 KB) a ~12–18 KB: menos flash, menos chunks por el AP y carga más rápida.

Reglas de limpieza (antes de comprimir):
- Se ELIMINAN las líneas que son un comentario completo:
    * comentarios de línea JS que empiezan con //
    * comentarios de bloque CSS/JS  /* ... */  (incluye multilínea)
    * comentarios HTML  <!-- ... -->  (incluye multilínea)
- Se CONSERVAN los comentarios inline (código seguido de // ...), las líneas
  en blanco y todo lo demás, byte a byte.
"""
import sys
import gzip
from pathlib import Path

SRC_ESTABLE = Path("HTML/mainForm.html")
SRC_DEV = Path("HTML/portal-dev/mainForm.html")
DST = Path("ESP32_controller/mainForm.h")
# Copia para el firmware ESP-IDF (solo existe en la rama esp-idf-migration). Mismo
# contenido que DST con un preámbulo que define PROGMEM vacío (no existe en IDF).
DST_IDF = Path("firmware/main/mainForm.h")
IDF_PREAMBLE = (
    "// Artefacto generado (gzip del portal). Copia para el build ESP-IDF.\n"
    "// En IDF puro PROGMEM no existe; se define vacío para reutilizar el mismo\n"
    "// formato que genera scripts/gen_mainform.py.\n"
    "#ifndef PROGMEM\n"
    "#define PROGMEM\n"
    "#endif\n"
    "\n"
)


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


def to_byte_array_header(data: bytes) -> str:
    """Convierte bytes gzip en un header C++ con el arreglo PROGMEM y su longitud.
    16 bytes por línea para que el archivo sea legible y no una sola línea enorme."""
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    body = "\n".join(lines)
    return (
        "// Generado por scripts/gen_mainform.py — NO EDITAR A MANO.\n"
        "// HTML del portal (sin comentarios) comprimido con gzip. Se sirve con\n"
        "// Content-Encoding: gzip; el navegador lo descomprime. Ver handleRoot.\n"
        "static const unsigned char mainForm_gz[] PROGMEM = {\n"
        f"{body}\n"
        "};\n"
        f"static const unsigned int mainForm_gz_len = {len(data)};\n"
    )


def main() -> int:
    dev = "--dev" in sys.argv[1:]
    src = SRC_DEV if dev else SRC_ESTABLE
    pista = "DESARROLLO (portal-dev)" if dev else "ESTABLE"
    if not src.exists():
        print(f"No se encontró {src}", file=sys.stderr)
        return 1
    html = src.read_text(encoding="utf-8")
    cleaned = strip_full_line_comments(html)
    raw = cleaned.encode("utf-8")
    # mtime=0 para que el .gz sea DETERMINISTA (mismo HTML -> mismos bytes), y no
    # cambie en cada ejecución por el timestamp que gzip incrusta por defecto.
    gz = gzip.compress(raw, compresslevel=9, mtime=0)
    header = to_byte_array_header(gz)
    DST.write_text(header, encoding="utf-8")
    ratio = 100 * (1 - len(gz) / len(raw)) if raw else 0
    print(f"Regenerado {DST}: {len(raw)} B HTML -> {len(gz)} B gzip "
          f"(-{ratio:.0f}%) desde {src} [pista {pista}]")
    # Si está el firmware ESP-IDF (rama esp-idf-migration), se regenera también su
    # copia para que ambos firmwares sirvan el mismo portal.
    if DST_IDF.parent.is_dir():
        DST_IDF.write_text(IDF_PREAMBLE + header, encoding="utf-8")
        print(f"Regenerado {DST_IDF} (copia ESP-IDF)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
