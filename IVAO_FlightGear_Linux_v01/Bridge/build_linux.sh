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
# Export XPLM entry points to dlopen() of the Linux IVAO plugin.
# libpcap is loaded dynamically via dlopen in ivao_fsd.c, so no -lpcap
# dependency at link time is needed (libpcap runtime is still required).
gcc -std=c11 -O2 -rdynamic -o altitude_host host.c ivao_fsd.c fg_traffic.c xplm_shim.c -pthread -ldl -lm
# Verify that the host exports all XPLM symbols required by the plugin.
required=$(mktemp)
exported=$(mktemp)
trap 'rm -f "$required" "$exported"' EXIT
nm -D --undefined-only "$plugin" | awk '$NF ~ /^XPLM/ {print $NF}' | sort -u > "$required"
nm -D --defined-only altitude_host | awk '$NF ~ /^XPLM/ {print $NF}' | sort -u > "$exported"
missing=$(comm -23 "$required" "$exported")
if [[ -n "$missing" ]]; then
  echo "ERREUR: symboles XPLM manquants dans altitude_host:" >&2
  printf '%s\n' "$missing" >&2
  exit 1
fi
printf 'XPLM: %s imports du plugin exposes par altitude_host.\n' "$(wc -l < "$required")"
cp -f "$plugin" ivao_pilot_test.xpl
# Only resolve the XPLM SDK dependency; all other ELF dependencies must be installed normally.
ln -sf libXPLMShim.so XPLM_64.so
ln -sf libXPLMShim.so libXPLM.so
ln -sf libXPLMShim.so XPLM.so
printf 'Compilation Linux terminee. Les dependances du plugin sont a verifier avec ldd.\n'
ldd ivao_pilot_test.xpl || true
