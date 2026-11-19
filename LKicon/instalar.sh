#!/usr/bin/env bash
# ============================================================
# INSCRIPCION DE K++ EN LINUX.
#
# Registra la extension .kpp como text/x-kpp en la base MIME
# del usuario y coloca el icono de la lengua (la K roja) en su
# tema de iconos. Sin root: todo vive bajo el directorio de
# datos del usuario que lo corre, en cualquier maquina.
#
# Uso:
#   bash instalar.sh               inscribe K++
#   bash instalar.sh --desinstalar retira la inscripcion
# ============================================================

set -u

BASE="${XDG_DATA_HOME:-$HOME/.local/share}"
DIR_MIME="$BASE/mime"
DIR_PAQUETES="$DIR_MIME/packages"
DIR_SVG="$BASE/icons/hicolor/scalable/mimetypes"
DIR_PNG="$BASE/icons/hicolor/256x256/mimetypes"

CARPETA="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "${1:-}" = "--desinstalar" ]; then
    rm -f "$DIR_PAQUETES/kpp.xml"
    rm -f "$DIR_SVG/text-x-kpp.svg"
    rm -f "$DIR_PNG/text-x-kpp.png"
    if command -v update-mime-database >/dev/null 2>&1; then
        update-mime-database "$DIR_MIME"
    fi
    echo "Inscripcion de K++ retirada. Reabre tu gestor de archivos."
    exit 0
fi

for archivo in kpp.xml text-x-kpp.svg text-x-kpp.png; do
    if [ ! -f "$CARPETA/$archivo" ]; then
        echo "Falta $archivo: debe estar en la misma carpeta que este script."
        exit 1
    fi
done

mkdir -p "$DIR_PAQUETES" "$DIR_SVG" "$DIR_PNG"
cp -f "$CARPETA/kpp.xml"        "$DIR_PAQUETES/kpp.xml"
cp -f "$CARPETA/text-x-kpp.svg" "$DIR_SVG/text-x-kpp.svg"
cp -f "$CARPETA/text-x-kpp.png" "$DIR_PNG/text-x-kpp.png"

if ! command -v update-mime-database >/dev/null 2>&1; then
    echo "No se encontro update-mime-database (paquete shared-mime-info)."
    echo "Los archivos quedaron copiados; la base se regenera al instalar ese paquete."
    exit 1
fi
update-mime-database "$DIR_MIME"

# Autoverificacion: un .kpp de prueba tiene que resolver a text/x-kpp.
# El testigo es gio, la herramienta de la misma biblioteca (GLib) que
# los gestores de archivos usan para decidir el tipo y el icono.
if command -v gio >/dev/null 2>&1; then
    PRUEBA="$(mktemp -d)"
    printf '// K++\n' > "$PRUEBA/prueba.kpp"
    TIPO="$(gio info -a standard::content-type "$PRUEBA/prueba.kpp" 2>/dev/null | sed -n 's/.*content-type: //p')"
    rm -rf "$PRUEBA"
    if [ "$TIPO" = "text/x-kpp" ]; then
        echo "K++ inscrito: los .kpp ya son text/x-kpp."
    else
        echo "Ojo: la consulta respondio '$TIPO' en vez de 'text/x-kpp'."
        exit 1
    fi
else
    echo "K++ inscrito (verificacion omitida: no hay gio)."
fi

echo "Reabre tu gestor de archivos: los .kpp estrenan la K roja."
