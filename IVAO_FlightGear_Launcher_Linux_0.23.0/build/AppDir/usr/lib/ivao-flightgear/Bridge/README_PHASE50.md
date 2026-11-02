# Phase 50 - dedicated Telnet interface for native FlightGear AI traffic

Phase49 logs: TCP connection to 127.0.0.1:5502 succeeded but `get /sim/time/elapsed-sec` produced 0 bytes, so AI Nasal updates were correctly blocked. Pilot Core / FSD positions still worked.

The AI transport now defaults to a SEPARATE telnet socket 127.0.0.1:5503. Radio/XPDR commands remain unchanged on 5502.

FlightGear must listen on BOTH channels, 5502 for the original radio commands and 5503 for AI, using its Telnet property protocol. Add `--telnet=5503 --allow-nasal-from-sockets` to your existing FlightGear launch arguments, retaining `--telnet=5502` if your FlightGear version permits repeated Telnet options. If it doesn't, use `--fg-ai-port 5502` to select the existing port, but phase49 proved the current 5502 endpoint doesn't answer our read probe. The issue may be the service type, start flags or protocol mode on that endpoint. Never expose Nasal-enabled Telnet ports publicly.

Build (macOS): `./build_macos.sh /absolute/path/to/ivao_pilot.xpl`
Run:
`./altitude_host "$PWD/ivao_pilot_test.xpl" --flightloop-diagnostic 120 --traffic-interface en0 --fg-ai --fg-ai-port 5503`

Success requires `[FG TRAFFIC] TELNET_REPLY`; `TELNET_PROBE_FAILED` still means Nasal was not sent. 3D rendering is NOT confirmed by this build.

Also filters the obviously bogus location near latitude/longitude 0,0 present in the phase49 log.
