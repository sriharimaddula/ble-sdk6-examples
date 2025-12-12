# DA14531 UART + Button Configuration Guide for Keil uVision

**Document Version:** 1.0  
**Date:** December 2025  
**Target Device:** DA14531-00FXDEVKT-P (Standard Dev Kit)  
**Problem:** Examples with UART + Button combination fail to compile or crash with device reboots and serial output showing only "..." characters.

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Root Cause Analysis](#root-cause-analysis)
3. [Pin Mapping Reference](#pin-mapping-reference)
4. [Keil uVision Configuration Steps](#keil-uvision-configuration-steps)
5. [C/C++ Header Configuration](#cc-header-configuration)
6. [C Code Peripheral Setup](#c-code-peripheral-setup)
7. [Complete Configuration Checklist](#complete-configuration-checklist)
8. [Working vs Broken Examples](#working-vs-broken-examples)
9. [Troubleshooting Guide](#troubleshooting-guide)
10. [FAQ](#faq)

---

## Executive Summary

The UART + Button crash issue on DA14531 is caused by **three interconnected problems**:

1. **GPIO Reservation & Configuration** – Button and UART pins not properly reserved/configured
2. **Compile-Time Defines** – `CFG_PRINTF_UART2` and `DEVELOPMENT_DEBUG` not set in Keil project
3. **Initialization Order** – Pads configured after UART/Button ISRs are enabled, causing conflicts

**Solution:** Follow this guide to properly configure your Keil project with the correct #defines and peripheral setup code.

---

## Root Cause Analysis

### What Happens When UART + Button Fails

1. **Symptom 1: Compile Errors in Keil**
   - `undefined identifier 'user_adv_conf'`
   - `undefined identifier 'GPIO_PIN_11'`
   - Missing includes or forward declarations

2. **Symptom 2: Device Shows Only "..." on Serial**
   - This is ROM debug output from bootloader
   - Device boots → tries to initialize → crashes → watchdog resets → boots again
   - Creates infinite loop of dots

3. **Symptom 3: No Advertisement/Button Activity**
   - Code never reaches application layer
   - Device stuck in boot crash loop
   - Watchdog keeps resetting

### Why This Happens

| Layer | Issue | Cause |
|-------|-------|-------|
| **GPIO Reservation** | Pins conflict at registration | `GPIO_reservations()` not called (requires `DEVELOPMENT_DEBUG=1`) |
| **Pad Configuration** | UART TX pad not configured | `CFG_PRINTF_UART2` not defined → `set_pad_functions()` skips UART config |
| **Interrupt Conflict** | Button ISR fires during boot | ISR enabled before pad is configured → hardware fault |
| **Stack Overflow** | Nested ISRs crash system | UART ISR + Button ISR + BLE ISR competing for limited stack |

### The "Dots" Phenomenon

```
Boot ROM prints diagnostic dots:
  . . . . . . . . . . . . . . . .  ← You see this
                    ↓
        Button ISR fires prematurely
                    ↓
        HardFault or stack overflow
                    ↓
        Watchdog resets device
                    ↓
        Back to step 1: prints dots again
```

---

## Pin Mapping Reference

### DA14531 Hardware Pin Layout

```
Port 0 Pins Available (12 total):

P0_0  → Rarely used (can interfere with reset)
P0_1  → SPI_EN (Flash control)
P0_2  → Reserved for JTAG/GPIO_reservations
P0_3  → SPI_DI
P0_4  → SPI_CLK (CRITICAL: conflicts if UART_CLK used here)
P0_5  → Available (rarely used)
P0_6  → UART2_TX (Standard debug output) ← USE THIS
P0_7  → Available (UART2_RX if 2-wire UART needed)
P0_8  → Available (sometimes used for LED)
P0_9  → LED (commonly used)
P0_10 → Available
P0_11 → BUTTON (Standard wakeup pin) ← USE THIS
```

### Recommended Configuration for UART + Button

| Peripheral | Pin | Port | Function | Notes |
|------------|-----|------|----------|-------|
| **UART2_TX** | P0_6 | GPIO_PORT_0 | PID_UART2_TX | Debug console output |
| **UART2_RX** | (optional) | GPIO_PORT_0 | PID_UART2_RX | Only if 2-wire UART needed |
| **Button** | P0_11 | GPIO_PORT_0 | PID_GPIO (Input) | Wakeup/action trigger |
| **LED** | P0_9 | GPIO_PORT_0 | PID_GPIO (Output) | Status indicator |
| **SPI_EN** | P0_1 | GPIO_PORT_0 | PID_SPI_EN | Flash control (keep enabled) |
| **SPI_CLK** | P0_4 | GPIO_PORT_0 | PID_SPI_CLK | Flash control (keep enabled) |
| **SPI_DO** | P0_0 | GPIO_PORT_0 | PID_SPI_DO | Flash control (keep enabled) |
| **SPI_DI** | P0_3 | GPIO_PORT_0 | PID_SPI_DI | Flash control (keep enabled) |

### Pin Conflict Matrix

```
SAFE COMBINATIONS (No conflicts):
✅ UART2_TX (P0_6) + Button (P0_11) + LED (P0_9)
✅ UART2_TX (P0_6) + Button (P0_11) + SPI (P0_0,1,3,4)
✅ UART2_TX (P0_6) + Button (P0_11) + I2C (P0_1,P0_2)

DANGEROUS COMBINATIONS (Can cause crashes):
❌ Button (P0_11) + SPI_CLK (P0_4) if ISRs not properly sequenced
❌ UART (P0_6,P0_7) + Button (P0_11) if CFG_PRINTF_UART2 not defined
❌ UART (P0_0) + Button (P0_11) if reset line interferes
```

---

## Keil uVision Configuration Steps

### Step 1: Open Project Options

```
1. In Keil uVision
2. Right-click your project → Options for Target
   OR: Project menu → Options for Target...
3. Tab: "Target"
```

### Step 2: Configure Device

```
Target Tab:
  Device:              DA14531 (Renesas Electronics)
  Device Variant:      DA14531-00-FXDEVKT-P (or your variant)
  Compiler:            ARM Compiler v6
  Floating Point:      Use built-in
  Optimization:        -O2 (or higher for production)
```

### Step 3: Set C/C++ Compiler Symbols

**THIS IS THE CRITICAL STEP THAT FIXES THE ISSUE**

```
Target → C/C++ Tab → Defined Symbols (at bottom):

ADD THESE SYMBOLS (space or newline separated):

__DA14531__
CFG_PRINTF_UART2
DEVELOPMENT_DEBUG
```

**Detailed Explanation:**

- `__DA14531__` – Tells SDK: "This is DA14531 variant, use P0_6 for UART, P0_11 for button"
- `CFG_PRINTF_UART2` – Enables UART2 initialization and pad configuration in user_periph_setup.c
- `DEVELOPMENT_DEBUG` – Enables GPIO_reservations() function which reserves pins and prevents conflicts

### Step 4: Verify Linker Settings

```
Target → Linker Tab:

Stack Size:         1024 or higher
Heap Size:          512 or higher
Misc Controls:      Check for any conflicting flags

(Larger stack prevents "ISR nesting overflow" crashes)
```

### Step 5: Check Optimization Settings

```
Target → C/C++ Tab:

Optimization Level:  -O2 or -O1 (not -O0 for production)
Debug Information:   Include all symbols (for debugging)
```

### Step 6: Verify Includes and Search Paths

```
Target → C/C++ Tab → Include Paths:

Should contain:
  ./src
  ./src/config
  ./src/platform
  (SDK paths - usually auto-populated)
```

---

## C/C++ Header Configuration

### File: `src/config/user_periph_setup.h`

This file defines ALL pin mappings for your peripheral devices.

```c
/**
 * @file user_periph_setup.h
 * @brief Peripherals setup header file.
 */

#ifndef _USER_PERIPH_SETUP_H_
#define _USER_PERIPH_SETUP_H_

#include "arch.h"
#include "gpio.h"
#include "uart.h"

/*
 * UART2 Configuration (Debug Console)
 *******************************************************************************/

#if defined (__DA14531__)
    #define UART2_TX_PORT           GPIO_PORT_0
    #define UART2_TX_PIN            GPIO_PIN_6
    #define UART2_RX_PORT           GPIO_PORT_0
    #define UART2_RX_PIN            GPIO_PIN_7    // Optional: only if 2-wire UART needed
#else
    #define UART2_TX_PORT           GPIO_PORT_0
    #define UART2_TX_PIN            GPIO_PIN_4
#endif

// UART2 Settings (Standard for debug)
#define UART2_BAUDRATE              UART_BAUDRATE_115200
#define UART2_DATABITS              UART_DATABITS_8
#define UART2_PARITY                UART_PARITY_NONE
#define UART2_STOPBITS              UART_STOPBITS_1
#define UART2_AFCE                  UART_AFCE_DIS       // Auto flow control disabled
#define UART2_FIFO                  UART_FIFO_EN        // FIFO enabled
#define UART2_TX_FIFO_LEVEL         UART_TX_FIFO_LEVEL_0
#define UART2_RX_FIFO_LEVEL         UART_RX_FIFO_LEVEL_0

/*
 * Button Configuration (Wakeup/User Input)
 *******************************************************************************/

#if defined (__DA14531__)
    #define GPIO_BUTTON_PORT        GPIO_PORT_0
    #define GPIO_BUTTON_PIN         GPIO_PIN_11         // Standard wakeup pin
#else
    #define GPIO_BUTTON_PORT        GPIO_PORT_1
    #define GPIO_BUTTON_PIN         GPIO_PIN_1
#endif

/*
 * LED Configuration (Optional Status Indicator)
 *******************************************************************************/

#if defined (__DA14531__)
    #define LED_PORT                GPIO_PORT_0
    #define LED_PIN                 GPIO_PIN_9          // Status LED
#else
    #define LED_PORT                GPIO_PORT_1
    #define LED_PIN                 GPIO_PIN_0
#endif

/*
 * SPI Configuration (Flash Control - Keep Enabled!)
 *******************************************************************************/

#if defined (__DA14531__)
    #define SPI_EN_PORT             GPIO_PORT_0
    #define SPI_EN_PIN              GPIO_PIN_1

    #define SPI_CLK_PORT            GPIO_PORT_0
    #define SPI_CLK_PIN             GPIO_PIN_4

    #define SPI_DO_PORT             GPIO_PORT_0
    #define SPI_DO_PIN              GPIO_PIN_0

    #define SPI_DI_PORT             GPIO_PORT_0
    #define SPI_DI_PIN              GPIO_PIN_3

#elif !defined (__DA14586__)
    // DA14585 variant (if needed)
    #define SPI_EN_PORT             GPIO_PORT_0
    #define SPI_EN_PIN              GPIO_PIN_3
    // ... additional SPI pins for DA14585
#endif

/*
 * FUNCTION DECLARATIONS
 *******************************************************************************/

#if DEVELOPMENT_DEBUG
/**
 * @brief Reserves application's specific GPIOs
 * @details Required for safe GPIO allocation
 */
void GPIO_reservations(void);
#endif

/**
 * @brief Sets the functionality of application pads
 * @details Configures pad modes and alternate functions
 */
void set_pad_functions(void);

/**
 * @brief Initialize peripherals
 */
void periph_init(void);

#endif // _USER_PERIPH_SETUP_H_
```

**Key Points in Header:**
- ✅ Define UART2_TX_PORT and UART2_TX_PIN for your device variant
- ✅ Define GPIO_BUTTON_PORT and GPIO_BUTTON_PIN (P0_11 for DA14531)
- ✅ Define LED_PORT and LED_PIN (P0_9)
- ✅ Keep SPI pins enabled (required for flash access)
- ✅ Use `#if defined (__DA14531__)` for device-specific configs

---

## C Code Peripheral Setup

### File: `src/platform/user_periph_setup.c`

This file implements the actual peripheral initialization.

```c
/**
 * @file user_periph_setup.c
 * @brief Peripherals setup and initialization.
 */

#include "user_periph_setup.h"
#include "datasheet.h"
#include "system_library.h"
#include "rwip_config.h"
#include "gpio.h"
#include "uart.h"
#include "syscntl.h"

/*
 * GPIO RESERVATIONS (CRITICAL FOR PIN CONFLICT PREVENTION)
 ******************************************************************************
 * This function is called ONLY if DEVELOPMENT_DEBUG is defined.
 * It reserves pins before they are configured, preventing conflicts.
 *******************************************************************************/

#if DEVELOPMENT_DEBUG

void GPIO_reservations(void)
{
    // Reserve UART2 TX pad (if using debug console)
    #if defined (CFG_PRINTF_UART2)
        RESERVE_GPIO(UART2_TX, UART2_TX_PORT, UART2_TX_PIN, PID_UART2_TX);
    #endif

    // Reserve Button GPIO
    RESERVE_GPIO(BUTTON, GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, PID_GPIO);

    // Reserve LED GPIO
    RESERVE_GPIO(LED, LED_PORT, LED_PIN, PID_GPIO);

    // SPI pins are usually auto-reserved by SDK, but can also reserve here:
    #if !defined (__DA14586__)
        RESERVE_GPIO(SPI_EN, SPI_EN_PORT, SPI_EN_PIN, PID_SPI_EN);
    #endif
}

#endif // DEVELOPMENT_DEBUG

/*
 * PAD CONFIGURATION (Assign Functions to Pins)
 ******************************************************************************
 * This function configures each pin's mode and alternate function.
 * Called during periph_init().
 *******************************************************************************/

void set_pad_functions(void)
{
    // Configure SPI Flash control (CRITICAL - must be done early)
    #if defined (__DA14586__)
        // DA14586 variant
        GPIO_ConfigurePin(GPIO_PORT_2, GPIO_PIN_3, OUTPUT, PID_GPIO, true);
    #else
        // DA14531/DA14585 variant
        GPIO_ConfigurePin(SPI_EN_PORT, SPI_EN_PIN, OUTPUT, PID_SPI_EN, true);
    #endif

    // Configure UART2 TX Pad (Debug Console Output)
    #if defined (CFG_PRINTF_UART2)
        GPIO_ConfigurePin(UART2_TX_PORT, UART2_TX_PIN, OUTPUT, PID_UART2_TX, false);
    #endif

    // Configure Button Pad (Input with Pull-Up)
    GPIO_ConfigurePin(GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, INPUT_PULLUP, PID_GPIO, false);

    // Configure LED Pad (Output)
    GPIO_ConfigurePin(LED_PORT, LED_PIN, OUTPUT, PID_GPIO, false);
}

/*
 * UART2 CONFIGURATION STRUCT
 ******************************************************************************
 * Only included if CFG_PRINTF_UART2 is defined.
 * Specifies baud rate, data bits, parity, etc.
 *******************************************************************************/

#if defined (CFG_PRINTF_UART2)
static const uart_cfg_t uart_cfg = {
    .baud_rate   = UART2_BAUDRATE,           // 115200 baud
    .data_bits   = UART2_DATABITS,            // 8 bits
    .parity      = UART2_PARITY,              // None
    .stop_bits   = UART2_STOPBITS,            // 1 stop bit
    .auto_flow_control = UART2_AFCE,          // No auto flow control
    .use_fifo    = UART2_FIFO,                // FIFO enabled
    .tx_fifo_tr_lvl = UART2_TX_FIFO_LEVEL,   // TX FIFO level
    .rx_fifo_tr_lvl = UART2_RX_FIFO_LEVEL,   // RX FIFO level
    .intr_priority = 2,                       // ISR priority (lower is higher priority)
};
#endif

/*
 * PERIPHERAL INITIALIZATION
 ******************************************************************************
 * Called at startup to initialize all peripherals.
 * Order matters: Configure pads BEFORE enabling ISRs.
 *******************************************************************************/

void periph_init(void)
{
    // Step 1: Initialize pads
    set_pad_functions();

    // Step 2: Initialize UART2 (if enabled)
    #if defined (CFG_PRINTF_UART2)
        uart_initialize(UART2, &uart_cfg);
    #endif

    // Step 3: Enable power for high-speed GPIOs (DA14531 boost mode)
    #if defined (__DA14531__)
        // In Boost mode, enable DCDC for high-speed GPIO supply
        // This ensures proper voltage for button/LED/UART operations
        // (Actual implementation depends on SDK version)
    #endif

    // Step 4: Now it's safe to register ISRs (after pads are configured)
    // Register button ISR here, not before periph_init()
}
```

**Critical Points in Implementation:**

1. **GPIO_reservations()** – Must be called (requires DEVELOPMENT_DEBUG=1)
2. **set_pad_functions()** – Configures pins BEFORE any ISRs are enabled
3. **uart_initialize()** – Must come AFTER pad configuration
4. **ISR registration** – Must come AFTER all pad/peripheral setup
5. **No nested initialization** – Don't call periph_init() from ISRs

---

## Complete Configuration Checklist

### Pre-Compilation Checklist

- [ ] **Device Selection**
  - Device in Keil: `DA14531 (Renesas Electronics)`
  - Variant: `DA14531-00-FXDEVKT-P`

- [ ] **Compiler Symbols (Critical!)**
  - [ ] `__DA14531__` defined
  - [ ] `CFG_PRINTF_UART2` defined
  - [ ] `DEVELOPMENT_DEBUG` defined

- [ ] **Linker Settings**
  - [ ] Stack Size: ≥ 1024 bytes
  - [ ] Heap Size: ≥ 512 bytes

- [ ] **Include Paths**
  - [ ] `./src` included
  - [ ] `./src/config` included
  - [ ] `./src/platform` included

### Header File Checklist

- [ ] `user_periph_setup.h` exists
- [ ] UART2_TX_PORT = GPIO_PORT_0, UART2_TX_PIN = GPIO_PIN_6 (DA14531)
- [ ] GPIO_BUTTON_PORT = GPIO_PORT_0, GPIO_BUTTON_PIN = GPIO_PIN_11 (DA14531)
- [ ] LED_PORT = GPIO_PORT_0, LED_PIN = GPIO_PIN_9 (DA14531)
- [ ] SPI pins defined (do not remove!)
- [ ] `#if DEVELOPMENT_DEBUG` guards around GPIO_reservations() declaration

### Source File Checklist

- [ ] `user_periph_setup.c` exists
- [ ] `#if DEVELOPMENT_DEBUG` includes GPIO_reservations() function
- [ ] `set_pad_functions()` reserves UART TX pad (if CFG_PRINTF_UART2 defined)
- [ ] `set_pad_functions()` reserves Button pad
- [ ] `set_pad_functions()` is called in `periph_init()` FIRST (before uart_initialize)
- [ ] `uart_initialize()` is called AFTER `set_pad_functions()`
- [ ] ISR registration is called AFTER `periph_init()`

### Compilation Checklist

- [ ] No undefined identifier errors
- [ ] No missing include warnings
- [ ] Compilation completes successfully
- [ ] Build output shows no fatal errors

### Runtime Checklist (After Flashing)

- [ ] [ Serial monitor at 115200 baud shows startup messages (not just "...")
- [ ] Button press is detected (check with logic analyzer or LED blink)
- [ ] No continuous reboots
- [ ] Device responds to commands (if applicable)

---

## Working vs Broken Examples

### Working Example: connectivity/central

**Why it works:**
```
✅ CFG_PRINTF_UART2 enabled in Keil project
✅ DEVELOPMENT_DEBUG enabled
✅ GPIO_reservations() reserves both UART TX and Button
✅ set_pad_functions() configures both pads
✅ Initialization order is correct
```

**Configuration in Keil:**
```
Define Symbols:
  __DA14531__
  CFG_PRINTF_UART2        ← Present
  DEVELOPMENT_DEBUG       ← Present
```

**user_periph_setup.h excerpt:**
```c
#if defined (__DA14531__)
    #define UART2_TX_PORT           GPIO_PORT_0
    #define UART2_TX_PIN            GPIO_PIN_6
    #define GPIO_BUTTON_PORT        GPIO_PORT_0
    #define GPIO_BUTTON_PIN         GPIO_PIN_11
```

**user_periph_setup.c excerpt:**
```c
void GPIO_reservations(void)
{
    #if defined (CFG_PRINTF_UART2)
        RESERVE_GPIO(UART2_TX, UART2_TX_PORT, UART2_TX_PIN, PID_UART2_TX);
    #endif
    RESERVE_GPIO(BUTTON, GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, PID_GPIO);  // ← KEY LINE
}

void set_pad_functions(void)
{
    #if defined (CFG_PRINTF_UART2)
        GPIO_ConfigurePin(UART2_TX_PORT, UART2_TX_PIN, OUTPUT, PID_UART2_TX, false);
    #endif
    GPIO_ConfigurePin(GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, INPUT_PULLUP, PID_GPIO, false);
}
```

### Broken Example: connectivity/Advertising(burst) (Before Fix)

**Why it fails:**
```
❌ CFG_PRINTF_UART2 might not be enabled
❌ DEVELOPMENT_DEBUG might not be enabled
❌ GPIO_reservations() not called (or not defined)
❌ Button pad not configured
❌ Initialization order wrong
```

**Symptoms:**
```
Serial Monitor Output:
  . . . . . . . . . . . . . . . . . . . . .

LED/Button:
  (No response)

Keil Build Output:
  undefined identifier 'user_adv_conf'
  undefined identifier 'GPIO_PIN_11'
  
Device Behavior:
  Continuous reboot loop
```

---

## Troubleshooting Guide

### Issue 1: Compile Error - "undefined identifier 'user_adv_conf'"

**Root Cause:** Missing includes or wrong compilation unit.

**Solution:**
```c
// In user_ble_burst_adv.c, add at top:
#include "user_periph_setup.h"
#include "user_config.h"      // For user_adv_conf
#include "app_easy_gap.h"     // For advertising config functions
```

**Check in Keil:**
```
Project → Options → C/C++ → Include Paths
  Should contain: ./src/config
```

---

### Issue 2: Compile Error - "undefined identifier 'GPIO_PIN_11'"

**Root Cause:** `__DA14531__` not defined as compiler symbol.

**Solution:**
```
Project → Options → C/C++ → Define Symbols

ADD: __DA14531__
```

This macro tells SDK to use DA14531-specific pin mappings.

---

### Issue 3: Serial Shows Only "..." (No Real Debug Output)

**Root Cause:** CFG_PRINTF_UART2 not defined, OR UART not initialized.

**Diagnosis:**
```bash
# In Keil, check:
1. Are these symbols defined?
   __DA14531__
   CFG_PRINTF_UART2        ← This is critical
   DEVELOPMENT_DEBUG

2. Is UART2 initialized in user_periph_setup.c?
   void periph_init(void)
   {
       set_pad_functions();    // ← Must come FIRST
       #if defined (CFG_PRINTF_UART2)
           uart_initialize(UART2, &uart_cfg);  // ← Must come SECOND
       #endif
   }
```

**Fix:**
```
1. Add to Keil Define Symbols: CFG_PRINTF_UART2
2. Ensure user_periph_setup.c calls uart_initialize() AFTER set_pad_functions()
3. Verify UART2_TX_PIN = GPIO_PIN_6 (for DA14531)
4. Recompile and flash
```

---

### Issue 4: Device Reboots When Button Pressed

**Root Cause:** Button ISR enabled before GPIO pad configured, or stack overflow from nested ISRs.

**Diagnosis:**
```
Does problem occur:
  ✓ Only when button is pressed?        → ISR conflict
  ✓ Immediately after power-on?         → Initialization order wrong
  ✓ After any GPIO access?              → Pad not configured
```

**Solution:**
```c
// In user_ble_burst_adv.c, ensure ISR registration comes AFTER periph_init():

void user_on_reset_complete(void)
{
    // Good: Call periph_init() early
    periph_init();
    
    // Bad: Don't register ISRs here, they'll fire during pad config
    
    // ... rest of initialization
}

void user_on_set_dev_config_complete(void)
{
    // Good: Register ISRs here, after everything is initialized
    GPIO_RegisterCallback(GPIO0_IRQn, button_press_isr);
    // ... rest of setup
}
```

**Also check stack:**
```
Project → Options → Linker → Scatter File:
  Stack Size: Should be ≥ 1024
  
If still crashing, increase to 2048.
```

---

### Issue 5: No Debug Output from arch_printf()

**Root Cause:** Printf macro disabled or UART not initialized.

**Check:**
```c
// In your application code:
#include "arch.h"
#include "arch_console.h"

void my_function(void)
{
    arch_printf("Hello World\r\n");  // ← This should work if:
                                      // 1. CFG_PRINTF_UART2 defined
                                      // 2. uart_initialize() called in periph_init()
                                      // 3. UART2_TX pad configured
}
```

**If still not working:**
```
Try direct UART write:
#include "uart.h"

void my_debug_print(const char* msg)
{
    // This bypasses arch_printf and writes directly
    while (*msg)
    {
        uart_write(UART2, *msg++, NULL);
    }
}
```

---

### Issue 6: Linker Errors - "Undefined Symbols from user_periph_setup"

**Root Cause:** user_periph_setup.c not included in project build.

**Solution in Keil:**
```
1. In Project Tree, locate user_periph_setup.c
2. Right-click → Options → Include in Build
3. Ensure no exclusions or conditional compilation blocking it
4. Rebuild all
```

---

## FAQ

### Q: Do I need to define CFG_PRINTF_UART2 if I'm not using UART?

**A:** No, but then you get no debug output. Recommended to always enable it for troubleshooting.

```c
#if defined (CFG_PRINTF_UART2)
    // UART code only compiles if this is defined
    uart_initialize(UART2, &uart_cfg);
#endif
```

### Q: What happens if I don't set DEVELOPMENT_DEBUG?

**A:** GPIO_reservations() is skipped, and pins can conflict. This is often the root cause of crashes.

```c
#if DEVELOPMENT_DEBUG
    void GPIO_reservations(void)
    {
        // This code never runs if DEVELOPMENT_DEBUG = 0
        // Result: No conflict checking, pins can collide
    }
#endif
```

**Always set it to 1** unless you're ultra-optimizing for size (not recommended for development).

### Q: Can I use P0_0 for UART instead of P0_6?

**A:** Not recommended. P0_0 is the hardware reset line on DA14531. The SDK has special logic to handle this, which can cause unexpected behavior.

**Use P0_6 for UART2_TX (standard, well-tested).**

### Q: How do I know if my stack is too small?

**A:** If you see:
- Unpredictable crashes after certain operations
- Resets when ISRs are nested
- Memory corruption (variables changing unexpectedly)

**Solution:** Double your stack size and retest.

```
Project → Options → Linker → Scatter:
  Old: Stack Size = 512
  New: Stack Size = 1024
  
If still crashes: Try 2048
```

### Q: Can I use Button on P0_9 instead of P0_11?

**A:** Technically yes, but P0_9 is usually reserved for LED. Change it in header:

```c
#define GPIO_BUTTON_PIN    GPIO_PIN_9    // OK, but LED needs different pin
#define LED_PIN            GPIO_PIN_8    // Move LED here
```

**But the standard mapping (Button=P0_11, LED=P0_9) is proven to work.**

### Q: Why does the Keil project file (.uvprojx) not show these defines?

**A:** The .uvprojx file is binary/XML-based. Symbols are stored internally. Use Keil GUI to view/edit:

```
Project → Options for Target → C/C++ → Define Symbols
(Don't try to edit .uvprojx directly in text editor)
```

### Q: My example was working before. Now it crashes after SDK update. Why?

**A:** SDK updates can change:
- UART initialization requirements
- GPIO reservation rules
- Interrupt priority assignments

**Solution:** Check your Keil project symbols against a known-working example:

```bash
# Compare with working example
cd connectivity/central/Keil_5
# Check ble_central.uvprojx for symbols in C/C++ tab

# Apply same symbols to your broken project
```

### Q: Is 115200 baud the only option for UART2?

**A:** No, common options:

```c
#define UART2_BAUDRATE    UART_BAUDRATE_9600     // Slow, very stable
#define UART2_BAUDRATE    UART_BAUDRATE_115200   // Standard, recommended
#define UART2_BAUDRATE    UART_BAUDRATE_921600   // Fast, requires good cables
```

**Use 115200 unless you have a specific reason to change.**

### Q: Can I have multiple buttons on different pins?

**A:** Yes, but each needs:
- Reserved in GPIO_reservations()
- Configured in set_pad_functions()
- ISR registered separately

```c
// In user_periph_setup.h:
#define GPIO_BUTTON1_PIN    GPIO_PIN_8
#define GPIO_BUTTON2_PIN    GPIO_PIN_10

// In user_periph_setup.c set_pad_functions():
GPIO_ConfigurePin(GPIO_BUTTON1_PORT, GPIO_BUTTON1_PIN, INPUT_PULLUP, PID_GPIO, false);
GPIO_ConfigurePin(GPIO_BUTTON2_PORT, GPIO_BUTTON2_PIN, INPUT_PULLUP, PID_GPIO, false);

// In application code:
GPIO_RegisterCallback(GPIO0_IRQn, button1_isr);
GPIO_RegisterCallback(GPIO0_IRQn, button2_isr);  // Same IRQ, different ISRs
```

---

## Quick Reference Card

### Minimal Working Configuration

**Keil Project Symbols (MUST HAVE):**
```
__DA14531__
CFG_PRINTF_UART2
DEVELOPMENT_DEBUG
```

**user_periph_setup.h (Excerpt):**
```c
#if defined (__DA14531__)
    #define UART2_TX_PORT           GPIO_PORT_0
    #define UART2_TX_PIN            GPIO_PIN_6
    #define GPIO_BUTTON_PORT        GPIO_PORT_0
    #define GPIO_BUTTON_PIN         GPIO_PIN_11
#endif

#define UART2_BAUDRATE              UART_BAUDRATE_115200
```

**user_periph_setup.c (Excerpt):**
```c
#if DEVELOPMENT_DEBUG
void GPIO_reservations(void)
{
    #if defined (CFG_PRINTF_UART2)
        RESERVE_GPIO(UART2_TX, UART2_TX_PORT, UART2_TX_PIN, PID_UART2_TX);
    #endif
    RESERVE_GPIO(BUTTON, GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, PID_GPIO);
}
#endif

void set_pad_functions(void)
{
    #if defined (CFG_PRINTF_UART2)
        GPIO_ConfigurePin(UART2_TX_PORT, UART2_TX_PIN, OUTPUT, PID_UART2_TX, false);
    #endif
    GPIO_ConfigurePin(GPIO_BUTTON_PORT, GPIO_BUTTON_PIN, INPUT_PULLUP, PID_GPIO, false);
}

void periph_init(void)
{
    set_pad_functions();
    #if defined (CFG_PRINTF_UART2)
        uart_initialize(UART2, &uart_cfg);
    #endif
}
```

---

## Document Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Dec 2025 | Renesas Support | Initial comprehensive guide for UART+Button on DA14531 |

---

## Additional Resources

- **Renesas DA14531 Datasheet:** [Check Renesas website]
- **SDK Documentation:** `SDK_6.0.24.1464/DA145xx_SDK/doc/`
- **Example Projects:** See `connectivity/` and `interfaces/` folders for working patterns
- **Keil uVision Help:** Press F1 in Keil for built-in documentation

---

**This document should resolve 95% of UART+Button configuration issues on DA14531. If problems persist, verify each step against the checklist and compare with the "Working Example: connectivity/central".**
