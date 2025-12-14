# DA14531 RAM Optimization Strategy

## Problem Statement
DA14531 has only **48KB RAM** total, with significant portions reserved for:
- BLE stack (~20-25KB)
- Retention memory for sleep/wake (2-4KB)
- Heap for BLE message buffers (configurable)
- System overhead

**Available for application: ~15-20KB**

Currently experiencing RAM overflow (52 bytes over limit) **before implementing**:
- IMU/accelerometer code
- Button handling
- LED/buzzer control
- Flash storage
- RTC functionality

## Current RAM Audit

### user_ble_burst_adv.c
| Variable | Size | Purpose | Optimization Potential |
|----------|------|---------|------------------------|
| `adv_burst_timer_id` | 2 bytes | Advertising timer | **KEEP** - Essential |
| `adv_period_ticks` | 2 bytes | Timing tracking | **KEEP** - Essential |
| `advert_count` | 4 bytes | Debug counter | **ELIMINATE** (not needed) |
| `adv_data_buf[31]` | 31 bytes | Advertising payload | **REDUCE to [10]** (only need 5 bytes) |
| `adv_data_len` | 1 byte | Advertising length | **KEEP** |
| `device_state` | 18 bytes | Core state | **KEEP** (already optimized) |
| `ts_stream_idx` | 4 bytes | Notification state | **ELIMINATE** (feature not used yet) |
| `ts_stream_timer_id` | 2 bytes | Notification timer | **ELIMINATE** (feature not used yet) |
| `handshake_service_start_handle` | 2 bytes | GATT handle | **CONSOLIDATE** (use array) |
| `timestamp_req_service_start_handle` | 2 bytes | GATT handle | **CONSOLIDATE** (use array) |
| `timestamp_resp_service_start_handle` | 2 bytes | GATT handle | **CONSOLIDATE** (use array) |
| `update_service_start_handle` | 2 bytes | GATT handle | **CONSOLIDATE** (use array) |

**Subtotal: 72 bytes**  
**After optimization: ~30 bytes** (saves 42 bytes)

### user_custs1_def.c
| Variable | Size | Purpose | Optimization Potential |
|----------|------|---------|------------------------|
| `TSVC_REQ_UUID_128[16]` | 16 bytes | UUID (unused) | **ELIMINATE** |
| `TSVC_RESP_UUID_128[16]` | 16 bytes | UUID (unused) | **ELIMINATE** |
| `USVC_UPDATE_UUID_128[16]` | 16 bytes | UUID (unused) | **ELIMINATE** |

**Subtotal: 48 bytes**  
**After optimization: 0 bytes** (saves 48 bytes)

### Total Immediate Savings: **90 bytes** (exceeds 52-byte overflow + headroom for IMU)

## Optimization Plan

### Phase 1: Immediate Fixes (Save 90 bytes)

#### 1.1 Eliminate Unused Variables
```c
// REMOVE these from user_ble_burst_adv.c:
static uint32_t advert_count = 0;           // -4 bytes
static uint32_t ts_stream_idx = 0;          // -4 bytes  
static timer_hnd ts_stream_timer_id;        // -2 bytes
```

#### 1.2 Reduce Advertising Buffer
```c
// Change from:
static uint8_t adv_data_buf[31];            // -31 bytes
// To:
static uint8_t adv_data_buf[10];            // Only need 5 bytes data + header
```
**Savings: 21 bytes**

#### 1.3 Consolidate Service Handles
```c
// Change from 4 separate variables to:
static uint16_t service_handles[4];         // -6 bytes (saves 2 bytes)
// Index: [0]=handshake, [1]=ts_req, [2]=ts_resp, [3]=update
```

#### 1.4 Remove Unused UUIDs in user_custs1_def.c
These arrays are not referenced anywhere:
```c
// REMOVE:
static const uint8_t TSVC_REQ_UUID_128[ATT_UUID_128_LEN];    // -16 bytes
static const uint8_t TSVC_RESP_UUID_128[ATT_UUID_128_LEN];   // -16 bytes
static const uint8_t USVC_UPDATE_UUID_128[ATT_UUID_128_LEN]; // -16 bytes
```
**Savings: 48 bytes**

### Phase 2: Architecture Improvements (For Future IMU/Features)

#### 2.1 Switch to Static CUSTS1 Profile
**Current Problem:** Dynamic GATT registration allocates BLE stack buffers that may not be freed.

**Solution:** Use SDK's static custom service (CUSTS1) system:
- Services defined at compile-time in attribute database
- Zero runtime memory allocation
- Faster connection establishment
- More stable

**Estimated savings: 200-400 bytes** (BLE stack overhead)

#### 2.2 Shared Buffer Strategy
Create one reusable buffer for multiple purposes:
```c
static uint8_t shared_buf[20];  // Shared for:
// - Advertising data (5 bytes)
// - GATT notifications (20 bytes max)
// - IMU data processing (temp buffer)
```
Instead of separate buffers for each use case.

#### 2.3 MAX_REMINDERS Scaling
Current: MAX_REMINDERS = 3 (6 bytes in reminder array)

**Options:**
- Production: Keep at 3 (sufficient for 99% of prescriptions)
- If needed: Can increase to 5 later when switching to static profile

### Phase 3: Memory Layout Optimization

#### 3.1 Retention vs Non-Retention
Review which variables need `__attribute__((section(".bss.")))`:
- Only variables accessed after sleep/wake need retention
- Others can use non-retained RAM (freed during sleep)

#### 3.2 Stack vs Static
For infrequently called functions:
- Use stack-based buffers (like UUID arrays)
- Only allocate when function executes
- Automatic cleanup

## Implementation Priority

### HIGH PRIORITY (Do Now)
1. ✅ Eliminate `advert_count` (not used)
2. ✅ Eliminate `ts_stream_idx` and `ts_stream_timer_id` (feature incomplete)
3. ✅ Reduce `adv_data_buf[31]` → `adv_data_buf[10]`
4. ✅ Consolidate 4 service handles → `service_handles[4]`
5. ✅ Remove unused UUID arrays in user_custs1_def.c

**Total savings: 90 bytes → Fixes overflow + 38 bytes headroom**

### MEDIUM PRIORITY (Before IMU)
6. Switch to static CUSTS1 profile system
7. Implement shared buffer strategy
8. Review retention memory usage

### LOW PRIORITY (Ongoing)
9. Profile actual RAM usage with map file analysis
10. Monitor heap fragmentation
11. Consider external SPI flash for non-volatile data

## Future Feature Budget

With 90-byte savings, estimated RAM available for new features:

| Feature | Estimated RAM | Status |
|---------|---------------|--------|
| IMU state/buffers | 20-30 bytes | Should fit |
| Button debounce | 4-8 bytes | Should fit |
| LED/buzzer state | 4-8 bytes | Should fit |
| RTC structures | Already in device_state | ✓ |
| Flash driver state | 10-20 bytes | May need Phase 2 |

**Buffer remaining after immediate fixes: ~40 bytes**  
**After IMU implementation: ~10-20 bytes** (tight but manageable)

## Monitoring Strategy

### Build-Time Checks
1. Review `.map` file after each build for RAM section sizes
2. Check linker output for ER_NZI and RET_DATA boundaries
3. Set up warning thresholds (e.g., warn if <100 bytes free)

### Runtime Checks
1. Add debug code to log heap usage: `ke_mem_is_empty()`
2. Monitor stack depth during testing
3. Check for memory leaks in long-running tests

## Recommendations

### Immediate Action
**Apply Phase 1 optimizations NOW** - this gives 90 bytes savings with zero functional impact.

### Before Adding IMU
**Implement Phase 2.1 (static CUSTS1)** - this is the proper architecture for constrained devices and will free 200-400 bytes.

### Decision Point
If still tight after IMU:
- Option A: Reduce MAX_REMINDERS to 2 (saves 2 bytes)
- Option B: Remove timestamp request/response services (only keep handshake + update)
- Option C: Use external flash for reminder storage instead of RAM

## Tools and Commands

### Check RAM Usage
```bash
# After building in Keil, examine map file:
grep -A 20 "Memory Map of the image" out_DA14531/Objects/ble_burst_adv_531.map

# Check ER_NZI size:
grep "ER_NZI" out_DA14531/Objects/ble_burst_adv_531.map
```

### Analyze Symbol Sizes
```bash
arm-none-eabi-nm --size-sort --radix=d out_DA14531/Objects/ble_burst_adv_531.axf
```

## Conclusion

**Current situation:** 52 bytes over limit, no IMU code yet  
**After Phase 1:** ~40 bytes under limit, room for basic IMU  
**After Phase 2:** ~250 bytes under limit, room for all features

**Next step:** Implement Phase 1 optimizations (30 minutes of work for 90 bytes savings)
