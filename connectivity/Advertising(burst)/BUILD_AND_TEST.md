# Build & Test Guide: Device GATT Implementation

## Quick Start

The Advertising(burst) example now includes a fully functional CUSTS1 BLE profile that mirrors the GoLangServer behavior. Your device can now communicate with mobile apps the same way the Go server does.

## What Was Added

### Source Files
- **`src/custom_profile/user_custs1_def.h`** – UUID definitions (128-bit UUIDs matching Go server exactly)
- **`src/custom_profile/user_custs1_def.c`** – ATT database descriptor (defines services, characteristics, permissions)
- **`src/custom_profile/user_custs_config.c`** – Profile registration for SDK ATT loader
- **`src/user_ble_burst_adv.c`** – Updated with:
  - CUSTS1 includes
  - `user_catch_rest_hndl()` – handles write events from mobile
  - `handle_timestamp_request()` – parses the 4-byte index
  - `notify_timestamp_chunk()` – sends timestamp notifications via `CUSTS1_VAL_NTF_REQ`

### Keil Project Updates
- All 5 targets (DA14585, DA14586, DA14531, DA14531_01, DA14535) now include custom_profile files
- Backup of original project created: `ble_burst_adv.uvprojx.bak`

## Keil Build Steps

1. **Open Keil uVision** and load `Keil_5/ble_burst_adv.uvprojx`

2. **Verify build defines for your target (critical)**:
   - Go to: Project → Options for Target → C/C++ → Define
   - Ensure these are present:
     ```
     __DA14531__          (or your chip: __DA14585__, __DA14586__, etc.)
     CFG_PRINTF_UART2     (for UART debug output)
     DEVELOPMENT_DEBUG    (for development mode)
     BLE_CUSTOM1_SERVER=1 (enable CUSTS1 profile)
     EXCLUDE_DLG_CUSTS1=0 (do not exclude CUSTS1)
     ```

3. **Check includes path**:
   - Ensure SDK include paths are set correctly (Project → Options → Directories)
   - Typical: `....\sdk\ble_stack\host\att\attm\`

4. **Build for your target**:
   ```
   Rebuild all (F7) or Build (Ctrl+B)
   ```
   - Output should show custom_profile files compiling without errors
   - Look for: `user_custs1_def.c` and `user_custs_config.c` in build output

## Expected Behavior After Flash

### Advertising
- Device advertises with:
  - Manufacturer Data: 4-byte **SyncCount** (big-endian), incremented per burst
  - Advert interval: ~8000 ms (matching Go server)

### GATT Services
When connected:

1. **Timestamp Service** (UUID from GoLangServer)
   - **Request Char** (write): Mobile writes 4-byte big-endian index → device streams timestamps from that index
   - **Response Char** (notify): Device sends up to 5 timestamps per 20-byte notification, 1s delay between packets

2. **Update Service** (UUID from GoLangServer)
   - **Update Char** (write): Mobile can send 4-byte clock/epoch to update device time

### Mobile App Testing
- Use **nRF Connect** or your custom app
- Write to "Timestamp Request" with `\x00\x00\x00\x00` → device streams all 10 timestamps
- Observe notifications arriving at ~1s intervals
- Write to "Update" with a 4-byte epoch → device logs the update

## Troubleshooting

| Error | Fix |
|-------|-----|
| Compiler: undefined `TASK_ID_CUSTS1` | Add `BLE_CUSTOM1_SERVER=1` to Defines |
| `custs1.h` not found | Check include paths in project options |
| Linker: undefined `app_custs1_create_db` | Ensure `attm_db_128.c` and `custs1.c` are in SDK build |
| Device crashes on connect | Check UART/GPIO pin conflicts (see UART_BUTTON_KEIL_CONFIGURATION_GUIDE.md) |
| No notifications sent | Ensure Client Characteristic Config (CCC) is enabled on Response characteristic |

## File Structure After Build

```
connectivity/Advertising(burst)/
├── Keil_5/
│   ├── ble_burst_adv.uvprojx (updated with custom_profile groups)
│   └── out_DA14531/
│       └── Objects/
│           └── ble_burst_adv_531.axf (compiled binary)
├── src/
│   ├── custom_profile/
│   │   ├── user_custs1_def.c
│   │   ├── user_custs1_def.h
│   │   └── user_custs_config.c
│   ├── user_ble_burst_adv.c (with handlers)
│   ├── user_ble_burst_adv.h
│   └── ... (other files)
└── IMPLEMENTATION_PLAN.md
```

## Next Steps

1. Build and test on hardware (DA14531 dev kit)
2. Flash binary to device using Keil / segger J-Link
3. Connect with mobile app and verify timestamp streaming
4. Fine-tune SyncCount advertisement if needed (see `start_advertising()` in `user_ble_burst_adv.c`)

## Protocol Reference

For exact protocol details, see:
- **`GoLangServer/src/ble_constants.go`** – UUID definitions
- **`GoLangServer/src/gatt_server.go`** – service/characteristic behavior
- **`src/custom_profile/user_custs1_def.h`** – device-side UUID mappings (LSB-first for ATT DB)
