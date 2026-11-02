#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec python3 Launcher/linux_gui.py
