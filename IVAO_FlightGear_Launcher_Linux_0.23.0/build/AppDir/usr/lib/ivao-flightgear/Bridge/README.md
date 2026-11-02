# Phase 44 - FSD traffic integrated in the C host

Base: working phase 35 (no CSL, no XPMP2 graphics). The new `ivao_fsd.c/.h` are compiled directly into **altitude_host**. No Python traffic processes.

## Build (macOS)

```sh
./build_macos.sh '/path/to/ivao_pilot.xpl'
```

## Verify a previously captured PCAP (no Core startup needed)

```sh
./altitude_host --traffic-pcap-only /path/to/traffic-private.pcap
```

## Run the usual bridge while collecting inbound FSD packets passively

```sh
./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 2>&1 | tee phase44.log
```

The capture is read-only, uses macOS `libpcap` dynamically and never records outgoing credentials. Access to `/dev/bpf*` may be restricted: the diagnostic prints an error when access is denied. **Do not run the whole IVAO Core as root**. Grant capture permissions to your regular user if desired, or use the offline PCAP mode.

If capture is enabled the summary prints at the end of the 120-second diagnostic. If macOS refuses capture, FlightGear telemetry and the Core still continue to operate.

For a self-contained check, use `--traffic-pcap-only` with a PCAP produced by `tcpdump` on the IVAO TCP connection. Output includes only sanitized pilot fields; authentication payloads are never printed.

## Implementation notes / limitations

- Decodes classic FSD `@N`, `@S`, `@Y` positions. Flight data state is retained in the C process, keyed by callsign; no Python traffic helper is used.
- The decoding is integrated, **but FlightGear traffic injection has NOT yet been added**. We deliberately do not repeat the unsuccessful 232-byte multiplayer approach from phase 39-41.
- Heading, pitch, roll and aircraft type are not present in the validated FSD subset; these are not fabricated.
- Live capture has not been tested on a real macOS installation; its availability depends on libpcap/BPF privileges. IPv4 Ethernet/loopback and classic PCAP input are supported. Interleaved concurrent TCP streams, unusual encapsulations, and out-of-order TCP segments need further hardening.
- Existing COM1/COM2/XPDR, telemetry, UI and network behaviour are unchanged from phase 35.


## macOS build fix

This patched release replaces feature-macro-dependent `INADDR_LOOPBACK` with `inet_addr("127.0.0.1")` and removes the nonportable `RTLD_NOLOAD` flag from the shim lookup. Existing IVAO/FlightGear behavior is unchanged.


## Phase 45 — live FSD capture diagnostics

- Logs `[FSD C] LIVE` every 5 seconds with packets received, server TCP data segments, positions and rejected frames.
- Logs every decoded pilot update immediately.
- Waits for the libpcap worker to terminate before printing the final aircraft list.
- If `packets=0`, the selected interface is not seeing packets; check the actual active network interface (`route -n get 57.128.162.80 | grep interface`) and use it instead of `en0`.
- If `packets>0` but `server_segments=0`, inspect the active interface / linktype, or check that TCP from IVAO port 6809 is visible.
- This phase does not yet render AI aircraft in FlightGear.


## Phase 46 - corrections after the live IVAO test

The phase 45 log demonstrates working live FSD capture: 46 server TCP payload segments and 19 pilot updates across 6 pilots. No new capture method is necessary.

- Automatically remove a pilot after a server `#DP:CALLSIGN` disconnect message.
- Expire pilots that have not sent a position for 90 seconds (live mode only).
- Correct TCP payload boundaries using IPv4 Total Length to ignore link-layer padding.
- Expose delete/expiration counters in live diagnostics.
- The IVAO Core's `interpolator not usable` warning is internal to the plugin and is not fixed by this module; the bridge continues to receive positions.

Compile and run with the same phase 45 commands. No Python runtime process is added.


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

## Phase 48
See README_PHASE48.md. AI object creation was not confirmed by Phase 47 logs; Phase 48 adds request/response checks.


Voir README_PHASE49.md pour la vérification Telnet ajoutée.
