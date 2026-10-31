#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "$#" -ne 1 ]; then echo "Usage: ./build_macos.sh /absolute/path/ivao_pilot.xpl" >&2; exit 2; fi
PLUGIN="$1"
mkdir -p diagnostic_xplane_root/Resources/plugins/IVAO_CSL
python3 make_shim.py
python3 build_model_index.py
clang -std=c11 -Wall -Wextra -dynamiclib -o libXPLMShim.dylib xplm_shim.c
clang -std=c11 -Wall -Wextra -o altitude_host host.c ivao_fsd.c fg_traffic.c -pthread
cp -p "$PLUGIN" ivao_pilot_test.xpl
install_name_tool -change '@executable_path/../../../Resources/plugins/XPLM.framework/XPLM' '@loader_path/libXPLMShim.dylib' ivao_pilot_test.xpl
install_name_tool -add_rpath /Library/Frameworks ivao_pilot_test.xpl 2>/dev/null || true
codesign --force --sign - ivao_pilot_test.xpl
codesign --force --sign - libXPLMShim.dylib
codesign --force --sign - altitude_host
printf 'Built test host. Run: ./altitude_host "$PWD/ivao_pilot_test.xpl"\n'
