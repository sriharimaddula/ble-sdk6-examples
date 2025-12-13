# BLE Droptor Protocol - Implementation Status

**Version**: 1  
**Last updated**: 28 September 2025  
**Device**: Eye Droptor (DA14531 BLE)  
**Mobile**: Android App

---

## Protocol Overview

The Eye Droptor is a BLE peripheral that records timestamps when eye drops are administered. The Android app monitors advertising data and synchronizes timestamp logs.

---

## Service Architecture

### 1. **Handshake Service** ❌ NOT IMPLEMENTED
- **Purpose**: Configure device with reminders and set system time
- **UUID**: `0x16B96793-4C66-4C03-9633-31B191590003` (from ble_constants.go)
- **Characteristic**: Write-only
- **Data Format**:
  ```
  First Packet (8 bytes):
    [UInt32 BE] - Size of reminders
    [UInt32 BE] - Current system time in seconds (epoch)
  
  Subsequent Packets (max 20 bytes):
    [UInt32 BE...] - Timestamps (max 5 per packet)
  ```

**Status**: ⚠️ **MISSING** - Need to implement this service with reminder storage

---

### 2. **Timestamp Request Service** ✅ IMPLEMENTED
- **Purpose**: Client requests timestamps starting from a specific index
- **Service UUID**: `0x17B96791-4C66-4C01-9633-31B191590001`
- **Characteristic UUID**: `0x17B96794-4C66-4C04-9633-31B191590004`
- **Properties**: Write
- **Implementation**: 
  - Handle: `timestamp_service_start_handle + 2` (dynamically allocated)
  - Handler: `handle_timestamp_request(uint32_t from_index)`
  - Message: `GATTC_WRITE_REQ_IND` → sends count notification + schedules chunks

**Data Format** (Write):
```
[UInt32 BE] - Android's SyncCount (from_index)
```

---

### 3. **Timestamp Response Service** ✅ IMPLEMENTED
- **Purpose**: Device notifies client with timestamp data
- **Service UUID**: `0x17B96791-4C66-4C01-9633-31B191590001` (same as request)
- **Characteristic UUID**: `0x17B96795-4C66-4C05-9633-31B191590005`
- **Properties**: Notify
- **Implementation**:
  - Handle: `timestamp_service_start_handle + 4`
  - Function: `notify_timestamp_chunk()`
  - CCC Descriptor: Handle + 1 (for enabling notifications)

**Response Format**:
```
First Header Packet (4 bytes):
  [UInt32 BE] - Remaining timestamps count

Subsequent Packets (max 20 bytes, 5 timestamps):
  [UInt32 BE] - Timestamp 1
  [UInt32 BE] - Timestamp 2
  [UInt32 BE] - Timestamp 3
  [UInt32 BE] - Timestamp 4
  [UInt32 BE] - Timestamp 5
```

**Timing**: 1-second delay between packets (matches GoLang server)

---

### 4. **Update Service** ⚠️ PARTIALLY IMPLEMENTED
- **Purpose**: Client writes clock time or other updates
- **Service UUID**: `0x16B96793-4C66-4C03-9633-31B191590003`
- **Characteristic UUID**: `0x17B96795-4C66-4C05-9633-31B191590017`
- **Properties**: Write
- **Implementation**: 
  - Handler exists but service NOT REGISTERED dynamically
  - RTC update code disabled (no rtc.c in build)

**Data Format**:
```
[UInt32 BE] - System time in seconds (epoch)
```

**Status**: Code exists but service registration commented out

---

## Protocol Flow

### Phase 1: Handshake ❌ NOT IMPLEMENTED
```
1. Device enters advertising mode
2. Android connects to device
3. Android writes:
   - First packet: [reminder_count | system_time_seconds]
   - Subsequent packets: reminder timestamps
4. Android disconnects
5. Android enters Watch Mode
```

**Current Gap**: Device does not have HandshakeService registered. Need to:
- Add HandshakeService to `register_custom_services()`
- Store reminders in device memory
- Process reminder data in write handler

---

### Phase 2: Watch Mode ✅ IMPLEMENTED
```
1. Device advertises with SyncCount in manufacturer data
2. Android monitors advertising packets
3. When SyncCount increments → trigger sync
```

**Implementation**:
- `advert_count` (uint32_t) = SyncCount
- Manufacturer data: `{0xFF, 0x4C, 0x00, count_byte}`
- Updated in `update_and_advertise()` on each burst
- Big-endian encoding: `(advert_count >> 24) & 0xFF` → MSB

**Status**: ✅ Working - counter increments and advertises correctly

---

### Phase 3: Synchronization ✅ IMPLEMENTED
```
1. Android enables notifications on TimestampResponseService
2. Android writes SyncCount to TimestampRequestService
3. Device sends:
   a. Header packet: [remaining_count]
   b. Timestamp chunks: 5 timestamps per notification (20 bytes)
   c. 1-second delay between chunks
4. Android saves timestamps to log book
5. Android disconnects
```

**Implementation**:
- Service registered: `register_custom_services()` in `user_on_connection()`
- Write handler: `GATTC_WRITE_REQ_IND` → `handle_timestamp_request()`
- Count response: Immediate 4-byte notification
- Chunk streaming: `notify_timestamp_chunk()` with 1s timer
- Data source: `prototype_timestamps[]` (hardcoded, 10 entries)

**Status**: ✅ Coded and ready - needs mobile app testing

---

## Data Encoding

### Big-Endian UInt32
All multi-byte integers use **big-endian** (network byte order):
```c
// Encode
buf[0] = (value >> 24) & 0xFF;  // MSB
buf[1] = (value >> 16) & 0xFF;
buf[2] = (value >> 8) & 0xFF;
buf[3] = value & 0xFF;          // LSB

// Decode
uint32_t value = ((uint32_t)buf[0] << 24) |
                 ((uint32_t)buf[1] << 16) |
                 ((uint32_t)buf[2] << 8)  |
                 ((uint32_t)buf[3]);
```

---

## Implementation Checklist

### ✅ Completed
- [x] Advertising with SyncCount (manufacturer data)
- [x] Advertising counter increment
- [x] BLE connection/disconnection handlers
- [x] Dynamic GATT service registration (TimestampService)
- [x] Timestamp request handler (GATTC_WRITE_REQ_IND)
- [x] Timestamp response notifications (GATTC_SEND_EVT_CMD)
- [x] Count-first protocol (4-byte header)
- [x] Chunked streaming (5 timestamps per packet)
- [x] 1-second pacing between chunks
- [x] Big-endian encoding for all UInt32 values
- [x] GATT write confirmation (GATTC_WRITE_CFM)

### ⚠️ Partial / Disabled
- [ ] Update Service registration (code exists, not registered)
- [ ] RTC clock update (code disabled, needs rtc.c in build)
- [ ] Production timestamp storage (using prototype array)

### ❌ Missing / TODO
- [ ] **Handshake Service** (critical for reminder setup)
- [ ] Reminder data structure and storage
- [ ] Reminder alarm/timer mechanism
- [ ] Persistent timestamp storage (currently hardcoded array)
- [ ] Second service registration (Update Service)
- [ ] Flash memory integration for timestamps
- [ ] Low-power/sleep modes between drops
- [ ] Battery monitoring
- [ ] Drop detection mechanism (accelerometer/button?)

---

## File Structure

### Device Code (DA14531)
```
connectivity/Advertising(burst)/
├── src/
│   ├── user_ble_burst_adv.c          # Main application logic
│   ├── user_ble_burst_adv.h          # Function declarations
│   ├── custom_profile/
│   │   ├── user_custs1_def.h         # UUID definitions, handle enum
│   │   ├── user_custs1_def.c         # ATT database (stubbed out)
│   │   └── user_custs_config.c       # Profile registration (stubbed)
│   └── config/
│       ├── user_callback_config.h    # Message handler registration
│       └── user_modules_config.h     # EXCLUDE_DLG_CUSTS1=1
├── Keil_5/
│   └── ble_burst_adv.uvprojx         # Keil project file
└── GoLangServer/
    └── src/
        ├── gatt_server.go            # Reference implementation
        ├── ble_constants.go          # UUID definitions
        └── main.go                   # Server entry point
```

### Key Variables
```c
static uint32_t advert_count = 0;                    // SyncCount
static uint32_t prototype_timestamps[10];            // Timestamp storage
static uint32_t ts_stream_idx;                       // Current streaming position
static timer_hnd ts_stream_timer_id;                 // Chunk timer
static uint16_t timestamp_service_start_handle = 0;  // Dynamic handle
```

---

## Testing Procedure

### Current Test (Timestamp Sync)
1. Flash device with latest firmware (commit: 5fe3c94f)
2. Device advertises with incrementing counter
3. Android app connects to device
4. Expected logs:
   ```
   [GATT] Registering custom services...
   [GATT] Service added: start_handle=23, status=0x00
   [GATT] Timestamp service registered at handle 23
   ```
5. Android enables notifications on Response characteristic
6. Android writes `00 00 00 00` to Request characteristic
7. Expected device logs:
   ```
   [GATT] Write event received: handle=25, length=4
   [GATT] Timestamp Request: index=0x00000000 (0)
   [TIMESTAMP] Sent count notification: 10 remaining
   [NOTIFY] Sending chunk: indices [0-4], 5 timestamps, 20 bytes
   [NOTIFY] Sending chunk: indices [5-9], 5 timestamps, 20 bytes
   ```
8. Android receives 10 timestamps over 2 notifications

### Future Test (Full Protocol)
1. Add Handshake Service implementation
2. Android writes reminders + system time
3. Device stores reminders and starts monitoring
4. Device detects drop event → increments SyncCount
5. Android sees SyncCount change → initiates sync
6. Full timestamp synchronization

---

## Protocol Compliance

| Feature | GoLang Server | DA14531 Device | Status |
|---------|---------------|----------------|--------|
| Advertising with SyncCount | ✅ | ✅ | Match |
| Handshake Service | ✅ | ❌ | Missing |
| Timestamp Request (Write) | ✅ | ✅ | Match |
| Count-First Response | ✅ | ✅ | Match |
| Chunked Notifications (5 ts) | ✅ | ✅ | Match |
| 1-Second Pacing | ✅ | ✅ | Match |
| Big-Endian Encoding | ✅ | ✅ | Match |
| Update Service | ✅ | ⚠️ | Partial |

---

## Next Steps (Priority Order)

### High Priority
1. **Test current timestamp sync with mobile app**
   - Verify service discovery
   - Verify write to Request characteristic
   - Verify count + timestamp notifications received

2. **Implement Handshake Service**
   - Add to `register_custom_services()`
   - Create reminder storage structure
   - Parse and store reminder data

3. **Replace prototype timestamps**
   - Integrate with persistent storage
   - Add timestamp recording on drop detection

### Medium Priority
4. **Add Update Service registration**
   - Second service in `register_custom_services()`
   - Enable RTC update functionality

5. **Optimize power consumption**
   - Sleep modes between advertising bursts
   - Wake on drop detection

### Low Priority
6. **Add error handling and edge cases**
7. **Implement firmware update mechanism**
8. **Add device reset/factory reset functionality**

---

## Contact / References
- **Protocol Document**: Provided by Mobile App Developer (28 Sept 2025)
- **Reference Server**: GoLangServer (gatt_server.go)
- **SDK**: Renesas DA145xx SDK 6.0.24.1464
- **Git Branch**: `feature/adv-burst-log`
- **Latest Commit**: 5fe3c94f (timestamp count-first fix)
