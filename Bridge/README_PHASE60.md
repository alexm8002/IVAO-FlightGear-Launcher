# Phase 60 — session-aware AI traffic and improved cadence

- Checks the IVAO plugin owned `ivaopilot/online` DataRef. No traffic is sent while offline.
- On network logout (online to offline), removes only aircraft tagged `IVAO-` using the existing Nasal reconciliation command.
- On host shutdown (including SIGINT), sends an explicit cleanup command to FlightGear.
- Traffic sends are no longer limited by the integer one-second timer; the target is up to 2 Hz using the existing loop, with Telnet round trips potentially reducing that rate. No Nasal animation timer.
- Shortens Telnet waits to reduce command-side stalls. The actual frequency depends on FlightGear replies/host load.
- Direct FSD positions, PBH and prediction capped at 4 seconds remain as in Phase 59.

Not validated in running FlightGear; verify online DataRef state and cleanup in logs. If `online` does not change after connecting, restore Phase 57 and report the log.
