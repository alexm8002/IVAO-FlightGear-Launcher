# IVAO FlightGear Launcher 0.23.0 — Linux x86_64 AppImage

This packaging project is based on the Linux port of the **validated macOS v50** code. macOS v50 is unchanged. The Linux port remains experimental pending end-to-end validation.

## Build on Ubuntu / elementaryOS x86_64

Install the build/runtime prerequisites:

```bash
sudo apt install build-essential python3 python3-tk libpcap0.8 libpcap-dev
```

Download the official **appimagetool-x86_64.AppImage** from the AppImage project's GitHub releases and place it at `tools/appimagetool-x86_64.AppImage` (make it executable). Then:

```bash
chmod +x build_appimage.sh tools/appimagetool-x86_64.AppImage
./build_appimage.sh
```

Output: `IVAO_FlightGear_Launcher-0.23.0-x86_64.AppImage`.

`build_appimage.sh` always creates `build/AppDir` first and does not modify the macOS code.

## Run

```bash
chmod +x IVAO_FlightGear_Launcher-0.23.0-x86_64.AppImage
./IVAO_FlightGear_Launcher-0.23.0-x86_64.AppImage
```

## Requirements and caveats

* **This AppImage is not fully self-contained**: Python 3, Tkinter, and libpcap must be available on the Linux host, along with FlightGear. The packager embeds the compiled traffic bridge, XPLM shim, model TSV, GUI source, and bridge resources. The GUI is invoked with host Python 3.
* An **ELF x86_64 Linux IVAO `.xpl`** plugin must be provided separately and may have additional dependencies. The macOS `.xpl` cannot run on Linux.
* The launcher may need permission to write FlightGear's `Protocol/fg-altitude29.xml` file. If FG_ROOT is system-owned, install that file manually.
* Packet capture may require elevated network permissions. Never run the entire GUI as root as a shortcut.
* AppImage packaging has not been executed in this environment because `appimagetool`/SquashFS tooling and internet download access are unavailable. Validate the finished image under your elementaryOS installation before publishing it as a stable Linux binary.
