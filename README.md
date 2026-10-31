# IVAO FlightGear Launcher

**A community launcher and compatibility bridge for using IVAO Altitude with FlightGear on macOS and Linux.**

> [!IMPORTANT]
> **Unofficial, experimental project.** Not affiliated with, supported by, or approved by IVAO or the FlightGear project. The macOS launcher/bridge has been reported working with the developer's setup; the Linux X-Plane plugin loading path is **not yet validated**. IVAO software and credentials are **not included** in this repository.

## What it does

The graphical launcher saves four paths (IVAO X-Plane plugin, FlightGear `FGRoot`, IVAO Altitude application, and FlightGear application/executable), then:

1. Prepares the FlightGear generic protocol `fg-altitude29.xml`.
2. Builds the local compatibility host and XPLM shim if required.
3. Opens the **FlightGear graphical launcher**, passing the required local socket options.
4. Waits for the user to select an aircraft and click **Fly!** (FlightGear Telnet port `5503` becomes available).
5. Starts the bridge in **continuous `--run` mode** — **no HOLD and no runtime timeout**.
6. Opens IVAO Altitude.

The bridge exchanges aircraft state/radio information with FlightGear, reads IVAO network information to display connected traffic, and selects matching FlightGear AI aircraft models when available (generic A320 fallback). These capabilities depend on the installed FG models and the local IVAO setup.

## Current status

| Platform | Launcher | Bridge / plugin compatibility |
| --- | --- | --- |
| macOS 12+ | SwiftUI launcher v0.7 | **Working in the original developer setup**; requires the macOS IVAO X-Plane plugin. |
| Linux x86_64 | Python 3 / Tkinter launcher v0.1 | **Prototype**. ELF compilation checked, but real IVAO Linux plugin loading and operation need testing. |

Other architectures (including Linux ARM64) are not currently supported by this X-Plane plugin loading approach. Running FlightGear alone does not require these plugin dependencies.

## Required applications (both platforms)

- **FlightGear** and its corresponding **FGData / FGRoot** directory (including `Protocol/` and, for AI matching, `AI/Aircraft/`). FlightGear 2024.1 was used during development; other versions may need adjustments.
- **IVAO Altitude** installed from an official IVAO source, with an IVAO account and the permissions/credentials required to use the service.
- **IVAO X-Plane plugin** installed separately from an authorized source, for the *same OS and CPU architecture* as the bridge host:
  - macOS: `mac_x64/.../*.xpl` (Mach-O).
  - Linux: `lin_x64/.../*.xpl` (ELF x86_64).
- An available local network interface for the bridge's IVAO traffic capture (for example, `en0` on macOS or an interface such as `enp...` on Linux). Set the interface in the launcher; the examples are **not universal defaults**.

> The bridge executes the installed native IVAO plugin through an experimental XPLM compatibility shim. This is **not an official IVAO integration**; review IVAO's current terms and obtain permission before distributing or publicizing any integration that uses the proprietary plugin.

## Build dependencies

### macOS

- macOS **12 or later** (launcher deployment target).
- Xcode Command Line Tools, providing `swiftc`, `clang`, `codesign`, and `install_name_tool`.
- **Python 3**, accessible as `python3` (for shim/model-index generation).
- macOS frameworks: **SwiftUI**, **AppKit** (provided with macOS).
- A macOS IVAO X-Plane plugin compatible with the host machine. The current bridge was developed around an `mac_x64` plugin; depending on your Mac, **Rosetta / CPU architecture compatibility may need attention**.

Install the Apple tools if not already present:

```bash
xcode-select --install
python3 --version
swiftc --version
clang --version
```

### Linux (Debian / Ubuntu example)

- **Linux x86_64** with a graphical desktop.
- **Python 3** with **Tkinter** (`python3-tk`).
- **GCC**, standard C development headers, `libdl`, `libm`, POSIX threads.
- `file`, `ldd` (usually provided by glibc/binutils packages), and **libpcap runtime** for packet capture.
- An official Linux x86_64 IVAO X-Plane plugin and its own shared-library dependencies.

```bash
sudo apt update
sudo apt install python3 python3-tk gcc libc6-dev file libpcap0.8 unzip
```

Package names differ on Fedora, Arch and other distributions. To inspect any missing plugin dependencies after building:

```bash
ldd Bridge/ivao_pilot_test.xpl
```

**Do not launch the graphical application as root.** Packet-capture permissions are distribution-specific and must be configured separately using least privilege; do not grant blanket root privileges to the launcher or plugin.

## Download and layout

Download the source archive for your platform from this project's GitHub **Releases** (when published), or clone the repository. The macOS and Linux prototypes currently have separate source directories.

Typical layout:

```text
IVAO_FlightGear_Launcher_v07/
├── Launcher/main.swift
├── Bridge/
│   ├── build_macos.sh
│   ├── host.c
│   ├── xplm_shim.c
│   ├── fg_traffic.c
│   ├── ivao_fsd.c
│   ├── fg-altitude29.xml
│   └── ...
└── build_app_macos.sh

IVAO_FlightGear_Linux_v01/
├── launcher.py
├── install.sh
└── Bridge/
    ├── build_linux.sh
    ├── fg-altitude29.xml
    └── ...
```

The proprietary IVAO plugin file must **not** be committed to the repository.

## Build and run: macOS

From the macOS source directory:

```bash
chmod +x build_app_macos.sh
./build_app_macos.sh
open "IVAO FlightGear Launcher.app"
```

The build script produces the `.app` bundle, copies the bridge sources into it, and applies an **ad-hoc code signature** (`codesign --sign -`). This is sufficient for local development; it is **not** a Developer ID signature or Apple notarization.

For signed public distribution, use your own valid Apple **Developer ID Application** certificate and a proper signing/notarization pipeline. **After adding or replacing an app icon**, rebuild/update `Contents/Resources`, reference it with `CFBundleIconFile` in `Info.plist`, and **sign the completed bundle last**. Do not modify a signed bundle afterwards.

## Install and run: Linux

From the Linux source directory:

```bash
chmod +x install.sh Bridge/build_linux.sh
./install.sh
python3 launcher.py
```

`install.sh` creates a desktop entry under `~/.local/share/applications/`. The launcher invokes the bridge's `build_linux.sh`, which compiles its ELF shim and native host with GCC. Linux execution of the actual IVAO plugin **remains experimental**.

## First launch / configuration

In the graphical interface, choose:

1. **IVAO X-Plane plugin** — `.xpl` for the correct operating system (`mac_x64` on macOS, `lin_x64` on Linux).
2. **FlightGear FGRoot** — the data directory containing `Protocol/`.
3. **IVAO Altitude application** — `Altitude.app` on macOS, executable/AppImage on Linux.
4. **FlightGear application** — `FlightGear.app` on macOS, `fgfs` executable on Linux.

Select the active capture interface and optionally enable **Diagnostics**. Click **Start FlightGear + IVAO**, then click **Fly!** inside the FlightGear GUI. The launcher starts the bridge and then opens Altitude once FlightGear is ready.

On macOS, applications are launched through **LaunchServices** using their `.app` bundles and `Info.plist`, not hard-coded paths to `Contents/MacOS/fgfs`.

## FlightGear options used

The launcher passes approximately the following options to FlightGear:

```text
--launcher
--fg-root=/path/to/FGRoot
--disable-ai-traffic
--allow-nasal-from-sockets
--telnet=socket,in,10,127.0.0.1,5502,tcp
--telnet=socket,in,10,127.0.0.1,5503,tcp
--generic=socket,out,20,127.0.0.1,5501,udp,fg-altitude29
```

| Port | Protocol | Purpose |
| --- | --- | --- |
| `5501` | UDP, FlightGear output | Aircraft/flight data via `fg-altitude29` |
| `5502` | TCP, Telnet | Bridge/FlightGear control |
| `5503` | TCP, Telnet | Nasal injection and FlightGear AI traffic |

The application supplies the `fg-altitude29.xml` protocol file from its sources. **Do not use `--disable-ai-models`**: this would also prevent the bridge's aircraft models from appearing. `--disable-ai-traffic` disables FlightGear's unrelated default AI traffic instead.

> [!CAUTION]
> `--allow-nasal-from-sockets` permits powerful code execution inside FlightGear. All related Telnet listeners must remain restricted to **`127.0.0.1`** and must **not** be made reachable from your LAN or the Internet. Only use trusted local programs and source/plugin binaries.

## Logs and troubleshooting

**macOS**:

```text
~/Library/Application Support/IVAOFlightGearLauncher/logs/
```

**Linux**:

```text
~/.local/state/ivao-flightgear-launcher/logs/
```

| Log file | Description |
| --- | --- |
| `launcher.log` | Launcher sequence and startup errors |
| `build.log` | Local native build details |
| `bridge.log` | Bridge diagnostics when enabled |

Enable **Diagnostics** and use **Open logs** in the interface when reporting a problem. The bridge's `--run` mode is **continuous**, with no diagnostic HOLD and no fixed timeout. Logging is optional and does not alter its lifetime.

Common checks:

- **FlightGear never becomes ready:** click **Fly!**, check whether TCP port `5503` is listening, and verify the three socket options.
- **Altitude does not start:** inspect `launcher.log`, `build.log` and `bridge.log`; a failed bridge build/start can stop the launch sequence before Altitude.
- **Linux: plugin fails to load:** inspect `ldd Bridge/ivao_pilot_test.xpl` and confirm the plugin is a Linux **ELF x86_64** file, not a macOS Mach-O binary.
- **No IVAO traffic:** verify the correct network interface and packet-capture permissions; a running GUI is not proof that packet capture is working.
- **No aircraft models:** verify `FGRoot/AI/Aircraft/`, the generated model catalogue, and that AI models are not globally disabled.
- **Application updated, old icon still visible (macOS):** rebuild/re-sign the complete bundle and allow LaunchServices/Finder to refresh its icon cache.

When filing an issue, share the OS/version, FlightGear version, launcher version, architecture, steps to reproduce and **sanitized** logs. **Never share IVAO passwords, auth tokens, plugin binaries or other credentials.**

## Known limitations / roadmap

- Linux XPLM/ELF plugin integration needs real-world validation.
- Session lifecycle and cleanup should be further tested (FlightGear exit, Altitude exit, restart, multiple starts).
- macOS Developer ID signing and notarization are required for a frictionless public distribution; current ad-hoc signing is for local builds.
- An installer and packaged Linux desktop icon are not yet included in these prototypes.
- FlightGear protocols, aircraft model availability and OS-specific dependencies may vary by release.

## Legal and trademark notice

FlightGear and IVAO are separate projects/organizations. Names and logos remain the property of their respective owners. This unofficial launcher does **not** include, modify in-place, or license the proprietary IVAO plugin for redistribution. Users must obtain IVAO Altitude and its plugin through authorized channels and follow the applicable terms. Seek explicit authorization before distributing a public build that relies on loading the IVAO plugin. Contributions must preserve third-party license notices and include only files you have permission to distribute.

## Contributing

Issues and pull requests are welcome, especially Linux x86_64 test results, reproducible build fixes, FlightGear version compatibility, and safe session shutdown behavior. Please document your environment and attach redacted diagnostic logs.

## License

**To be specified by the repository maintainer.** Do not assume that the whole repository can be relicensed under one permissive license: audit inherited ATC/FlightGear/XPLM-related code and third-party assets first. The proprietary IVAO plugin is not part of this repository.
