# IVAO FlightGear Launcher Linux - port from validated macOS v50

This is a **Linux port of the v50 source**, not a claim of a validated IVAO login on Linux.
The macOS v50 remains the validated production version and is not changed.

## Requirements
- Linux x86_64 or ARM64, with a native **Linux ELF 64-bit IVAO Altitude X-Plane plugin** (`lin.xpl` / `.xpl`) matching the host CPU. The macOS `.xpl` cannot be used. No IVAO proprietary binaries are included.
- FlightGear (`fgfs`) and installed FG_ROOT containing `AI/` and `Protocol/`.
- `python3`, `python3-tk`, a C compiler (`build-essential` on Ubuntu / elementaryOS).
- `libpcap` shared library for traffic capture. Live capture of the IVAO TCP session requires suitable access to the network interface (e.g. group or carefully scoped capabilities); lack of privileges causes missing traffic despite a successful plugin connection. Avoid running the entire GUI as root.
- For launching the actual IVAO Altitude UI, install the Linux version separately and select its executable if available. Whether the IVAO plugin has a usable native Linux build is an **external prerequisite** not verified here.

On Ubuntu / elementaryOS: `sudo apt install build-essential python3 python3-tk libpcap0.8`

## Build
```
./build_linux.sh
./launch_linux.sh
```
The build creates `Bridge/altitude_host`, `Bridge/libXPLMShim.so` and two soname symlinks. Linux binaries built on the development machine may not match your Linux distribution; building locally is recommended.

In the GUI select (1) the Linux native plugin file, (2) FG_ROOT, (3) the fgfs executable, (4) the network interface (e.g., `enp3s0` / `wlp2s0`), and optionally (5) the Altitude executable. Click **Demarrer FlightGear + IVAO**, then **Fly!** in FlightGear's launcher. The app waits for FlightGear's Telnet port 5503, then starts the v50 bridge. Logs are in `~/.local/state/ivao-flightgear-launcher`.

The launcher installs `Bridge/fg-altitude29.xml` into `FG_ROOT/Protocol` only when missing (without overwriting). If FG_ROOT is not writable, copy that file manually with appropriate privileges.

## Dependency diagnosis
```
file /absolute/path/to/lin.xpl
ldd /absolute/path/to/lin.xpl
cd Bridge
LD_LIBRARY_PATH="$PWD:${LD_LIBRARY_PATH:-}" ./altitude_host /absolute/path/to/lin.xpl --enable-diagnostic --verbose
```
The `libXPLMShim.so` can satisfy functions implemented in the shim, but other Linux plugin dependencies (audio, GStreamer, Qt and the exact XPLM ABI) must also exist. A native IVAO Linux binary and its dependency compatibility were not available for testing in this environment.

## What is retained
- The v50 `fg_traffic.c` model/livery catalogue and selection, including B737 -> B738 mappings.
- XPMP2 positions, AI object creation/update and the established FlightGear UDP/Telnet protocol.
- Linux specific adaptations: ELF dynamic linking, libXPLM shim as `.so`, Linux `/proc/self/exe` fallback for TSV, lightweight tkinter launcher.

## What is not proven
- Connection to IVAO with a Linux plugin, Altitude GUI compatibility, live FlightGear 3D rendering, and traffic packet capture on the target machine. All require runtime tests on the chosen Linux distribution and plugin.
