# Phase 63 - IVAO model matching

Base: phase 62 compact Nasal XML verified in FlightGear. No changes to Telnet timings or Nasal framing.

Model info: FSD `-PDCALLSIGN:RECEIVER:ICAO4+AIRLINE` parsed and cached, also when received before first @ position.

Build now indexes XML files found in `$FG_ROOT/AI/Aircraft` (default `~/Library/Application Support/FlightGear/fgdata_2024_1/AI/Aircraft`) into `fg_models.tsv`.

Matching: exact ICAO4 -> preferred matching airline filename, fallback exact ICAO4 generic XML, final fallback known-working A380 Air France.

Caveats: FlightGear XML naming varies; indexed models need ICAO code as the first 4 basename characters. Some families differ from IVAO ICAO4 or use nonstandard XML names. Existing live aircraft keep their already loaded model until despawn/restart. Use a fresh FlightGear session to test.

Disable **generated traffic**, but preserve the AI models: add `--disable-ai-traffic` to the FlightGear launch command. Do not pass `--disable-ai-models`. Disable separately loaded AI scenarios if present. The bridge only removes callsigns with `IVAO-` prefix.
