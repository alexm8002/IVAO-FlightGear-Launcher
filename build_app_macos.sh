#!/bin/sh
set -eu
cd "$(dirname "$0")"
APP="IVAO FlightGear Launcher.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp FGIVAO.icns "$APP/Contents/Resources"
cp -R Bridge "$APP/Contents/Resources/Bridge"
swiftc -parse-as-library -O -framework SwiftUI -framework AppKit -o "$APP/Contents/MacOS/IVAOFlightGearLauncher" Launcher/main.swift
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>IVAOFlightGearLauncher</string>
<key>CFBundleIdentifier</key><string>org.flightgear.ivao.launcher.unofficial</string>
<key>CFBundleName</key><string>IVAO FlightGear Launcher</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.7.0</string>
<key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>CFBundleIconFile</key><string>FGIVAO</string>
<key>CFBundleIconName</key><string>FGIVAO</string>
</dict></plist>
PLIST
codesign --force --deep --sign - "$APP"
echo "Application construite : $APP"
