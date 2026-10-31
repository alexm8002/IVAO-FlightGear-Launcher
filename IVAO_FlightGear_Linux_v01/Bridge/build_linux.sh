#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -ne 2 ]]; then echo 'Usage: ./build_linux.sh /path/to/lin_x64/ivao_pilot.xpl /path/to/FGRoot' >&2; exit 2; fi
fgroot=$(realpath "$2")
[[ -d "$fgroot/AI/Aircraft" ]] || { echo "FGRoot invalide : $fgroot/AI/Aircraft absent" >&2; exit 2; }
plugin=$(realpath "$1")
[[ -f "$plugin" ]] || { echo 'Plugin absent' >&2; exit 2; }
if ! file "$plugin" | grep -q 'ELF 64-bit'; then echo 'Le plugin doit etre un binaire Linux ELF 64-bit (lin_x64), pas mac_x64.' >&2; exit 2; fi
mkdir -p diagnostic_xplane_root/Resources/plugins/IVAO_CSL
python3 make_shim.py
python3 build_model_index.py --fg-root "$fgroot"
gcc -std=c11 -O2 -fPIC -shared -o libXPLMShim.so xplm_shim.c -pthread
gcc -std=c11 -O2 -o altitude_host host.c ivao_fsd.c fg_traffic.c -pthread -ldl -lm
cp -f "$plugin" ivao_pilot_test.xpl
# Only resolve the XPLM SDK dependency; all other ELF dependencies must be installed normally.
ln -sf libXPLMShim.so XPLM_64.so
ln -sf libXPLMShim.so libXPLM.so
ln -sf libXPLMShim.so XPLM.so
printf 'Compilation Linux terminee. Les dependances du plugin sont a verifier avec ldd.\n'
ldd ivao_pilot_test.xpl || true
