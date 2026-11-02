# Phase 60 Fix 3 - FlightGear Telnet recovery

The FSD capture and IVAO online state are functional in Fix 2. The log reveals `GET reply empty on TCP 5503; Nasal not submitted`, so the Nasal aircraft creation script was never sent.

This fix restores the known working Phase 57 Telnet read timeouts in `fg_traffic.c`:
- initial greeting: 250 ms (was 100)
- initial `get /position/latitude-deg`: 900 ms (was 300)
- Nasal interpreter: 700 ms (was 150)
- result queries: 1200 ms (was 400)

All other Phase 60 features are retained: session detection, deletion handling, and position extrapolation. 2 Hz is best effort, not guaranteed because Telnet transactions may take longer.

Expected logs: `[FG TRAFFIC] batch=... aircraft=...`; `last-created` should become nonzero when FlightGear accepts new aircraft.
