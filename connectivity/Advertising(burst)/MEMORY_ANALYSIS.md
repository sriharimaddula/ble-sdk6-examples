# DA14531 Memory Analysis & Optimization Strategy

## Current Situation: RAM Overflow

**Error:** `Execution region ER_NZI overlaps with RET_DATA`  
**Root Cause:** Dynamic GATT registration consuming excessive RAM on DA14531 (48KB total RAM)

---

## Memory Breakdown (DA14531 - 48KB RAM)

### BLE Stack Heaps (from SDK defaults - da1458x_stack_config.h)
```
DB_HEAP_SZ       = 1024 bytes  (GATT database storage)
ENV_HEAP_SZ      = 4928 bytes  (BLE environment/connection contexts)
MSG_HEAP_SZ      = 6880 bytes  (BLE stack messages/events)
NON_RET_HEAP_SZ  = 2048 bytes  (Non-retained heap for temporary allocations)
---------------------------------------------------------
TOTAL BLE HEAPS  = 14,880 bytes (~31% of 48KB)
```

### Retention Memory
```
CFG_RET_DATA_SIZE = 1536 bytes (reduced from 2048 to fix overflow)
```

### ROM Code + BLE Stack (Pre-allocated by SDK)
```
Estimated ~20-25KB (BLE controller, profiles, ROM functions)
```

### Application Code & Data
```
Your application code + static variables + stack
Estimated ~8-10KB
```

---

## Problem: Dynamic GATT is RAM-Hungry

**Why you're running out of RAM:**

1. **Dynamic GATT Registration:**
   - Each `GATTM_ADD_SVC_REQ` allocates from MSG_HEAP
   - Each service definition requires attribute descriptors (128-bit UUIDs = 16 bytes each)
   - 4 services × (service_desc + attributes) = ~600-800 bytes temporarily during registration
   - AFTER registration, data moves to DB_HEAP (~820 bytes permanently)

2. **Device Information Service (DISS):**
   - You have `CFG_PRF_DISS` enabled
   - DISS profile adds ~1.5-2KB of code + data
   - **YOU DON'T NEED THIS** - Mobile app doesn't use it

3. **Security Module:**
   - `CFG_APP_SECURITY` is undefined ✅ (good - saves ~3KB)

4. **Debug Features:**
   - `CFG_PRINTF` enabled = UART driver + printf library = ~2-3KB
   - `CFG_DEVELOPMENT_DEBUG` = debug symbols + validation = ~1-2KB
   - **Necessary for development but should be disabled for production**

---

## Immediate Fixes (Apply Now)

### 1. **REMOVE Device Information Service** ⚡ BIGGEST SAVINGS
Mobile app doesn't query DIS, you're wasting ~2KB RAM + 1-2KB code

**In `user_profiles_config.h`:**
```c
// #define CFG_PRF_DISS  // ❌ COMMENT OUT - saves ~2KB RAM
```

**In `user_config.h`:**
Remove DIS UUID from advertising data (line 183-184)

### 2. **Reduce BLE Heap Sizes** (Conservative Approach)
You only have 4 services (13 attributes). SDK defaults are for complex apps.

**In `da14531_config_advanced.h`:**
```c
#define DB_HEAP_SZ       800   // Down from 1024 (you need ~820 for 4 services)
#define MSG_HEAP_SZ      5000  // Down from 6880 (reduced message queue)
// Keep ENV_HEAP_SZ = 4928 (connection context, don't touch)
// Keep NON_RET_HEAP_SZ = 2048 (temporary allocations)
```

**Savings:** ~2KB RAM

### 3. **Optimize Retention Memory** ✅ Already Done
```c
CFG_RET_DATA_SIZE = 1536 bytes (was 2048) ✅
```

**Savings:** 512 bytes

---

## Medium-Term Optimization (Next Step if Still Issues)

### Convert to Static CUSTS1 Profile

**Current:** Dynamic GATT with `GATTM_ADD_SVC_REQ` messages  
**Better:** Static CUSTS1 profile (compile-time GATT database)

**Benefits:**
- No runtime MSG_HEAP allocation during service registration
- Services in GATT DB from boot (no timing bugs)
- Saves ~400-600 bytes RAM
- Faster boot time

**Implementation:** 
- Define services in `user_custs1_def.c` using `attm_desc_128` arrays
- Use `custs1_create_db()` instead of dynamic registration
- Reference: `SDK/ble_stack/profiles/custom/custs1` example

**Effort:** ~2-3 hours of refactoring

---

## Production Optimizations (Final Release)

### 1. **Disable Debug Features** 🔥 HUGE SAVINGS
```c
#undef CFG_PRINTF              // Saves ~2-3KB (UART + printf library)
#undef CFG_DEVELOPMENT_DEBUG   // Saves ~1-2KB (debug validation)
```

**Total Savings:** ~4-5KB RAM + code

### 2. **Compiler Optimizations**
In Keil project settings:
- Optimization Level: `-O3` (aggressive size/speed)
- Link-Time Optimization (LTO): Enable
- Remove unused sections: `--remove` flag

**Savings:** ~2-3KB code (doesn't help RAM but reduces flash)

### 3. **Reduce MAX_REMINDERS**
```c
#define MAX_REMINDERS 1  // Down from 2
```
**Savings:** 2 bytes per reminder structure

---

## Recommended Action Plan

### **PHASE 1: Quick Wins (Do This Now)** ⚡
1. ✅ Remove `CFG_PRF_DISS` from `user_profiles_config.h`
2. ✅ Remove DIS UUID from advertising data
3. ✅ Reduce `DB_HEAP_SZ` to 800 bytes
4. ✅ Reduce `MSG_HEAP_SZ` to 5000 bytes
5. ✅ Rebuild and test

**Expected Result:** +3-4KB free RAM → Should fix overflow

### **PHASE 2: If Still Tight**
6. Convert to static CUSTS1 profile (saves 400-600 bytes)

### **PHASE 3: Production**
7. Disable `CFG_PRINTF` and `CFG_DEVELOPMENT_DEBUG`
8. Enable compiler optimizations

---

## Memory Budget After Phase 1

```
BLE Stack Heaps:    12,680 bytes (down from 14,880)
Retention Memory:    1,536 bytes
ROM + BLE Stack:   ~22,000 bytes (fixed by SDK)
Application Code:   ~8,000 bytes
--------------------------------
TOTAL:             ~44,216 bytes
FREE:               ~3,784 bytes (8% margin) ✅
```

---

## Why This Happened

**Root Cause:** You're using **dynamic GATT registration** (meant for flexible runtime service changes) on a **severely RAM-constrained device** (DA14531 has only 48KB vs DA14585's 80KB).

**SDK Examples Misleading:** Most SDK examples assume DA14585 (80KB RAM). Only a few examples target DA14531's constraints.

**Better Approach for Production:** Static CUSTS1 profile (what most production DA14531 apps use).

---

## Questions to Consider

1. **Do you need DIS profile?** Mobile app doesn't query device info → Remove it
2. **Do you need debug printf in production?** No → Disable for release builds
3. **Do services change at runtime?** No → Use static CUSTS1 instead of dynamic
4. **Do you need 2 reminders?** Maybe 1 is enough → Saves 2 bytes

---

## Tools to Monitor Memory

**In Keil:**
1. Build → Check "Build Output" window for memory map
2. Look for:
   ```
   Total RO Size (Code):  XXXXX bytes
   Total RW Size (Data):  XXXXX bytes
   Total ZI Size (BSS):   XXXXX bytes
   ```
3. RW + ZI = Your RAM usage

**Enable Memory Logging (da14531_config_advanced.h):**
```c
#define CFG_LOG_HEAP_USAGE  // Prints heap usage at runtime
```

---

## Summary

**You're right** - we're only shaving off small amounts. The real issues are:

1. ❌ **DIS Profile enabled but unused** (~2KB waste)
2. ❌ **Default heap sizes too large** (~2KB waste)
3. ⚠️ **Dynamic GATT on 48KB device** (architectural concern)

**Fix priorities:**
1. Remove DIS (2 minutes)
2. Tune heaps (2 minutes)
3. Rebuild and test

This should give you **~4KB headroom** which is sufficient for this application.
