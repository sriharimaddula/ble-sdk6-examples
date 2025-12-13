# EyeDroptor BLE Device - Project Plan

## Overview

**Product**: EyeDroptor - Smart Eye Drop Reminder Device  
**Platform**: Renesas DA14531 + SDK6  
**Mobile App**: Published Android app (cannot modify easily)

## Product Concept

### What is EyeDroptor?
A smart eye drop bottle holder that:
1. **Stores medication reminders** prescribed by doctors
2. **Alerts users** via LED flash or buzzer when it's time to take drops
3. **Works offline** - reminders stored in device RTC, works without phone connection
4. **Tracks usage** - Records when drops are taken using IMU (accelerometer) data
5. **Syncs with mobile** - Updates drop-taken timestamps and IMU data to mobile app for analysis

### User Workflow
1. **Doctor Appointment**: Doctor prescribes eye drops (e.g., "3 times daily at 8AM, 2PM, 8PM")
2. **Mobile Setup**: User pairs EyeDroptor device with mobile app
3. **Configure Reminders**: User enters 2-10 reminder times in mobile app
4. **Sync to Device**: Mobile app sends reminders via BLE to device
5. **Daily Use**: Device alerts at scheduled times, user administers drops
6. **Usage Tracking**: Device detects drop administration via IMU (tilt/shake detection)
7. **Data Sync**: Next time user opens app, device sends usage history back
8. **Medical Review**: Doctor can review adherence data from app

---

## Current Project Phase: **BLE Protocol Implementation**

### Goal
Implement complete BLE communication protocol so mobile app can:
- ✅ Discover and connect to device
- ❌ **Send reminders to device** ← CURRENT BLOCKER
- ⚠️ Sync timestamps back to mobile (not tested yet)
- ⚠️ Update device clock (not tested yet)

---

## BLE Protocol Specification

### Services

#### 1. Handshake/Configure Service
- **UUID**: `0000180D-0000-1000-8000-00805f9b34fb`
- **Characteristic UUID**: `0000181D-0000-1000-8000-00805f9b34fb`
- **Properties**: Write With Response
- **Purpose**: Mobile writes reminder schedule to device

**Data Format** (Big-Endian):
```
First Packet (8 bytes):
  [0-3]: UInt32 BE reminder_count  (number of reminders, max 10)
  [4-7]: UInt32 BE system_time     (Unix epoch timestamp)

Subsequent Packets (20 bytes each):
  [0-3]:   UInt32 BE reminder_1_minutes_since_midnight
  [4-7]:   UInt32 BE reminder_2_minutes_since_midnight
  [8-11]:  UInt32 BE reminder_3_minutes_since_midnight
  [12-15]: UInt32 BE reminder_4_minutes_since_midnight
  [16-19]: UInt32 BE reminder_5_minutes_since_midnight
  (max 5 reminders per packet)
```

**Example**: 2 reminders at 8:00 AM (480 mins) and 12:00 PM (720 mins)
```
Packet 1: 00 00 00 02  67 65 F3 20     // 2 reminders, time=1734710048
Packet 2: 00 00 01 E0  00 00 02 D0     // 480, 720 minutes
```

#### 2. Timestamp Request Service
- **UUID**: `17b96791-4c66-4c01-9633-31b191590001`
- **Characteristic UUID**: `17b96794-4c66-4c01-9633-31b191590004`
- **Properties**: Write With Response
- **Purpose**: Mobile requests timestamp history from specific index

**Data Format**:
```
[0-3]: UInt32 BE from_index  (start position in timestamp array)
```

#### 3. Timestamp Response Service
- **UUID**: `17b96792-4c66-4c01-9633-31b191590002`
- **Characteristic UUID**: `17b96795-4c66-4c01-9633-31b191590005`
- **Properties**: Notify
- **Purpose**: Device sends timestamp data to mobile

**Data Format** (Count-First Protocol):
```
First Notification (4 bytes):
  [0-3]: UInt32 BE total_count  (how many timestamps to expect)

Subsequent Notifications (20 bytes each):
  [0-3]:   UInt32 BE timestamp_1  (Unix epoch)
  [4-7]:   UInt32 BE timestamp_2
  [8-11]:  UInt32 BE timestamp_3
  [12-15]: UInt32 BE timestamp_4
  [16-19]: UInt32 BE timestamp_5
  (5 timestamps per packet, max)
```

#### 4. Update Service (Clock Sync)
- **UUID**: `16b96793-4c66-4c01-9633-31b191590003`
- **Characteristic UUID**: `17b96795-4c66-4c01-9633-31b191590005`
- **Properties**: Write With Response
- **Purpose**: Mobile updates device's system clock

**Data Format**:
```
[0-3]: UInt32 BE system_time  (Unix epoch timestamp)
```

---

## Current Implementation Status

### ✅ Completed (Device Side)

1. **Service Registration**: All 4 services dynamically registered via GATTM API
2. **Handshake Handler**: `handle_handshake_write()` with multi-packet parsing
3. **Timestamp Request Handler**: Parses 4-byte index, triggers response
4. **Timestamp Response Handler**: Sends count-first, then chunks of 5 timestamps
5. **Update Handler**: Stores system_time in device_state
6. **Write Confirmations**: All GATTC_WRITE_CFM responses sent
7. **Read Handler**: Responds to 0x0E11 read requests during service discovery
8. **Debug Logging**: Comprehensive printf logs for diagnosis

### ❌ Current Blocker: Mobile App Not Writing

**Symptoms:**
- Mobile app connects successfully ✓
- Services discovered (0x0E11 read requests handled) ✓
- User adds reminders in mobile UI ✓
- User clicks "Save" → shows "Hang ON" alert ✓
- Mobile app writes first packet... **but `onCharacteristicWrite()` never fires** ❌
- Mobile app hangs forever waiting for write confirmation
- Device receives **NO HANDSHAKE WRITE EVENTS** (no [HANDSHAKE] logs)

**Mobile App Code Analysis** (DroptorSync.kt):
```kotlin
fun configure(minutesOfDay: IntArray) {
    // Builds packets correctly (verified)
    pendingWrites = packets.iterator()
    
    if (currConnState == BluetoothProfile.STATE_CONNECTED) {
        startSequentialWrite()  // Goes here if already connected
    } else {
        connect()
    }
}

private fun startSequentialWrite() {
    currGatt?.apply {
        val configService = getService(BleConstants.CONFIGURE_SERVICE_UUID)
        if (configService != null) {
            write(this, configService.characteristics[0], pendingWrites?.next()!!)
        } else {
            Log.e(TAG, "Cannot write configure packets, config service not found!")
        }
    }
}

override fun onCharacteristicWrite(...) {
    // Should fire after device sends GATTC_WRITE_CFM
    // But NEVER FIRES!
    if (serviceUuid == BleConstants.CONFIGURE_SERVICE_UUID) {
        if (pendingWrites?.hasNext() == true) {
            write(gatt, characteristic, pendingWrites?.next()!!)  // Send next packet
        }
    }
}
```

**Hypotheses:**
1. ~~UUID mismatch~~ - Checked, our UUIDs match GoLang server ✓
2. ~~Write confirmation missing~~ - Verified GATTC_WRITE_CFM sent ✓
3. ~~Wrong write type~~ - Verified WRITE_TYPE_DEFAULT (with response) ✓
4. ⚠️ **Characteristic not discoverable?** - Mobile accesses `characteristics[0]`
5. ⚠️ **Handle mismatch?** - Need to verify mobile writes to correct handle
6. ⚠️ **Permission issue?** - PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE) should be correct
7. ⚠️ **ATT MTU issue?** - Default 23 bytes (20 data + 3 ATT header) should work

### Debug Strategy

**Next Steps:**
1. **Rebuild with new debug logs** showing:
   - Write handle expected vs received
   - Write confirmation sent confirmation
2. **Test with mobile app** and capture:
   - Device UART logs showing writes (or lack thereof)
   - Android logcat filtered by "DroptorSync"
3. **Compare with GoLang server**:
   - What does working server do differently?
   - Check GoLang server logs for comparison

---

## Future Phases (Post-BLE Protocol)

### Phase 2: RTC Integration
- Configure DA14531 RTC with reminder times
- Convert minutes-since-midnight to RTC alarm registers
- Implement RTC interrupt handler

### Phase 3: Alert System
- LED flash pattern (frequency, duration, intensity)
- Buzzer pattern (beep sequence)
- Power management (low power between alerts)

### Phase 4: IMU Integration
- Detect tilt/shake indicating drop administration
- Timestamp drop-taken events
- Store in circular buffer for sync

### Phase 5: Data Persistence
- Store reminder schedule in non-volatile memory
- Store drop-taken history
- Handle power loss recovery

### Phase 6: Production Features
- Battery level monitoring
- Low battery alerts
- Firmware update over BLE (DFU)

---

## Device Specifications

### Hardware
- **MCU**: Renesas DA14531 (BLE 5.1, ARM Cortex-M0+)
- **Sensors**: 
  - IMU/Accelerometer (mCube MC36xx or similar)
  - Optional: Temperature sensor for medication tracking
- **Outputs**:
  - LED (RGB or single color)
  - Buzzer/Speaker
- **Power**: CR2032 coin cell battery

### Software
- **SDK**: Renesas DA145xx SDK 6.0.24.1464
- **Toolchain**: Keil µVision 5, ARM Compiler v6.23
- **RTOS**: None (bare metal with SDK's kernel)

---

## Mobile App Details

### Published App
- **Package**: `in.alfaleus.droptor`
- **BLE Library**: `in.alfaleus.droptorcore`
- **Status**: **Published, cannot easily modify**
- **Scan Strategy**: Filters by MAC address (stored in preferences)
- **Connection**: Scans → Connects → Service Discovery → Operations

### BLE Constants (Expected)
```kotlin
object BleConstants {
    val CONFIGURE_SERVICE_UUID = UUID.fromString("0000180D-0000-1000-8000-00805f9b34fb")
    val TS_REQ_SERVICE_UUID = UUID.fromString("17b96791-4c66-4c01-9633-31b191590001")
    val TS_RES_SERVICE_UUID = UUID.fromString("17b96792-4c66-4c01-9633-31b191590002")
    val UPDATE_SERVICE = UUID.fromString("16b96793-4c66-4c01-9633-31b191590003")
    val CCC_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}
```

---

## Development Branch
- **Repository**: `renesas/ble-sdk6-examples`
- **Branch**: `feature/adv-burst-log`
- **Base**: `connectivity/Advertising(burst)` example

### Key Files
- `src/user_ble_burst_adv.c` - Main application, GATT handlers (1044 lines)
- `src/custom_profile/user_custs1_def.h` - UUID definitions, data structures
- `src/config/user_config.h` - Device name, advertising configuration

---

## Testing Checklist

### BLE Protocol Tests
- [ ] Mobile app discovers device by MAC address
- [ ] Services discovered correctly (4 services visible)
- [ ] Handshake write works (8-byte first packet)
- [ ] Multi-packet handshake (2+ reminder packets)
- [ ] Timestamp request triggers response
- [ ] Timestamp notifications received by mobile
- [ ] Update service syncs clock
- [ ] Write confirmations trigger Android callbacks

### Edge Cases
- [ ] 0 reminders (empty schedule)
- [ ] 1 reminder (single packet after header)
- [ ] 10 reminders (max capacity, 3 packets total)
- [ ] 11+ reminders (should truncate to 10)
- [ ] Clock sync before/after reminders
- [ ] Reconnection after power cycle
- [ ] Multiple write packets without delays

---

## Contact & Resources

### Reference Implementations
- **GoLang Server**: `connectivity/Advertising(burst)/GoLangServer/` - Working reference implementation
- **Mobile App Code**: `DroptorSync.kt` - Android BLE manager class

### Documentation
- **Protocol**: `BLE_DROPTOR_PROTOCOL.md` - Full protocol specification
- **Mobile Debug**: `MOBILE_APP_DEBUG.md` - Mobile app integration guide
- **Build Guide**: `BUILD_AND_TEST.md` - Compilation and testing instructions
