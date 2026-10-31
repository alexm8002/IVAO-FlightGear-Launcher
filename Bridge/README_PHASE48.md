# Phase 48 - FlightGear AI acknowledgement diagnostics

Phase 47 logged successful `send()` calls as if AI objects were created. That is not proof of execution. Phase 48 keeps the existing live FSD C decoder and replaces the traffic Telnet transport with **readback diagnostics**.

- Sends `nasal`, multiline code, `##EOF##` using the stock FlightGear protocol.
- Runs `get /sim/ivao-bridge/last-seen`, `get /sim/ivao-bridge/last-created`, `get /sim/ivao-bridge/last-errors` and `get /ai/models/count` **after the Nasal block**.
- Reads returned Telnet text with a bounded, short wait and logs only the diagnostic query lines or errors. No raw capture or private FSD data is logged from the Telnet response.
- Corrected log label from "Sent native AI Nasal updates" to "Submitted Nasal ... response_bytes"; creation and 3D rendering are **not confirmed** until FlightGear acknowledges them.
- Existing FSD decoder, Pilot Core, UDP and radios are unchanged.

Compile and run like phase 47 with `--fg-ai --traffic-interface en0`. FlightGear must be started with `--telnet=5502 --allow-nasal-from-sockets`, with Telnet exposed only on localhost. See `[FG TRAFFIC] Reply:` lines in `phase48.log`.

**Known limitation:** the AI model path and model creation/movement still require a real FlightGear execution test. The diagnostic now makes errors visible instead of treating a successful `send()` as a successful `add-aiobject`.
