# Phase 65 — Quiet mode and generic A320 fallback

Based on the phase 64 restart-safe bridge.

- Unknown IVAO aircraft and missing catalogue use `Aircraft/A320/A320-default.xml` as fallback. This file was present in the supplied FlightGear 2024.1 AI aircraft inventory.
- Normal execution is silent on stdout and stderr, including logs produced by the shim, IVAO core and FSD decoder. No log file is created by default.
- Pass `--verbose` to restore all diagnostics to the terminal.
- Or pass `--log-file /path/to/diagnostics.log` to capture diagnostics for bug reporting.
- CLI usage/errors before initializing silent mode remain visible.
- The `--traffic-pcap-only` test command preserves its own output.

Example:
```sh
./build_macos.sh "/Volumes/Sans titre/X-Plane 12/Resources/plugins/ivao_pilot/mac_x64/ivao_pilot.xpl"
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503
```

Diagnostic example:
```sh
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503 --log-file phase65.log
```

Note: this remains an experimental host requiring independent FlightGear and macOS testing; it is not an officially approved IVAO client. Review licensing of the embedded proprietary IVAO plugin, XPLM emulation, and the distribution model before any community publication.
