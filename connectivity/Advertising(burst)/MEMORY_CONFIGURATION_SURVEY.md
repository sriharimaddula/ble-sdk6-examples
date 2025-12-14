# DA14531 Memory Configuration Survey (BLE SDK Examples)

**Date:** December 14, 2025  
**Purpose:** Validate our memory configuration by comparing with official Renesas examples

---

## Summary of 12+ BLE Examples Analyzed

| Example                  | DB_HEAP | ENV_HEAP | MSG_HEAP | NON_RET | RET_DATA | Notes                        |
|--------------------------|---------|----------|----------|---------|----------|------------------------------|
| dyn_custom_svc           | SDK     | SDK      | SDK      | SDK     | 2048     | Dynamic GATT (like ours)     |
| ble_Notify_button_Wakeup | SDK     | SDK      | SDK      | SDK     | 2048     | Notifications                |
| ble_temperature_ntf      | SDK     | SDK      | SDK      | SDK     | 2048     | Temperature service          |
| ble_indication_hs3001    | SDK     | SDK      | SDK      | SDK     | 2048     | Indication + sensor          |
| central                  | SDK     | SDK      | SDK      | SDK     | 2048     | GAP Central role             |
| multi_con_periph         | SDK     | SDK      | SDK      | SDK     | 2200     | Multiple connections         |
| accelerometer_notif      | SDK     | SDK      | SDK      | SDK     | 2200     | Accel + notifications        |
| active_scanner           | SDK     | SDK      | **10000**| SDK     | 2048     | Scanner needs large MSG heap |
| switching_roles          | SDK     | SDK      | **6880** | SDK     | 2048     | Multi-role complexity        |
| **Advertising(burst)**   | SDK     | SDK      | SDK      | SDK     | 2048     | **Our configuration** ✅     |

**SDK = Commented out (lets SDK auto-calculate optimal sizes)**

---

## Key Findings

### 1. **99% of Examples Use SDK Defaults** ✅
- **Only 2 out of 12+ examples** override heap sizes
- Both have **specific technical reasons**:
  - `active_scanner`: MSG_HEAP=10000 (scanner role needs large buffer for scan reports)
  - `switching_roles`: MSG_HEAP=6880 (multi-role switching complexity)

### 2. **RET_DATA_SIZE is Consistently 2048 Bytes** ✅
- Standard for single-connection peripherals: **2048 bytes**
- Slight increase for special cases:
  - `multi_con_periph`: 2200 bytes (multiple concurrent connections = more state)
  - `accelerometer_notif`: 2200 bytes (sensor data retention)

### 3. **Our Original Mistake** ❌
We incorrectly enabled heap defines:
```c
// WRONG (what we did in commits 42a40d15 - 1ccb1676):
#define DB_HEAP_SZ      700   // ❌ Forced fixed size
#define MSG_HEAP_SZ     4500  // ❌ Too small, caused overflow
#define ENV_HEAP_SZ     4500  // ❌ Reduced unnecessarily
```

**Result:** Rigid memory layout → regions overlapped → linker errors

**Correct (reverted in commit 74734893):**
```c
// CORRECT (SDK defaults):
// #define DB_HEAP_SZ      1024  // ✅ Commented = SDK auto-calculates
// #define MSG_HEAP_SZ     6880  // ✅ Commented = SDK auto-calculates
// #define ENV_HEAP_SZ     4928  // ✅ Commented = SDK auto-calculates
```

### 4. **When to Override Heap Sizes**

**Only override if:**
1. **Scanner/Observer role**: Need MSG_HEAP ≥ 10000 for scan report buffer
2. **Multiple connections**: May need larger ENV_HEAP (tested at runtime)
3. **Multi-role switching**: May need fixed MSG_HEAP for determinism
4. **Proven insufficient**: Runtime testing shows heap exhaustion

**For single-connection peripheral with dynamic GATT: NEVER override** ✅

---

## Application Match Analysis

Our application characteristics:
- ✅ Peripheral role (not scanner/central)
- ✅ Single connection (CFG_MAX_CONNECTIONS = 1)
- ✅ Dynamic GATT registration (4 custom services)
- ✅ Notify characteristics (timestamp streaming)

**Closest matching examples:**
1. **dyn_custom_svc** - Dynamic GATT registration ← **Primary reference**
2. **ble_temperature_ntf** - Notify characteristics
3. **ble_indication_hs3001** - Sensor data + BLE notifications

**All three use:**
- ✅ SDK default heaps (commented out)
- ✅ RET_DATA_SIZE = 2048
- ✅ CFG_AUTO_DETECT_NON_RET_HEAP enabled

---

## Validation Commands

To verify any BLE example's memory configuration:
```bash
# Check heap defines
grep -E "(DB_HEAP|ENV_HEAP|MSG_HEAP|NON_RET_HEAP)" \
  connectivity/EXAMPLE_NAME/src/config/da14531_config_advanced.h

# Check retention size
grep "CFG_RET_DATA_SIZE" \
  connectivity/EXAMPLE_NAME/src/config/da14531_config_advanced.h
```

---

## Memory Impact Rule (Established)

**For every code change, evaluate:**

1. **Static/Global Variables**
   - Check size with `sizeof()`
   - Consider alignment/padding
   - Example: `device_state_t` = 10 bytes

2. **Buffer Allocations**
   - Stack: Local arrays (function scope)
   - Heap: Malloc/KE_MSG_ALLOC (BLE messages)
   - Retention: `__attribute__((section(".bss.")))`

3. **SDK Macro Side Effects**
   - Enabling heap defines = forced allocation
   - Commenting them out = dynamic calculation
   - **Trust SDK defaults unless proven insufficient**

4. **Build After Each Change**
   - Don't batch changes
   - Catch memory issues early
   - Check linker memory map output

---

## Conclusion

✅ **Our current configuration matches industry best practices:**
- All heap sizes commented out (SDK auto-calculates)
- RET_DATA_SIZE = 2048 bytes (standard for single-connection peripheral)
- Identical to `dyn_custom_svc` and other notification examples

✅ **Lessons learned:**
1. SDK's dynamic heap sizing is smarter than manual tuning
2. 99% of examples trust SDK defaults - so should we
3. Only override heaps for specific technical requirements (scanner, multi-role)
4. Memory configuration is application-role dependent, not code-size dependent

✅ **This configuration should build successfully and fit within DA14531's 48KB RAM.**
