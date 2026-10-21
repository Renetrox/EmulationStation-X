#!/usr/bin/env bash
# Safely preview ES-X's static splash without permanently modifying the theme.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
settings="$HOME/.emulationstation/es_settings.cfg"
if [[ ! -f "$settings" ]]; then
    echo "No encuentro $settings" >&2
    exit 1
fi

theme="$(python3 - "$settings" <<'PY'
import sys
import xml.etree.ElementTree as ET
try:
    root = ET.parse(sys.argv[1]).getroot()
    print(next((item.get("value", "") for item in root
                if item.get("name") == "ThemeSet"), ""))
except (ET.ParseError, OSError) as exc:
    raise SystemExit("No pude leer el tema seleccionado: " + str(exc))
PY
)"
if [[ -z "$theme" || "$theme" == */* || "$theme" == "." || "$theme" == ".." ]]; then
    echo "No hay un ThemeSet válido en $settings" >&2
    exit 1
fi

root=""
if [[ -d "$HOME/.emulationstation/themes/$theme" ]]; then
    root="$HOME/.emulationstation/themes/$theme"
elif [[ -d "/etc/emulationstation/themes/$theme" ]]; then
    root="/etc/emulationstation/themes/$theme"
fi
if [[ -z "$root" || ! -w "$root" ]]; then
    echo "El tema '$theme' no se encuentra o no se puede escribir de forma segura." >&2
    echo "No se modificó ningún archivo." >&2
    exit 1
fi

splash="$root/splash.xml"
backup_dir="$(mktemp -d)"
original=false
restore_splash() {
    rm -f -- "$splash"
    if [[ "$original" == true ]]; then
        mv -- "$backup_dir/splash.xml" "$splash"
    fi
    rmdir "$backup_dir" 2>/dev/null || true
}
if [[ -e "$splash" || -L "$splash" ]]; then
    mv -- "$splash" "$backup_dir/splash.xml"
    original=true
fi
trap restore_splash EXIT

cp -- "$repo/docs/examples/splash-smoke-test.xml" "$splash"
echo "Probando splash temporal en el tema: $theme"
echo "Al salir de ES-X se restablecerá automáticamente el splash.xml original."
cd "$repo"
./emulationstation
