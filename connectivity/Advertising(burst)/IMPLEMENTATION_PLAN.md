# Implementation Plan: Device-side GATT + Advert Sync for Advertising(burst)

Goal: Implement BLE GATT services on the DA14531 device to match the behaviour of
the GoLangServer used for mobile app testing. The device should advertise a 4-byte
big-endian SyncCount in manufacturer data, accept writes to request timestamp data,
and stream timestamp notifications in 20-byte packets (5 timestamps per notification).

Scope (prototype):
- Add UUID constants to the example header (placeholders for now)
- Add an embedded timestamps array as prototype data
- Add request handler stubs and a timer-driven chunk sender (logs to UART for now)
- Add an actionable plan and next steps for full SDK-based GATT registration

Files to change (prototype):
- `src/user_ble_burst_adv.h` -- add UUID/macros and function prototypes
- `src/user_ble_burst_adv.c` -- add timestamp data array and handler stubs
- `IMPLEMENTATION_PLAN.md` (this file)

Full implementation (next phase):
1. Create custom ATT DB entries using `attm_db_128` for the timestamp request/response
   and update services (follow examples under `features/*/src/custom_profile`).
2. Implement profile configuration and message handlers (CUSTS1-style) and expose
   notification APIs using the SDK-provided notify helper.
3. Add persistence (read timestamps from a file or external storage) if needed.
4. Test with mobile app (nRF Connect or your app) to validate packetization and timing.

Next actions after this prototype commit:
- Implement full ATT DB and profile files matching the UUIDs in `ble_constants.go`
- Add unit tests and a small mobile test script

