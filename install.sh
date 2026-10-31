#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
chmod +x launcher.py Bridge/build_linux.sh
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/ivao-flightgear-launcher.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=IVAO FlightGear Launcher
Exec=/usr/bin/env python3 $PWD/launcher.py
Terminal=false
Categories=Game;Utility;
DESKTOP
printf 'Installation du raccourci terminée. Démarrer : python3 launcher.py\n'
