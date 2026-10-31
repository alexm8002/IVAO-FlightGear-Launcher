# Phase 47 – IVAO traffic to native FlightGear AI, C only

This is an **experimental first native AI insertion**, not yet a verified live FlightGear render. Phase 46 IVAO capture, COM and XPDR implementation is unchanged. `fg_traffic.c` and `.h` are compiled into the `altitude_host` executable.

## Build

```sh
./build_macos.sh '/absolute/path/to/ivao_pilot.xpl'
```

## FlightGear prerequisite

Unlike the failed multiplayer UDP path, this iteration uses the existing FlightGear Telnet interface, plus permission to execute Nasal through it. Add **`--allow-nasal-from-sockets`** to FlightGear launch settings (retain `--telnet=5502`, UDP generic port 5501 and existing other options). This is a security-sensitive setting: FlightGear Telnet must be accessible on localhost only, not exposed on external network interfaces. This flag is necessary for the Nasal `add-aiobject` command.

## Run

```sh
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai 2>&1 | tee phase47.log
```

When `--fg-ai` is omitted, behaviour is identical to phase 46. When enabled, the host sends up to 32 current aircraft per second via Telnet port 5502 in a Nasal source block. Names and numerical positions are validated; untrusted text from IVAO is not embedded in the executed code.

The proof-of-concept uses FlightGear's `add-aiobject` FGCommand with a `static` object and `AI/Aircraft/A320/A320-default.xml` model. It attempts updates through the `/ai/models/static` property nodes. **We have not yet confirmed the runtime semantics of dynamic position updates on FlightGear 2024.1 or the model being visible.** If positions stay frozen, the native AI object type/movement mechanism must be refined in the next iteration. No fake claims of successful rendering are made.

Observe FlightGear's Property Browser under `/ai/models`, and the model near your own latitude/longitude. The bridge reports `[FG TRAFFIC]` batch activity, which confirms only command transmission, not graphical creation. Because FlightGear's `add-aiobject` is native and differs from multiplayer protocol, the UDP 5001 listener is not required.

The Phase 46 `#DP` handling and expiry are retained in the C traffic manager, although removal of already-created AI objects is not yet hooked up. Stale objects are only marked inactive in the Nasal registry. This build is limited to a 120-second diagnostic, to avoid uncontrolled accumulation of static AI models.

## Validation

`clang -std=c11 -Wall -Wextra -Werror -o altitude_host host.c ivao_fsd.c fg_traffic.c -pthread` passes under Linux. `test_fsd` and PCAP replay pass. Real macOS linking and the FlightGear Nasal command execution must be verified on the target Mac.
