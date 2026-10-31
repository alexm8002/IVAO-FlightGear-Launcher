# Phase 53: FlightGear AI lifecycle

Starting from Phase 52, whose Nasal execution and AI object creation were confirmed by user logs.

* Each tick constructs the set of valid IVAO callsigns with a fresh FSD position (maximum age 45 seconds).
* Reconciles existing FlightGear `/ai/models/aircraft` nodes, creating or updating objects as before.
* For existing `IVAO-...` aircraft absent from the current active set, asks FlightGear's AI manager to remove the object by its `id` property using `fgcommand("remove-aiobject", props.Node.new({id:...}))`.
* Never deletes `/ai/models` nodes directly or removes aircraft from other sources.
* Adds `/sim/ivao-bridge/last-removed` as a per-batch counter; `last-errors` includes missing id/removal command failure.

CAUTION: the precise `remove-aiobject` ID argument is version-specific and its behavior still requires a real FlightGear test. `last-removed` is an fgcommand success count, not an independently verified C++ object destruction count. Confirm disappearance of `IVAO-...` in the property browser and visually before relying on it.

Launch/build as Phase 52 (AI Telnet TCP port 5503).
