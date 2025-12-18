/**
 ****************************************************************************************
 *
 * @file user_rtc_app.c
 *
 * @brief RTC application module source code.
 *
 ****************************************************************************************
 */

#include "user_rtc_app.h"
#include "rtc.h"
#include "user_ble_burst_adv.h" // For device_state
#include "user_periph_setup.h"  // For GPIO_LED_PORT/PIN
#include "arch_api.h"           // For app_easy_wakeup
#include "gpio.h"
#include "arch_system.h"

#ifdef CFG_PRINTF
    #include "arch_console.h"
#endif

extern rcx_time_data_t rcx_time_data;

/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */
static void rtc_interrupt_handler(uint8_t event);
static void epoch_to_rtc(uint32_t epoch, rtc_time_t *t, rtc_calendar_t *c);
static void user_rtc_clock_set(void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void user_rtc_init(void)
{
    // Initialize RTC configuration (24H mode, keep RTC on reset)
    // rtc_config_t config = { .hour_clk_mode = RTC_HOUR_MODE_24H, .keep_rtc = 1 };
    
    // We don't set time here, just ensure mode is correct. 
    // Time is set via user_rtc_set_time()
    
    // Note: rtc_configure requires time/calendar pointers, so we'll do full config in set_time
}

void user_rtc_set_time(uint32_t epoch)
{
    rtc_time_t time;
    rtc_calendar_t calendar;
    rtc_config_t config = { .hour_clk_mode = RTC_HOUR_MODE_24H, .keep_rtc = 1 };

    // Convert Epoch to RTC format
    epoch_to_rtc(epoch, &time, &calendar);

    // Power up the PD_TIM domain
    SetBits16(PMU_CTRL_REG, TIM_SLEEP, 0); // wakeup timer PD
    while (GetBits16(SYS_STAT_REG, TIM_IS_UP) == 0);

    // Initialize the RTC configuration
    rtc_init(&config);

    // Set the clock of the RTC depending on the selected LP clock
    user_rtc_clock_set();

    // Enable the RTC clk
    rtc_clock_enable();

    // Set time and calendar
    rtc_set_time_clndr(&time, &calendar);
    
    // Start RTC
    rtc_time_start();

    #ifdef CFG_PRINTF
        arch_printf("\n\r[RTC] Time set to: %04d-%02d-%02d %02d:%02d:%02d",
                    calendar.year, calendar.month, calendar.mday,
                    time.hour, time.minute, time.sec);
    #endif
}

static void user_rtc_clock_set(void)
{
    // Set the frequency that the RTC will operate
    if (arch_clk_is_RCX20()) {
        // Calculate RCX frequency. (1000000 / 625 >> 20) is equal to (100 >> 16), which is equal to 25 >> 14
        rtc_clk_config(RTC_DIV_DENOM_1000, rcx_time_data.rcx_freq);
    } else {
        rtc_clk_config(RTC_DIV_DENOM_1000, 32768);
    }
}

void user_rtc_schedule_next_alarm(void)
{
    if (device_state.reminder_count == 0)
    {
        #ifdef CFG_PRINTF
            arch_printf("\n\r[RTC] No reminders to schedule.");
        #endif
        return;
    }

    // 1. Get current time
    rtc_time_t time;
    rtc_calendar_t clndr;
    rtc_get_time_clndr(&time, &clndr);

    // 2. Convert current time to minutes since midnight
    uint16_t current_minutes = (time.hour * 60) + time.minute;
    
    // 3. Find the next reminder
    uint16_t next_alarm_minutes = 0xFFFF; // Invalid init value
    uint16_t min_reminder_minutes = 0xFFFF; // Earliest reminder in the day (for wrap around)

    for (int i = 0; i < device_state.reminder_count; i++) {
        uint16_t r_min = device_state.reminders[i].minutes_since_midnight;
        
        // Track the earliest reminder for tomorrow's wrap-around
        if (r_min < min_reminder_minutes) {
            min_reminder_minutes = r_min;
        }

        // Check if this reminder is later today and sooner than any other found so far
        // We add a small buffer (e.g. 1 minute) to avoid re-triggering same minute immediately if logic is fast
        if (r_min > current_minutes && r_min < next_alarm_minutes) {
            next_alarm_minutes = r_min;
        }
    }

    // 4. Determine if we found a reminder for today, or if we need to wrap to tomorrow
    if (next_alarm_minutes == 0xFFFF) {
        // No more reminders today, schedule the earliest one for tomorrow
        if (min_reminder_minutes != 0xFFFF) {
            next_alarm_minutes = min_reminder_minutes;
            #ifdef CFG_PRINTF
                arch_printf("\n\r[RTC] Scheduling for tomorrow.");
            #endif
        } else {
            // Should not happen if count > 0
            return; 
        }
    }

    // 5. Configure the RTC Alarm
    rtc_time_t alarm_time;
    alarm_time.hour = next_alarm_minutes / 60;
    alarm_time.minute = next_alarm_minutes % 60;
    alarm_time.sec = 0;
    alarm_time.hsec = 0;
    alarm_time.hour_mode = RTC_HOUR_MODE_24H;

    // Set alarm (Ignore Date -> Daily recurrence logic)
    // We enable Hour, Minute, Second interrupts
    // Note: Passing NULL for calendar implies ignoring date fields if mask is set correctly
    rtc_set_alarm(&alarm_time, NULL, RTC_ALARM_EN_HOUR | RTC_ALARM_EN_MIN | RTC_ALARM_EN_SEC);

    // 6. Enable Interrupt
    // Clear any pending
    rtc_register_intr(rtc_interrupt_handler, RTC_INTR_ALRM);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[RTC] Next Alarm set for %02d:%02d:00", alarm_time.hour, alarm_time.minute);
    #endif
}

/**
 * @brief ISR Handler for RTC Interrupts
 */
static void rtc_interrupt_handler(uint8_t event)
{
    // Check if it is an alarm event
    if (event & RTC_INTR_ALRM)
    {
        // 1. Wake up the BLE core if it's sleeping
        if (GetBits16(SYS_STAT_REG, PER_IS_DOWN)) {
            periph_init();
        }

        // 2. Force BLE Wakeup
        arch_ble_force_wakeup();

        // 3. Trigger a callback in the main loop
        app_easy_wakeup_set(user_rtc_alarm_cb);
        app_easy_wakeup();
    }
}

/**
 * @brief Application Callback (Main Loop)
 */
void user_rtc_alarm_cb(void)
{
    #ifdef CFG_PRINTF
        arch_printf("\n\r[RTC] ALARM TRIGGERED!");
    #endif

    // Toggle LED
    if (GPIO_GetPinStatus(GPIO_LED_PORT, GPIO_LED_PIN))
    {
        GPIO_SetInactive(GPIO_LED_PORT, GPIO_LED_PIN);
    }
    else
    {
        GPIO_SetActive(GPIO_LED_PORT, GPIO_LED_PIN);
    }

    // Schedule the next alarm
    user_rtc_schedule_next_alarm();
}

/**
 * @brief Helper: Convert Unix epoch to RTC time
 */
static void epoch_to_rtc(uint32_t epoch, rtc_time_t *t, rtc_calendar_t *c)
{
    uint32_t days = epoch / 86400u;
    uint32_t rem = epoch % 86400u;

    t->hour = rem / 3600u;
    t->minute = (rem % 3600u) / 60u;
    t->sec = rem % 60u;
    t->hsec = 0; 

    /* Compute year */
    uint32_t year = 1970;
    while (1) {
        uint32_t isleap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        uint32_t days_in_year = 365 + isleap;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }

    /* Month lengths */
    const uint8_t month_days_norm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint8_t month = 0;
    for (int m = 0; m < 12; m++) {
        uint8_t mdays = month_days_norm[m];
        if (m == 1) { /* February */
            uint32_t isleap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
            if (isleap) mdays = 29;
        }
        if (days < mdays) { month = m + 1; break; }
        days -= mdays;
    }

    c->year = (uint16_t)year;
    c->month = month;
    c->mday = (uint8_t)(days + 1);
    c->wday = (uint8_t)(( (epoch / 86400u) + 4u) % 7u);
}
