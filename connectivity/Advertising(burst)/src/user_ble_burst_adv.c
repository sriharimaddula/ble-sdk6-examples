/**
 ****************************************************************************************
 *
 * @file user_ble_burst_adv.c
 *
 * @brief BLE burst advertising project source code.
 *
 * Copyright (c) 2012-2021 Renesas Electronics Corporation and/or its affiliates
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
 
#include "user_ble_burst_adv.h"
#include "gpio.h"


/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

/* Number of adveritsing events per burst - the time between events is the advertising
   interval define in user_config.h (user_adv_conf.intv_min) */
#define ADV_EVENTS_PER_BURST					4
/* How often bursts of advertising events should be transmitted (time between bursts) */
#define BURST_REPEAT_PERIOD_ms				8000
#define BURST_REPEAT_PERIOD_TICKS			MS_TO_TIMERUNITS(BURST_REPEAT_PERIOD_ms)

static timer_hnd adv_burst_timer_id		__attribute__((section(".bss."))); // @RETENTION MEMORY
static uint16_t adv_period_ticks      __attribute__((section(".bss."))); // @RETENTION MEMORY

/* Advertisement burst counter */
static uint32_t advert_count = 0;

/* Advertisement data buffer with counter byte (manufacturer data) */
/* Format: length(1) + type(1) + company_id(2) + counter(1) = 5 bytes total
   \x04 = length (4 bytes follow)
   \xFF = manufacturer specific data type
   \x4C\x00 = Apple company ID (little endian)
   counter byte will be updated dynamically */
static uint8_t adv_data_buf[31];
static uint8_t adv_data_len = 0;

/* Forward declarations */
static void led_blink_off(void);
static void start_advertising(void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

/**
 ****************************************************************************************
 * @brief Blink LED for 100ms.
 * 
 * @param[in] None. 
 *
 * @return None.
 ****************************************************************************************
 */
static void led_blink_on(void)
{
    /* Turn LED on */
    GPIO_SetActive(LED_PORT, LED_PIN);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\rLED ON");
    #endif
    
    /* Schedule LED turn off after 100ms */
    app_easy_timer(MS_TO_TIMERUNITS(100), led_blink_off);
}

/**
 ****************************************************************************************
 * @brief Turn LED off (callback from timer).
 * 
 * @param[in] None. 
 *
 * @return None.
 ****************************************************************************************
 */
static void led_blink_off(void)
{
    /* Turn LED off */
    GPIO_SetInactive(LED_PORT, LED_PIN);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\rLED OFF");
    #endif
}

/**
 ****************************************************************************************
 * @brief Update advertisement data with counter byte and advertise it.
 * 
 * @param[in] None. 
 *
 * @return None.
 ****************************************************************************************
 */
static void update_and_advertise(void)
{
    /* Base advertising data: DIS service UUID */
    const uint8_t base_adv_data[] = {0x03, 0x03, 0x0A, 0x18};
    
    /* Build new adv data: base data + manufacturer data with counter */
    uint8_t idx = 0;
    
    /* Copy base advertising data */
    for (uint8_t i = 0; i < sizeof(base_adv_data); i++) {
        adv_data_buf[idx++] = base_adv_data[i];
    }
    
    /* Add manufacturer specific data: length(1) + type(1) + company_id(2) + counter(1) */
    adv_data_buf[idx++] = 0x04;  /* Length: 4 bytes follow */
    adv_data_buf[idx++] = 0xFF;  /* Manufacturer Specific Data type */
    adv_data_buf[idx++] = 0x4C;  /* Apple Company ID (low byte) */
    adv_data_buf[idx++] = 0x00;  /* Apple Company ID (high byte) */
    adv_data_buf[idx++] = (uint8_t)(advert_count & 0xFF);  /* Counter byte */
    
    adv_data_len = idx;
    
    /* Update the advertisement data */
    app_easy_gap_update_adv_data(adv_data_buf, adv_data_len, NULL, 0);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\rADV_BURST #%u : Payload = ", advert_count);
        for (uint8_t i = 0; i < adv_data_len; i++) {
            arch_printf("%02X ", adv_data_buf[i]);
        }
    #endif
}

/**
 ****************************************************************************************
 * @brief Button press interrupt handler.
 * 
 * @param[in] None. 
 *
 * @return None.
 ****************************************************************************************
 */
static void button_press_isr(void)
{
    #ifdef CFG_PRINTF
        arch_printf("\n\r*** BUTTON PRESSED ***");
    #endif
    
    /* Trigger advertisement burst on button press */
    start_advertising();
}

/**
 ****************************************************************************************
 * @brief Start advertising burst on button press.
 * 
 * @param[in] None. 
 *
 * @return None.
 ****************************************************************************************
 */
static void start_advertising(void)
{
	  /* Calculate how long we should advertise for in order to get the required number of 
	     events. Simple calculation based on the advertising interval and the required
	     number of events. */
    adv_period_ticks = MS_TO_TIMERUNITS((user_adv_conf.intv_min * 0.625) * ADV_EVENTS_PER_BURST);

    /* Increment advertisement burst counter */
    advert_count++;

    /* Update advertisement data with counter and advertise */
    update_and_advertise();

    /* Blink LED */
    led_blink_on();

  	#ifdef CFG_PRINTF
	      arch_printf("\n\r>>> Advertising burst #%u started (duration: %dms) <<<", 
                     advert_count, adv_period_ticks * 10 / 1);
	  #endif
  	
  	app_easy_gap_undirected_advertise_with_timeout_start(adv_period_ticks, NULL);
}

/**
 ****************************************************************************************
 * @brief Called when device configuration is complete.
 *
 * @param[in] None.
 *
 * @return None.
 ****************************************************************************************
 */
void user_on_set_dev_config_complete(void)
{
    #ifdef CFG_PRINTF
	      arch_printf("\n\n\r");
	      arch_printf("========================================\n\r");
	      arch_printf("BLE Burst Advertisement - Button Mode\n\r");
	      arch_printf("========================================\n\r");
	      arch_printf("%s - Ready for button press\n\r", __FUNCTION__);
	      arch_printf("Button: P0_11 | LED: P0_9\n\r");
	      arch_printf("Press button to send advertisement burst\n\r");
	      arch_printf("========================================\n\r");
	  #endif
	
    default_app_on_set_dev_config_complete();
	
	  /* Initialize button with interrupt handler */
	  GPIO_ConfigurePin(BUTTON_PORT, BUTTON_PIN, INPUT_PULLUP, PID_GPIO, false);
	  GPIO_EnableIRQ(BUTTON_PORT, BUTTON_PIN, GPIO0_IRQn, GPIO_IRQ_INPUT_LEVEL_LOW, 
	                 true, BUTTON_IRQ_DEBOUNCE_MS);
	  GPIO_RegisterCallback(GPIO0_IRQn, button_press_isr);
	  
	  /* Initialize LED */
	  GPIO_ConfigurePin(LED_PORT, LED_PIN, OUTPUT, PID_GPIO, false);
	  GPIO_SetInactive(LED_PORT, LED_PIN);  /* LED off initially */
	  
	  #ifdef CFG_PRINTF
	      arch_printf("System initialized - waiting for button press...\n\r");
	  #endif
}

/**
 ****************************************************************************************
 * @brief Called when stack has completed the process of stopping advertising.
 *
 * @param[in] None.
 *
 * @return None.
 ****************************************************************************************
 */
void user_on_adv_undirect_complete(uint8_t status)
{
    #ifdef CFG_PRINTF
	      arch_printf("\n\r%s - status: %d", __FUNCTION__, status);
	  #endif
	
	  /* Advertisement burst completed - waiting for next button press */
	  if (status != 0)
	  {
        #ifdef CFG_PRINTF
	          arch_printf("\n\rBurst complete - Press button for next advertisement");
	      #endif
    }
}

/**
 ****************************************************************************************
 * @brief Called on connection event.
 *
 * @param[in] Pointer to gapc_connection_req_ind message.
 *
 * @return None. 
 ****************************************************************************************
 */
void user_on_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
    #ifdef CFG_PRINTF
	      arch_printf("\n\r%s - connection_idx: %d", __FUNCTION__, connection_idx);
	  #endif

	  if (connection_idx != GAP_INVALID_CONIDX)
    {
        // Stop advertising now we are connected
        #ifdef CFG_PRINTF
            arch_printf("\n\rConnected! Stopping advertisement.");
        #endif
        
			  app_easy_gap_advertise_with_timeout_stop();  
			  
			  // Enable the created profiles/services
        app_prf_enable(connection_idx);
    }
    else
    {
       // No connection has been established, restart advertising
       #ifdef CFG_PRINTF
           arch_printf("\n\rConnection failed. Waiting for button press.");
       #endif
       start_advertising();
    }
}

/**
 ****************************************************************************************
 * @brief Called on disconnection event.
 *
 * @param[in] Pointer to gapc_disconnect_ind message. 
 *
 * @return None. 
 ****************************************************************************************
 */
void user_on_disconnect(struct gapc_disconnect_ind const *param)
{
    #ifdef CFG_PRINTF
	      arch_printf("\n\r%s - Disconnected. Waiting for button press.", __FUNCTION__);
	  #endif

  	/* Wait for button press to restart advertising */
}

/// @} APP
