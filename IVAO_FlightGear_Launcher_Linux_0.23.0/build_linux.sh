#!/bin/sh
set -eu
cd "$(dirname "$0")"
command -v cc >/dev/null || { echo 'Missing C compiler: install build-essential' >&2; exit 1; }
command -v python3 >/dev/null || { echo 'Missing python3' >&2; exit 1; }
cd Bridge
python3 make_shim.py
cc -std=c11 -D_GNU_SOURCE -fPIC -shared -Wall -Wextra -o libXPLMShim.so xplm_shim.c -lm -lpthread
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -o altitude_host host.c ivao_fsd.c fg_traffic.c -lm -ldl -pthread
ln -sfn libXPLMShim.so XPLM_64.so
ln -sfn libXPLMShim.so libXPLM.so
printf 'Linux bridge built successfully. Launch ./launch_linux.sh or python3 Launcher/linux_gui.py\n'
