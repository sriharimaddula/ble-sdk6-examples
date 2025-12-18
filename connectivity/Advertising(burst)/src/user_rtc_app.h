/**
 ****************************************************************************************
 *
 * @file user_rtc_app.h
 *
 * @brief RTC application module header file.
 *
 ****************************************************************************************
 */

#ifndef _USER_RTC_APP_H_
#define _USER_RTC_APP_H_

#include <stdint.h>

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief Initialize the RTC driver and configuration.
 */
void user_rtc_init(void);

/**
 * @brief Set the RTC time from a Unix Epoch timestamp.
 * @param[in] epoch Unix Epoch seconds (UTC)
 */
void user_rtc_set_time(uint32_t epoch);

/**
 * @brief Find the next reminder in the list and schedule an RTC alarm.
 */
void user_rtc_schedule_next_alarm(void);

/**
 * @brief Callback function executed in the main loop when an alarm triggers.
 */
void user_rtc_alarm_cb(void);

#endif // _USER_RTC_APP_H_
