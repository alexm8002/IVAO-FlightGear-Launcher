# IVAO FlightGear Launcher - macOS prototype 0.1

Build on a Mac with Xcode Command Line Tools / Swift:

    xcode-select --install
    ./build_app_macos.sh
    open "IVAO FlightGear Launcher.app"

Choose the four paths requested in the GUI:
- `ivao_pilot.xpl`, taken from a local legitimate installation of the IVAO X-Plane plugin
- FlightGear FGRoot (actual FGData containing AI/ and Protocol/)
- Altitude.app
- FlightGear.app

Then choose network interface (default `en0`), click Start. The app copies bridge sources to `~/Library/Application Support/IVAOFlightGearLauncher/bridge/`, compiles the existing Phase 65 host at first start (and if the selected plugin changes), and copies `fg-altitude29.xml` into `FGRoot/Protocol/` if absent. It never overwrites a differing existing protocol file. It opens `FlightGear.app/Contents/MacOS/fgfs --launcher` with local ports 5502/5503 and generic output 5501, and waits until 5503 is available **after the user clicks Fly!** in FlightGear's own GUI. It launches the bridge, then Altitude.app.

No plugin binary distributed. The launcher does not force-terminate FlightGear or Altitude when stopping the bridge, to avoid losing an active flight. FlightGear existing process detection and termination supervision remain future improvements.

IMPORTANT security: `--allow-nasal-from-sockets` is enabled; Telnet binds only 127.0.0.1. Never expose ports 5502/5503 to LAN/WAN.

NOTICE: This is source code, not a precompiled .app: compiling SwiftUI requires macOS. Untested on actual Mac/FlightGear runtime. This is an unofficial proof of concept; community redistribution and handling of IVAO plugin needs IVAO review/authorization.


## Correctifs v0.2

- `appDir` est une propriété calculée (aucune référence à `self` pendant une initialisation stockée).
- `altitudeProcess` conserve un `NSRunningApplication`, conformément au type retourné par `NSWorkspace.openApplication`.
- La compilation Swift ajoute `-parse-as-library` pour autoriser `@main` dans un fichier nommé `main.swift`.

Après extraction, exécuter `./build_app_macos.sh` puis `open "IVAO FlightGear Launcher.app"`. Ne pas ouvrir l’application si la compilation a échoué.
