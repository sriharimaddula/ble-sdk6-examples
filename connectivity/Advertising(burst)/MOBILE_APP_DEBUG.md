# Mobile App Integration - Debug Guide

## Issue: Alarm/Reminder Data NOT Being Written to Device

### Observed Behavior (from device logs):
```
[GATT] Handshake service registered at handle 23
[GATT] Timestamp REQUEST service registered at handle 27
[GATT] Timestamp RESPONSE service registered at handle 31
[GATT] Update service registered at handle 36

[DEBUG] Handshake write handle: 25
[DEBUG] Timestamp REQUEST write handle: 29
[DEBUG] Timestamp RESPONSE notify handle: 33
[DEBUG] Update write handle: 38

[GATT] Read request handle=6        ← Service discovery
[GATT] Read request handle=36       ← Service discovery

user_on_disconnect                  ← NO WRITES! ❌
```

**Result**: Mobile app connects, discovers services, but **NEVER writes alarm data**.

---

## Root Cause Analysis

### Code Review: `DroptorSync.kt`

#### Problem 1: Missing Work Type Enum
```kotlin
enum class PendingWorkType {
    TIMESTAMP_SYNC,
    UPDATE_DROPTOR,
    NONE,
    // ❌ MISSING: CONFIGURE
}
```

The `configure()` function (line 370) doesn't set a `pendingWorkType`, so when it calls `connect()`, the device connects but `onServicesDiscovered()` may execute the wrong operation.

#### Problem 2: `configure()` Flow
```kotlin
fun configure(minutesOfDay: IntArray) {
    val packets = mutableListOf<ByteArray>()
    // ... build packets ...
    pendingWrites = packets.iterator()
    
    if (currConnState == BluetoothProfile.STATE_CONNECTED) {
        startSequentialWrite()  // ✓ Works if already connected
    } else {
        connect()  // ❌ Sets pending but doesn't set pendingWorkType!
    }
}
```

#### Problem 3: `onServicesDiscovered()` Logic
```kotlin
override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
    when (pendingWorkType) {
        PendingWorkType.TIMESTAMP_SYNC -> {
            startTimebookSync()  // ← Runs instead of configure!
        }
        PendingWorkType.UPDATE_DROPTOR -> {
            updateDroptor(gatt)
        }
        else -> {
            // Only runs if pendingWorkType == NONE
            pendingWrites?.let {
                if (it.hasNext()) {
                    startSequentialWrite()
                }
            }
        }
    }
}
```

**Issue**: If the user saves alarms while `pendingWorkType` is still `TIMESTAMP_SYNC` (from previous scan), the device will try to sync timestamps instead of writing configure packets!

---

## Fix Options

### Option A: Add CONFIGURE Work Type (Recommended)

```kotlin
enum class PendingWorkType {
    TIMESTAMP_SYNC,
    UPDATE_DROPTOR,
    CONFIGURE,  // ← ADD THIS
    NONE,
}

fun configure(minutesOfDay: IntArray) {
    val packets = mutableListOf<ByteArray>()
    // ... build packets ...
    pendingWrites = packets.iterator()
    pendingWorkType = PendingWorkType.CONFIGURE  // ← ADD THIS
    
    if (currConnState == BluetoothProfile.STATE_CONNECTED) {
        startSequentialWrite()
    } else {
        connect()
    }
}

override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
    when (pendingWorkType) {
        PendingWorkType.TIMESTAMP_SYNC -> {
            pendingWorkType = PendingWorkType.NONE
            startTimebookSync()
        }
        PendingWorkType.UPDATE_DROPTOR -> {
            pendingWorkType = PendingWorkType.NONE
            updateDroptor(gatt)
        }
        PendingWorkType.CONFIGURE -> {  // ← ADD THIS
            pendingWorkType = PendingWorkType.NONE
            startSequentialWrite()
        }
        else -> {
            pendingWrites?.let {
                if (it.hasNext()) {
                    startSequentialWrite()
                }
            }
        }
    }
}
```

### Option B: Reset pendingWorkType in configure()

```kotlin
fun configure(minutesOfDay: IntArray) {
    val packets = mutableListOf<ByteArray>()
    // ... build packets ...
    pendingWrites = packets.iterator()
    pendingWorkType = PendingWorkType.NONE  // ← RESET THIS
    
    if (currConnState == BluetoothProfile.STATE_CONNECTED) {
        startSequentialWrite()
    } else {
        connect()
    }
}
```

**Caveat**: This might cause issues if a timestamp sync is pending.

---

## Service UUID Reference

Ensure `BleConstants` matches device implementation:

```kotlin
object BleConstants {
    // Handshake/Configure Service (reminders + system time)
    val CONFIGURE_SERVICE_UUID = UUID.fromString("0000180D-0000-1000-8000-00805f9b34fb")
    
    // Timestamp Request Service (write from_index)
    val TS_REQ_SERVICE_UUID = UUID.fromString("17b96791-4c66-4c01-9633-31b191590001")
    
    // Timestamp Response Service (receive notifications)
    val TS_RES_SERVICE_UUID = UUID.fromString("17b96792-4c66-4c01-9633-31b191590002")
    
    // Update Service (clock sync)
    val UPDATE_SERVICE = UUID.fromString("16b96793-4c66-4c01-9633-31b191590003")
    
    // Client Characteristic Configuration Descriptor
    val CCC_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}
```

---

## Testing Checklist

### 1. Add Debug Logging
```kotlin
override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
    Log.d(TAG, "=== onServicesDiscovered ===")
    Log.d(TAG, "pendingWorkType: $pendingWorkType")
    Log.d(TAG, "pendingWrites hasNext: ${pendingWrites?.hasNext()}")
    
    val configService = gatt.getService(BleConstants.CONFIGURE_SERVICE_UUID)
    Log.d(TAG, "ConfigService found: ${configService != null}")
    if (configService != null) {
        Log.d(TAG, "ConfigService UUID: ${configService.uuid}")
        Log.d(TAG, "ConfigService chars: ${configService.characteristics.size}")
    }
    
    // ... rest of function
}

fun configure(minutesOfDay: IntArray) {
    Log.d(TAG, "=== configure() called ===")
    Log.d(TAG, "minutesOfDay: ${minutesOfDay.toList()}")
    Log.d(TAG, "currConnState: $currConnState")
    Log.d(TAG, "pendingWorkType (before): $pendingWorkType")
    
    // ... build packets ...
    
    Log.d(TAG, "Total packets: ${packets.size}")
    Log.d(TAG, "First packet: ${packets[0].toHexString()}")
}
```

### 2. Expected Flow
```
User clicks Save Alarms
  ↓
App calls droptorSync.configure([480, 720])  // Example: 8:00am, 12:00pm
  ↓
configure() builds packets:
  - Packet 0: [00 00 00 02] [67 65 F3 20]  // 2 alarms, timestamp
  - Packet 1: [00 00 01 E0] [00 00 02 D0]  // 480, 720 (minutes)
  ↓
Sets pendingWrites, pendingWorkType
  ↓
Calls connect() or startSequentialWrite()
  ↓
onServicesDiscovered() → calls startSequentialWrite()
  ↓
Writes to CONFIGURE_SERVICE_UUID characteristic
  ↓
Device logs should show:
  [HANDSHAKE] Write received to handle 25, length=8
  [HANDSHAKE] Data: 00 00 00 02 67 65 F3 20
  [HANDSHAKE] Reminder count: 2, System time: 1734710048
```

### 3. Common Issues

#### Issue: "Cannot write configure packets, config service not found!"
**Cause**: `BleConstants.CONFIGURE_SERVICE_UUID` doesn't match device UUID
**Fix**: Verify UUID is `0000180D-0000-1000-8000-00805f9b34fb`

#### Issue: No write events, but connection succeeds
**Cause**: `pendingWorkType` not set correctly, wrong operation executed
**Fix**: Implement Option A or B above

#### Issue: Writes happen but nothing logged on device
**Cause**: Wrong handle being written to
**Fix**: Verify mobile app is writing to handle 25 (service + 2)

---

## Device-Side Verification

Device logs will show when data is received:

### Success Logs:
```
[HANDSHAKE] Write received to handle 25, length=8
[HANDSHAKE] Data: 00 00 00 02 67 65 F3 20
[HANDSHAKE] Received 8 bytes (first_packet=1)
[HANDSHAKE] Reminder count: 2, System time: 1734710048
[HANDSHAKE] Received 8 bytes (first_packet=0)
[HANDSHAKE] Reminder 1: 08:00
[HANDSHAKE] Reminder 2: 12:00
[HANDSHAKE] Complete - 2 reminders stored
```

### Failure Logs:
```
[DEBUG] Handshake write handle: 25
[GATT] Read request handle=X  ← Only reads, no writes
user_on_disconnect
```

---

## Quick Test

To verify the mobile app is attempting writes at all:

```kotlin
override fun onCharacteristicWrite(
    gatt: BluetoothGatt,
    characteristic: BluetoothGattCharacteristic,
    status: Int
) {
    Log.d(TAG, "=== onCharacteristicWrite ===")
    Log.d(TAG, "Service UUID: ${characteristic.service.uuid}")
    Log.d(TAG, "Char UUID: ${characteristic.uuid}")
    Log.d(TAG, "Status: $status")
    
    // ... rest of function
}
```

If this callback **never fires** after clicking Save, the write is not being initiated.

---

## Contact

If issues persist after implementing fixes:
1. Share complete logcat output (filter by "DroptorSync")
2. Share device UART logs showing service registration
3. Confirm `BleConstants` UUIDs match exactly
