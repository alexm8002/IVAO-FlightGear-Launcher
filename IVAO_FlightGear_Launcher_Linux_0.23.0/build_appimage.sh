#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$PWD"
ARCH="$(uname -m)"
if [[ "$ARCH" != x86_64 ]]; then echo "This release targets Linux x86_64, got $ARCH" >&2; exit 1; fi
./build_linux.sh
APPDIR="$ROOT/build/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib/ivao-flightgear/Bridge" "$APPDIR/usr/lib/ivao-flightgear/Launcher" "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp -a Bridge/. "$APPDIR/usr/lib/ivao-flightgear/Bridge/"
cp -a Launcher/. "$APPDIR/usr/lib/ivao-flightgear/Launcher/"
# Exclude platform-incompatible plugin executables; user supplies native Linux plugin.
find "$APPDIR/usr/lib/ivao-flightgear" -type f \( -name '*.dylib' -o -name '*.xpl' -o -name '*.o' \) -delete
cp resources/ivao-flightgear.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/ivao-flightgear.png"
cp resources/ivao-flightgear.png "$APPDIR/ivao-flightgear.png"
cp resources/ivao-flightgear.desktop "$APPDIR/ivao-flightgear.desktop"
cp resources/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun" "$APPDIR/usr/lib/ivao-flightgear/Bridge/altitude_host"
# The compatibility shim is discoverable by the native Linux plugin through LD_LIBRARY_PATH.
OUTFILE="$ROOT/IVAO_FlightGear_Launcher-0.23.0-x86_64.AppImage"
if command -v appimagetool >/dev/null 2>&1; then TOOL="$(command -v appimagetool)";
elif [[ -x "$ROOT/tools/appimagetool-x86_64.AppImage" ]]; then TOOL="$ROOT/tools/appimagetool-x86_64.AppImage";
else
  echo 'Missing appimagetool. Obtain appimagetool-x86_64.AppImage from the official AppImage GitHub releases,' >&2
  echo 'save it as tools/appimagetool-x86_64.AppImage, chmod +x it, and run this script again.' >&2
  echo "The complete staging directory was created at: $APPDIR" >&2
  exit 2
fi
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" "$APPDIR" "$OUTFILE"
chmod +x "$OUTFILE"
file "$OUTFILE"
echo "Created: $OUTFILE"
