# IVAO FlightGear Launcher

**IVAO Altitude integration for FlightGear — macOS & Linux**

![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux-blue)
![Release](https://img.shields.io/badge/release-v0.23.0-blue)
![macOS](https://img.shields.io/badge/macOS-stable-brightgreen)
![Linux](https://img.shields.io/badge/Linux-experimental-orange)

IVAO FlightGear Launcher is a cross-platform integration project that connects the **IVAO Altitude Pilot Core** to **FlightGear**, enabling IVAO network connectivity and real-time multiplayer aircraft visualization without requiring X-Plane.

The project uses a custom **X-Plane Plugin Manager (XPLM) compatibility layer** to host the IVAO Pilot Core and an **XPMP2 bridge** to transfer aircraft positions and orientations into FlightGear.

**Current release: v0.23.0**

- **macOS:** Stable and functionally validated.
- **Linux x86_64:** Experimental, compilation validated; end-to-end testing pending.

---

## Features

### IVAO Network Integration

- IVAO Altitude Pilot Core integration.
- Custom XPLM compatibility layer.
- IVAO network connection and multiplayer traffic reception.
- FlightGear simulator synchronization.
- Compatible with IVAO Altitude's existing pilot interface, subject to platform-specific runtime compatibility.

### Real-Time Multiplayer Traffic

- Aircraft creation, updating, and removal in FlightGear.
- Real-time aircraft positions calculated by XPMP2.
- Heading, pitch, roll, and altitude synchronization.
- Smooth aircraft movement and reduced interpolation artifacts.
- Persistent TCP communication for traffic updates.
- Automatic AI aircraft cleanup after disconnection.

### Aircraft Models and Liveries

- Dynamic aircraft model selection using `fg_models.tsv`.
- ICAO aircraft type matching.
- Airline-specific liveries when available.
- Boeing 737 family compatibility with FlightGear B738 models.
- Generic model fallback for unsupported aircraft.
- Automatic model replacement when updated IVAO information is received.
- Detailed model selection diagnostics.

### Native Launchers

**macOS**
- Native graphical launcher.
- FlightGear configuration and startup.
- IVAO Pilot Core integration.
- Native XPLM compatibility library.

**Linux**
- Python/Tkinter graphical launcher.
- Native Linux XPLM compatibility library.
- FlightGear and IVAO plugin path configuration.
- Network interface selection.
- Dedicated build and launch scripts.

---

## Architecture

```text
                  IVAO Network
                       |
                       v
              IVAO Altitude Pilot Core
                       |
                       v
               XPLM Compatibility Layer
                 (macOS / Linux)
                       |
                       v
                  XPMP2 Engine
                       |
                       v
                  Traffic Bridge
                       |
             +---------+---------+
             |                   |
             v                   v
       fg_models.tsv       Aircraft Position
       Model & Livery       and Orientation
             |                   |
             +---------+---------+
                       |
                       v
             FlightGear AI Traffic
               (3D Aircraft Models)
```

The IVAO plugin runs through a compatibility layer rather than a full X-Plane simulator.

XPMP2 provides the aircraft positioning and orientation data. The bridge selects suitable FlightGear models and transmits traffic updates to the simulator.

**The XPMP2 CSL model is only used for technical positioning; it is not the rendered FlightGear aircraft model.**

---

## Platform Support

| Platform | Architecture | Status |
|---|---|---|
| macOS | Supported build architecture | Stable |
| Linux | x86_64 | Experimental |
| Linux | ARM64 | Not validated |
| Windows | — | Not supported |

---

## Requirements

### Common Requirements

- FlightGear with AI traffic support.
- IVAO Altitude Pilot Core compatible with the target platform.
- Suitable aircraft models installed in FlightGear.
- Network connectivity to IVAO.
- Local TCP/UDP ports available for bridge communication.

### macOS Requirements

- macOS with compatible development tools.
- Xcode Command Line Tools.
- FlightGear for macOS.
- Compatible macOS IVAO Pilot Core plugin.

### Linux Requirements

- Linux x86_64 (Ubuntu, elementaryOS, or compatible distribution).
- GCC or compatible C compiler.
- Python 3.
- Tkinter.
- libpcap development/runtime libraries.
- FlightGear for Linux.
- Compatible Linux IVAO Pilot Core plugin (`lin.xpl`).

**Important:** macOS `.xpl` plugins cannot be loaded directly into the Linux native runtime. A Linux-compatible plugin and its dependencies are required.

---

## Installation

### macOS

Download the macOS source archive from the [GitHub Releases](../../releases) page.

Extract it and run:

```bash
./build_app_macos.sh
```

Then launch:

```text
IVAO FlightGear Launcher.app
```

Configure FlightGear and the IVAO Pilot Core paths using the graphical interface.

### Linux (Ubuntu / elementaryOS)

Download the Linux archive from the [GitHub Releases](../../releases) page.

Install the required packages:

```bash
sudo apt update
sudo apt install build-essential python3 python3-tk libpcap0.8 libpcap-dev
```

Extract the archive, open a terminal in the project directory, and build:

```bash
chmod +x build_linux.sh launch_linux.sh
./build_linux.sh
```

Start the launcher:

```bash
./launch_linux.sh
```

Configure the FlightGear executable, FG_ROOT, IVAO plugin path, and network interface.

**Linux support is currently experimental.** Successful compilation does not guarantee IVAO Pilot Core runtime compatibility.

---

## FlightGear Configuration

The bridge uses local communication channels for simulator synchronization.

| Port | Protocol | Purpose |
|---|---|---|
| 5501 | UDP | FlightGear simulator data |
| 5502 | TCP | Radio and simulator commands |
| 5503 | TCP | Persistent FlightGear AI traffic updates |

The required FlightGear communication interfaces must be enabled before starting the bridge.

Ports and binding addresses should be checked against the launcher configuration.

---

## Aircraft Model Matching

The bridge uses the `fg_models.tsv` aircraft catalogue to select the most suitable FlightGear model.

The matching process considers:

1. IVAO aircraft type and airline information.
2. ICAO aircraft identifier.
3. Airline identification from the callsign when applicable.
4. Corresponding model and livery in the FlightGear catalogue.
5. Generic model fallback when no suitable livery exists.

Examples:

| IVAO Aircraft | FlightGear Model Selection |
|---|---|
| `A320AFR` | A320 Air France |
| `A320EZY` | A320 easyJet, if available |
| `B738RYR` | B738 Ryanair |
| `B737KLM` | B738 KLM, if available |
| Unknown B737 livery | Generic B738 |
| Unsupported aircraft | Available generic fallback |

The actual result depends on the FlightGear models installed and the paths recorded in `fg_models.tsv`.

Aircraft models can be replaced dynamically when more accurate IVAO aircraft information becomes available.

---

## Diagnostics

The application provides diagnostic logging for:

- IVAO Pilot Core initialization.
- XPLM shim operations.
- XPMP2 aircraft creation and position updates.
- IVAO network traffic reception.
- Aircraft matching and selected models.
- FlightGear TCP communication.
- AI aircraft lifecycle and model replacement.

For example:

```text
[FG MODEL] indexed 979 aircraft XMLs; ICAO+airline liveries enabled
[FG MODEL SELECT] callsign=AFR73DD ivao=A320AFR selected=Aircraft/A320/A320-AirFrance.xml reason=CATALOGUE_MATCH
```

The number of indexed aircraft depends on the catalogue.

Additional diagnostic properties may be available in FlightGear under:

```text
/sim/ivao-bridge/
```

When reporting an issue, include the launcher bridge log and, when relevant, the FlightGear log.

---

## Known Limitations

- Linux runtime integration is not yet fully validated.
- Linux requires a native compatible IVAO Pilot Core binary.
- Not all IVAO aircraft types have a corresponding FlightGear AI model.
- Some airline liveries are unavailable and require generic fallback models.
- Landing gear and exterior lighting synchronization are not implemented in v0.50.0.
- Linux ARM64 support has not been validated.
- Windows is not supported.

---

## Development Status

### v0.23.0 — macOS Stable

Validated functionality:

- IVAO connection.
- Multiplayer aircraft detection.
- FlightGear 3D aircraft rendering.
- XPMP2-based aircraft movement and orientation.
- Aircraft model matching.
- Airline livery selection.
- Dynamic aircraft model updates.

### v0.23.0 — Linux Experimental

Implemented:

- Linux XPLM compatibility layer.
- Linux bridge port.
- Python/Tkinter launcher.
- Linux build and launch scripts.
- Platform-specific configuration.

Pending validation:

- Native IVAO Pilot Core compatibility.
- End-to-end IVAO connection.
- FlightGear multiplayer aircraft visualization.
- Aircraft model and livery rendering.

---

## Roadmap

- [x] macOS IVAO Altitude integration.
- [x] FlightGear AI multiplayer traffic.
- [x] XPMP2 position and orientation synchronization.
- [x] Dynamic aircraft models and liveries.
- [x] Boeing 737 / B738 compatibility mapping.
- [x] macOS v50 validation.
- [x] Initial Linux port.
- [ ] Linux end-to-end runtime validation.
- [ ] Landing gear animation.
- [ ] Exterior aircraft lighting.
- [ ] Extended FlightGear AI aircraft model compatibility.
- [ ] Linux ARM64 compatibility assessment.

---

## Contributing

Contributions, bug reports, compatibility testing, and improvements are welcome.

When submitting an issue, please specify:

- Operating system and architecture.
- FlightGear version.
- IVAO Altitude Pilot Core version.
- Launcher version.
- Relevant bridge and FlightGear logs.
- Steps to reproduce the issue.

Please avoid including IVAO credentials, passwords, or other sensitive information in logs or issue reports.

---

## Disclaimer

IVAO FlightGear Launcher is an independent interoperability project.

It is not an official IVAO, FlightGear, or X-Plane product and is not endorsed by their respective organizations.

IVAO, FlightGear, and X-Plane names and trademarks remain the property of their respective owners.

Users are responsible for obtaining and using compatible third-party components in accordance with their applicable licenses and terms.

## License

Refer
