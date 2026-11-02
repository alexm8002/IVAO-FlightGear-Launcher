#!/bin/sh
set -eu
cd "$(dirname "$0")"
APP="IVAO FlightGear Launcher.app"
# Always build a fresh signed bundle, never leave stale resources from prior builds.
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp FGIVAO.icns "$APP/Contents/Resources"
cp -R Bridge "$APP/Contents/Resources/Bridge"
# Build bridge once at application packaging time; no plugin binary distributed.
BRIDGE="$APP/Contents/Resources/Bridge"
(
 cd "$BRIDGE"
 python3 make_shim.py
 clang -std=c11 -Wall -Wextra -dynamiclib -o libXPLMShim.dylib xplm_shim.c
 clang -std=c11 -Wall -Wextra -o altitude_host host.c ivao_fsd.c fg_traffic.c -pthread
 codesign --force --sign - libXPLMShim.dylib
 codesign --force --sign - altitude_host
)
swiftc -parse-as-library -O -framework SwiftUI -framework AppKit -o "$APP/Contents/MacOS/IVAOFlightGearLauncher" Launcher/main.swift
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>IVAOFlightGearLauncher</string>
<key>CFBundleIdentifier</key><string>org.flightgear.ivao.launcher.unofficial</string>
<key>CFBundleName</key><string>IVAO FlightGear Launcher</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.23.0</string>
<key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>CFBundleIconFile</key><string>FGIVAO</string>
<key>CFBundleIconName</key><string>FGIVAO</string>
</dict></plist>
PLIST
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
echo "Application construite : $APP"
