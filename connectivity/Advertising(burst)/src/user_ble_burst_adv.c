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
#include "gattc_task.h"
#include "gattm_task.h"
#include "prf_utils.h"
#include "attm.h"
#include "custom_profile/user_custs1_def.h"
// #include "rtc.h"  // RTC support disabled - add rtc.c to Keil project to enable


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

/* Device state storage (reminders, system time) */
static device_state_t device_state __attribute__((section(".bss."))) = {0};

/* Streaming state for timestamp notifications */
static uint32_t ts_stream_idx = 0; /* next index to send */
static timer_hnd ts_stream_timer_id __attribute__((section(".bss.")));

/* GATT service handle tracking (set when service is registered) */
static uint16_t handshake_service_start_handle = 0;
static uint16_t timestamp_req_service_start_handle = 0;
static uint16_t timestamp_resp_service_start_handle = 0;
static uint16_t update_service_start_handle = 0;

/* Forward declarations */
void handle_handshake_write(const uint8_t *data, uint16_t length);
void handle_timestamp_request(uint32_t from_index);
void handle_update_write(const uint8_t *data, uint16_t length);
void notify_timestamp_chunk(void);
static void register_custom_services(void);

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

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
 * @brief Start advertising and also start timer that when fires will stop advertising.
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

  	#ifdef CFG_PRINTF
	      arch_printf("\n\radv_period_ticks: %d", adv_period_ticks);
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
	      arch_printf("\n\r%s", __FUNCTION__);
	  #endif
	
    default_app_on_set_dev_config_complete();
	
	  start_advertising();
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
	
	  /* This callback is triggered when the application stops advertising and a burst 
	     is complete and also when a connection has been created. We only want to restart
       the burst period timer in the first case and can detect the reason for the call 
       using the status parameter.	*/
	  if (status != 0)
	  {
        /* Start timer which when fires will restart advertising - adjusted for time 
	         spent advertising so that time between one busrt starting and the next starting
	         is what the user set via BURST_REPEAT_PERIOD_ms */
	      adv_burst_timer_id = app_easy_timer(BURST_REPEAT_PERIOD_TICKS - adv_period_ticks, 
	                                          start_advertising);
	
        #ifdef CFG_PRINTF
	          arch_printf("\n\radv_burst_timer_delay: %d", 
					                    BURST_REPEAT_PERIOD_TICKS - adv_period_ticks);
            arch_printf("\n\radv_burst_timer_id: %d", adv_burst_timer_id);
	      #endif
    }
}

/**
 ****************************************************************************************
 * @brief Register Handshake Service (reminders + system time).
 * 
 * Mobile app looks for service UUID: 0000180D-0000-1000-8000-00805f9b34fb
 * Uses BleConstants.CONFIGURE_SERVICE_UUID
 ****************************************************************************************
 */
static void register_handshake_service(void)
{
    static const uint8_t handshake_svc_uuid[] = DEF_HSVC_UUID_128;
    static const uint8_t handshake_char_uuid[] = DEF_HSVC_CHAR_UUID_128;
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[REGISTER] Handshake/Configure Service UUID: 0000180D-0000-1000-8000-00805f9b34fb");
    #endif
    
    const uint8_t num_atts = 3; // 1 svc + 1 char_decl + 1 char_val
    
    struct gattm_add_svc_req *req = KE_MSG_ALLOC_DYN(GATTM_ADD_SVC_REQ,
                                                      TASK_GATTM,
                                                      TASK_APP,
                                                      gattm_add_svc_req,
                                                      num_atts * sizeof(struct gattm_att_desc));
    
    req->svc_desc.start_hdl = 0;
    req->svc_desc.task_id = TASK_APP;
    req->svc_desc.perm = (PERM_MASK_SVC_UUID_LEN & PERM_UUID_128) | 
                         (PERM_MASK_SVC_PRIMARY & PERM_RIGHT_ENABLE);
    req->svc_desc.nb_att = num_atts;
    memcpy(req->svc_desc.uuid, handshake_svc_uuid, ATT_UUID_128_LEN);
    
    // Attribute 1: Characteristic declaration
    req->svc_desc.atts[1].uuid[0] = (ATT_DECL_CHARACTERISTIC & 0xFF);
    req->svc_desc.atts[1].uuid[1] = ((ATT_DECL_CHARACTERISTIC >> 8) & 0xFF);
    req->svc_desc.atts[1].perm = PERM(RD, ENABLE);
    req->svc_desc.atts[1].max_len = 0;
    
    // Attribute 2: Characteristic value (WRITE)
    memcpy(req->svc_desc.atts[2].uuid, handshake_char_uuid, ATT_UUID_128_LEN);
    req->svc_desc.atts[2].perm = PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE);
    req->svc_desc.atts[2].max_len = DEF_HSVC_CHAR_LEN;
    
    ke_msg_send(req);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] Handshake service registration sent");
    #endif
}

/**
 ****************************************************************************************
 * @brief Register Timestamp Request Service (WRITE characteristic).
 ****************************************************************************************
 */
static void register_timestamp_request_service(void)
{
    static const uint8_t timestamp_req_svc_uuid[] = DEF_TSVC_UUID_128;  // ...590001
    static const uint8_t timestamp_req_char_uuid[] = DEF_TSVC_REQ_UUID_128;
    
    const uint8_t num_atts = 3; // 1 svc + 1 char_decl + 1 char_val
    
    struct gattm_add_svc_req *req = KE_MSG_ALLOC_DYN(GATTM_ADD_SVC_REQ,
                                                      TASK_GATTM,
                                                      TASK_APP,
                                                      gattm_add_svc_req,
                                                      num_atts * sizeof(struct gattm_att_desc));
    
    req->svc_desc.start_hdl = 0;
    req->svc_desc.task_id = TASK_APP;
    req->svc_desc.perm = (PERM_MASK_SVC_UUID_LEN & PERM_UUID_128) | 
                         (PERM_MASK_SVC_PRIMARY & PERM_RIGHT_ENABLE);
    req->svc_desc.nb_att = num_atts;
    memcpy(req->svc_desc.uuid, timestamp_req_svc_uuid, ATT_UUID_128_LEN);
    
    // Attribute 1: Request characteristic declaration
    req->svc_desc.atts[1].uuid[0] = (ATT_DECL_CHARACTERISTIC & 0xFF);
    req->svc_desc.atts[1].uuid[1] = ((ATT_DECL_CHARACTERISTIC >> 8) & 0xFF);
    req->svc_desc.atts[1].perm = PERM(RD, ENABLE);
    req->svc_desc.atts[1].max_len = 0;
    
    // Attribute 2: Request characteristic value (WRITE)
    memcpy(req->svc_desc.atts[2].uuid, timestamp_req_char_uuid, ATT_UUID_128_LEN);
    req->svc_desc.atts[2].perm = PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE);
    req->svc_desc.atts[2].max_len = DEF_TSVC_REQ_CHAR_LEN;
    
    ke_msg_send(req);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] Timestamp REQUEST service registration sent");
    #endif
}

/**
 ****************************************************************************************
 * @brief Register Timestamp Response Service (NOTIFY characteristic).
 ****************************************************************************************
 */
static void register_timestamp_response_service(void)
{
    static const uint8_t timestamp_resp_svc_uuid[] = DEF_TSVC_RESP_SVC_UUID_128;  // ...590002
    static const uint8_t timestamp_resp_char_uuid[] = DEF_TSVC_RESP_UUID_128;
    
    const uint8_t num_atts = 3; // 1 svc + 1 char_decl + 1 char_val + 1 ccc = 4
    
    struct gattm_add_svc_req *req = KE_MSG_ALLOC_DYN(GATTM_ADD_SVC_REQ,
                                                      TASK_GATTM,
                                                      TASK_APP,
                                                      gattm_add_svc_req,
                                                      4 * sizeof(struct gattm_att_desc));
    
    req->svc_desc.start_hdl = 0;
    req->svc_desc.task_id = TASK_APP;
    req->svc_desc.perm = (PERM_MASK_SVC_UUID_LEN & PERM_UUID_128) | 
                         (PERM_MASK_SVC_PRIMARY & PERM_RIGHT_ENABLE);
    req->svc_desc.nb_att = 4;
    memcpy(req->svc_desc.uuid, timestamp_resp_svc_uuid, ATT_UUID_128_LEN);
    
    // Attribute 1: Response characteristic declaration
    req->svc_desc.atts[1].uuid[0] = (ATT_DECL_CHARACTERISTIC & 0xFF);
    req->svc_desc.atts[1].uuid[1] = ((ATT_DECL_CHARACTERISTIC >> 8) & 0xFF);
    req->svc_desc.atts[1].perm = PERM(RD, ENABLE);
    req->svc_desc.atts[1].max_len = 0;
    
    // Attribute 2: Response characteristic value (NOTIFY)
    memcpy(req->svc_desc.atts[2].uuid, timestamp_resp_char_uuid, ATT_UUID_128_LEN);
    req->svc_desc.atts[2].perm = PERM(NTF, ENABLE);
    req->svc_desc.atts[2].max_len = DEF_TSVC_RESP_CHAR_LEN;
    
    // Attribute 3: CCC descriptor for notifications
    req->svc_desc.atts[3].uuid[0] = (ATT_DESC_CLIENT_CHAR_CFG & 0xFF);
    req->svc_desc.atts[3].uuid[1] = ((ATT_DESC_CLIENT_CHAR_CFG >> 8) & 0xFF);
    req->svc_desc.atts[3].perm = PERM(RD, ENABLE) | PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE);
    req->svc_desc.atts[3].max_len = sizeof(uint16_t);
    
    ke_msg_send(req);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] Timestamp RESPONSE service registration sent");
    #endif
}

/**
 ****************************************************************************************
 * @brief Register Update Service (clock update).
 ****************************************************************************************
 */
static void register_update_service(void)
{
    static const uint8_t update_svc_uuid[] = DEF_UPDATE_SVC_UUID_128;
    static const uint8_t update_char_uuid[] = DEF_UPDATE_CHAR_UUID_128;
    
    const uint8_t num_atts = 3; // 1 svc + 1 char_decl + 1 char_val
    
    struct gattm_add_svc_req *req = KE_MSG_ALLOC_DYN(GATTM_ADD_SVC_REQ,
                                                      TASK_GATTM,
                                                      TASK_APP,
                                                      gattm_add_svc_req,
                                                      num_atts * sizeof(struct gattm_att_desc));
    
    req->svc_desc.start_hdl = 0;
    req->svc_desc.task_id = TASK_APP;
    req->svc_desc.perm = (PERM_MASK_SVC_UUID_LEN & PERM_UUID_128) | 
                         (PERM_MASK_SVC_PRIMARY & PERM_RIGHT_ENABLE);
    req->svc_desc.nb_att = num_atts;
    memcpy(req->svc_desc.uuid, update_svc_uuid, ATT_UUID_128_LEN);
    
    // Attribute 1: Characteristic declaration
    req->svc_desc.atts[1].uuid[0] = (ATT_DECL_CHARACTERISTIC & 0xFF);
    req->svc_desc.atts[1].uuid[1] = ((ATT_DECL_CHARACTERISTIC >> 8) & 0xFF);
    req->svc_desc.atts[1].perm = PERM(RD, ENABLE);
    req->svc_desc.atts[1].max_len = 0;
    
    // Attribute 2: Characteristic value (WRITE)
    memcpy(req->svc_desc.atts[2].uuid, update_char_uuid, ATT_UUID_128_LEN);
    req->svc_desc.atts[2].perm = PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE);
    req->svc_desc.atts[2].max_len = DEF_UPDATE_CHAR_LEN;
    
    ke_msg_send(req);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] Update service registration sent");
    #endif
}

/**
 ****************************************************************************************
 * @brief Register all custom GATT services dynamically.
 ****************************************************************************************
 */
static void register_custom_services(void)
{
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] Registering custom services...");
    #endif
    
    register_handshake_service();
    register_timestamp_request_service();
    register_timestamp_response_service();
    register_update_service();
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
			  app_easy_gap_advertise_with_timeout_stop();  
			  
			  // Register custom GATT services dynamically
			  register_custom_services();
			  
			  // Enable the created profiles/services
        app_prf_enable(connection_idx);
    }
    else
    {
       // No connection has been established, restart advertising
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
	      arch_printf("\n\r%s", __FUNCTION__);
	  #endif

  	/* Restart burst advertising */
	  start_advertising();
}

/*
 * GATT Write Handlers
 */

/**
 ****************************************************************************************
 * @brief Handle handshake write (reminders + system time)
 * 
 * Protocol (from protocol doc):
 *   First packet (8 bytes):  [UInt32 BE reminder_count][UInt32 BE system_time]
 *   Rest packets (20 bytes): [UInt32 BE...] timestamps (max 5 per packet)
 ****************************************************************************************
 */
void handle_handshake_write(const uint8_t *data, uint16_t length)
{
    static uint32_t expected_reminder_count = 0;
    static uint32_t reminders_received = 0;
    static bool first_packet = true;
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[HANDSHAKE] Received %d bytes (first_packet=%d)", length, first_packet);
    #endif
    
    if (first_packet && length >= 8)
    {
        // First packet: extract reminder count and system time
        expected_reminder_count = ((uint32_t)data[0] << 24) |
                                   ((uint32_t)data[1] << 16) |
                                   ((uint32_t)data[2] << 8)  |
                                   ((uint32_t)data[3]);
        
        device_state.system_time = ((uint32_t)data[4] << 24) |
                                    ((uint32_t)data[5] << 16) |
                                    ((uint32_t)data[6] << 8)  |
                                    ((uint32_t)data[7]);
        
        #ifdef CFG_PRINTF
            arch_printf("\n\r[HANDSHAKE] Reminder count: %u, System time: %u", 
                       expected_reminder_count, device_state.system_time);
        #endif
        
        // Limit to MAX_REMINDERS
        if (expected_reminder_count > MAX_REMINDERS)
        {
            expected_reminder_count = MAX_REMINDERS;
            #ifdef CFG_PRINTF
                arch_printf("\n\r[HANDSHAKE] WARNING: Reminder count limited to %d", MAX_REMINDERS);
            #endif
        }
        
        device_state.reminder_count = 0;
        reminders_received = 0;
        first_packet = false;
        
        // If no reminders expected, we're done
        if (expected_reminder_count == 0)
        {
            device_state.handshake_complete = true;
            first_packet = true;
            #ifdef CFG_PRINTF
                arch_printf("\n\r[HANDSHAKE] Complete (no reminders)");
            #endif
        }
    }
    else if (!first_packet)
    {
        // Subsequent packets: parse reminder timestamps (minutes since midnight, 4 bytes each)
        uint16_t offset = 0;
        
        while (offset + 4 <= length && reminders_received < expected_reminder_count)
        {
            uint32_t minutes_since_midnight = ((uint32_t)data[offset] << 24) |
                                               ((uint32_t)data[offset + 1] << 16) |
                                               ((uint32_t)data[offset + 2] << 8)  |
                                               ((uint32_t)data[offset + 3]);
            
            // Validate minutes (0-1439 for 24 hours)
            if (minutes_since_midnight < 1440)
            {
                if (device_state.reminder_count < MAX_REMINDERS)
                {
                    device_state.reminders[device_state.reminder_count].minutes_since_midnight = 
                        (uint16_t)minutes_since_midnight;
                    device_state.reminder_count++;
                }
            }
            
            reminders_received++;
            offset += 4;
            
            #ifdef CFG_PRINTF
                uint8_t hour = minutes_since_midnight / 60;
                uint8_t minute = minutes_since_midnight % 60;
                arch_printf("\n\r[HANDSHAKE] Reminder %u: %02d:%02d", 
                           reminders_received, hour, minute);
            #endif
        }
        
        // Check if all reminders received
        if (reminders_received >= expected_reminder_count)
        {
            device_state.handshake_complete = true;
            first_packet = true;
            #ifdef CFG_PRINTF
                arch_printf("\n\r[HANDSHAKE] Complete - %u reminders stored", device_state.reminder_count);
            #endif
        }
    }
    else
    {
        #ifdef CFG_PRINTF
            arch_printf("\n\r[HANDSHAKE] ERROR: Invalid packet (length=%d, first=%d)", 
                       length, first_packet);
        #endif
    }
}

/**
 ****************************************************************************************
 * @brief Handle update write (clock update).
 ****************************************************************************************
 */
void handle_update_write(const uint8_t *data, uint16_t length)
{
    if (length >= 4)
    {
        uint32_t new_epoch = ((uint32_t)data[0] << 24) |
                              ((uint32_t)data[1] << 16) |
                              ((uint32_t)data[2] << 8)  |
                              ((uint32_t)data[3]);
        
        device_state.system_time = new_epoch;
        
        #ifdef CFG_PRINTF
            arch_printf("\n\r[UPDATE] Clock update: epoch=%u", new_epoch);
        #endif
    }
    else
    {
        #ifdef CFG_PRINTF
            arch_printf("\n\r[UPDATE] ERROR: Invalid length (%d bytes, expected 4)", length);
        #endif
    }
}

/*
 * Timestamp request handling & chunked notify streaming (prototype)
 * NOTE: For production, replace with actual persistent storage query.
 * Currently uses dummy data with 10 test timestamps.
 */
void handle_timestamp_request(uint32_t from_index)
{
    // For testing: assume we have 10 dummy timestamps available
    const uint32_t total_timestamps = 10;
    
    /* Clamp start index */
    if (from_index >= total_timestamps) {
        ts_stream_idx = total_timestamps; /* nothing to send */
        #ifdef CFG_PRINTF
            arch_printf("\n\r[TIMESTAMP] Request index %u exceeds available (%u), terminating stream",
                       from_index, total_timestamps);
        #endif
        return;
    }
    
    ts_stream_idx = from_index;
    uint32_t remaining_count = total_timestamps - from_index;
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[TIMESTAMP] Request received: from_index=%u, remaining=%u",
                   from_index, remaining_count);
    #endif

    /* FIRST: Send count of remaining timestamps (4 bytes, big-endian) - matches GoLang server */
    uint8_t count_buf[4];
    count_buf[0] = (remaining_count >> 24) & 0xFF;
    count_buf[1] = (remaining_count >> 16) & 0xFF;
    count_buf[2] = (remaining_count >> 8) & 0xFF;
    count_buf[3] = remaining_count & 0xFF;
    
    struct gattc_send_evt_cmd *count_req = KE_MSG_ALLOC_DYN(GATTC_SEND_EVT_CMD,
                                                             TASK_GATTC,
                                                             TASK_APP,
                                                             gattc_send_evt_cmd,
                                                             4);
    count_req->operation = GATTC_NOTIFY;
    count_req->seq_num = 0;
    count_req->handle = timestamp_resp_service_start_handle + 2;  // Response service notify char
    count_req->length = 4;
    memcpy(count_req->value, count_buf, 4);
    ke_msg_send(count_req);
    
    #ifdef CFG_PRINTF
        arch_printf("\n\r[TIMESTAMP] Sent count notification: %u remaining", remaining_count);
    #endif

    /* Start sending timestamp chunks after 1 second */
    ts_stream_timer_id = app_easy_timer(MS_TO_TIMERUNITS(1000), notify_timestamp_chunk);
    #ifdef CFG_PRINTF
        arch_printf("\n\r[TIMESTAMP] First chunk scheduled in 1s, timer_id=%d", ts_stream_timer_id);
    #endif
}

/* Sends up to 5 timestamps per notification (20 bytes) via CUSTS1_VAL_NTF_REQ.
   Each notification contains 4-byte big-endian Unix timestamps.
   Protocol: 1s delay between packets to avoid overwhelming mobile client.
   
   NOTE: For production, replace with actual persistent timestamp storage.
   Currently generates dummy data for testing. */
void notify_timestamp_chunk(void)
{
    // For testing: assume we have 10 dummy timestamps to send
    const uint32_t total_timestamps = 10;
    
    if (ts_stream_idx >= total_timestamps) {
        #ifdef CFG_PRINTF
            arch_printf("\n\r[NOTIFY] Stream complete. Sent %u timestamps", total_timestamps);
        #endif
        return;
    }

    uint8_t buf[20];
    uint8_t pos = 0;
    uint8_t sent = 0;
    uint32_t chunk_start_idx = ts_stream_idx;

    /* Pack up to 5 timestamps (4 bytes each = 20 bytes max) */
    while (pos + 4 <= sizeof(buf) && ts_stream_idx < total_timestamps && sent < 5) {
        // Generate dummy timestamp: base time + index offset (for testing)
        uint32_t ts = 1622505600u + (ts_stream_idx * 3600);  // hourly increments
        ts_stream_idx++;
        
        /* Big-endian encoding as per GoLangServer protocol */
        buf[pos++] = (uint8_t)((ts >> 24) & 0xFF);
        buf[pos++] = (uint8_t)((ts >> 16) & 0xFF);
        buf[pos++] = (uint8_t)((ts >> 8) & 0xFF);
        buf[pos++] = (uint8_t)(ts & 0xFF);
        sent++;
    }

    #ifdef CFG_PRINTF
        arch_printf("\n\r[NOTIFY] Sending chunk: indices [%u-%u], %d timestamps, %d bytes",
                   chunk_start_idx, ts_stream_idx - 1, sent, pos);
        arch_printf("\n\r[NOTIFY] DATA: ");
        for (uint8_t i = 0; i < pos; i++) arch_printf("%02X ", buf[i]);
    #endif

    /* Allocate and send GATT notification using gattc_send_evt_cmd */
    struct gattc_send_evt_cmd *req = KE_MSG_ALLOC_DYN(GATTC_SEND_EVT_CMD,
                                                       TASK_GATTC,
                                                       TASK_APP,
                                                       gattc_send_evt_cmd,
                                                       pos);
    req->operation = GATTC_NOTIFY;
    req->seq_num = 0;
    req->handle = timestamp_resp_service_start_handle + 2;  // Response service notify char
    req->length = pos;
    memcpy(req->value, buf, pos);
    ke_msg_send(req);

    #ifdef CFG_PRINTF
        arch_printf("\n\r[NOTIFY] Notification sent, handle=%d, remaining=%u",
                   req->handle, (total_timestamps - ts_stream_idx));
    #endif

    /* Schedule next chunk after ~1 second if there are more timestamps */
    if (ts_stream_idx < total_timestamps) {
        ts_stream_timer_id = app_easy_timer(MS_TO_TIMERUNITS(1000), notify_timestamp_chunk);
        #ifdef CFG_PRINTF
            arch_printf("\n\r[NOTIFY] Next chunk scheduled in 1s, timer_id=%d", ts_stream_timer_id);
        #endif
    }
}

/* RTC support disabled - to enable, add sdk/platform/driver/rtc/rtc.c to Keil project */
#if 0
/**
 ****************************************************************************************
 * @brief Convert Unix epoch (seconds) to RTC time/calendar structures (UTC)
 *
 * @param[in] epoch  Unix epoch seconds
 * @param[out] t     rtc_time_t pointer to populate (hour,min,sec,hsec)
 * @param[out] c     rtc_calendar_t pointer to populate (year,month,mday,wday)
 *
 * @note Simple conversion (UTC) sufficient for device RTC set from remote epoch.
 ****************************************************************************************
 */
static void epoch_to_rtc(uint32_t epoch, rtc_time_t *t, rtc_calendar_t *c)
{
    uint32_t days = epoch / 86400u;
    uint32_t rem = epoch % 86400u;

    t->hour = rem / 3600u;
    t->minute = (rem % 3600u) / 60u;
    t->sec = rem % 60u;
    t->hsec = 0; /* sub-second resolution not provided */

    /* Compute year */
    uint32_t year = 1970;
    while (1) {
        uint32_t isleap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        uint32_t days_in_year = 365 + isleap;
        if (days < days_in_year) break;
        days -= days_in_year;
        year++;
    }

    /* Month lengths for the computed year */
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

    /* tm_wday: 0 = Sunday. 1970-01-01 was a Thursday (4). */
    c->wday = (uint8_t)(( (epoch / 86400u) + 4u) % 7u);
}
#endif  /* RTC disabled */

/**
 ****************************************************************************************
 * @brief Message handler for CUSTS1 write events (Timestamp request / Update write).
 *        This is called whenever a mobile client writes to any custom characteristic.
 *        Implements the protocol defined in GoLangServer.
 *
 * @param[in] msgid    Message ID (should be CUSTS1_VAL_WRITE_IND for writes)
 * @param[in] param    Pointer to CUSTS1_VAL_WRITE_IND message containing write data
 * @param[in] dest_id  Destination task
 * @param[in] src_id   Source task
 *
 * @return None.
 ****************************************************************************************
 */
void user_catch_rest_hndl(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    #ifdef CFG_PRINTF
        arch_printf("\n\r[GATT] msgid=0x%04X, dest=0x%04X, src=0x%04X", msgid, dest_id, src_id);
        
        // Debug: Show which service handles are active
        static bool once = false;
        if (!once && handshake_service_start_handle && timestamp_req_service_start_handle && 
            timestamp_resp_service_start_handle && update_service_start_handle)
        {
            arch_printf("\n\r[DEBUG] Handshake write handle: %d", handshake_service_start_handle + 2);
            arch_printf("\n\r[DEBUG] Timestamp REQUEST write handle: %d", timestamp_req_service_start_handle + 2);
            arch_printf("\n\r[DEBUG] Timestamp RESPONSE notify handle: %d", timestamp_resp_service_start_handle + 2);
            arch_printf("\n\r[DEBUG] Update write handle: %d", update_service_start_handle + 2);
            once = true;
        }
    #endif

    switch(msgid)
    {
        case GATTM_ADD_SVC_RSP:
        {
            struct gattm_add_svc_rsp const *rsp = (struct gattm_add_svc_rsp const *)(param);
            
            #ifdef CFG_PRINTF
                arch_printf("\n\r[GATT] Service added: start_handle=%d, status=0x%02X", rsp->start_hdl, rsp->status);
            #endif
            
            if (rsp->status == ATT_ERR_NO_ERROR)
            {
                // Store service handles in order: handshake, timestamp_req, timestamp_resp, update
                if (handshake_service_start_handle == 0)
                {
                    handshake_service_start_handle = rsp->start_hdl;
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] Handshake service registered at handle %d", handshake_service_start_handle);
                    #endif
                }
                else if (timestamp_req_service_start_handle == 0)
                {
                    timestamp_req_service_start_handle = rsp->start_hdl;
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] Timestamp REQUEST service registered at handle %d", timestamp_req_service_start_handle);
                    #endif
                }
                else if (timestamp_resp_service_start_handle == 0)
                {
                    timestamp_resp_service_start_handle = rsp->start_hdl;
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] Timestamp RESPONSE service registered at handle %d", timestamp_resp_service_start_handle);
                    #endif
                }
                else if (update_service_start_handle == 0)
                {
                    update_service_start_handle = rsp->start_hdl;
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] Update service registered at handle %d", update_service_start_handle);
                    #endif
                }
            }
            else
            {
                #ifdef CFG_PRINTF
                    arch_printf("\n\r[GATT] ERROR: Service registration failed!");
                #endif
            }
        } break;
        
        case GATTC_WRITE_REQ_IND:
        {
            struct gattc_write_req_ind const *msg = (struct gattc_write_req_ind const *)(param);

            #ifdef CFG_PRINTF
                arch_printf("\n\r[GATT] Write event received: handle=%d, length=%d", msg->handle, msg->length);
            #endif

            // Ignore writes if services not yet registered (spurious messages during connection)
            if (!handshake_service_start_handle || !timestamp_req_service_start_handle || 
                !timestamp_resp_service_start_handle || !update_service_start_handle)
            {
                #ifdef CFG_PRINTF
                    arch_printf("\n\r[GATT] WARNING: Write received before services registered, ignoring");
                #endif
                
                // Still send confirmation to avoid protocol errors
                struct gattc_write_cfm *cfm = KE_MSG_ALLOC(GATTC_WRITE_CFM,
                                                           src_id,
                                                           dest_id,
                                                           gattc_write_cfm);
                cfm->handle = msg->handle;
                cfm->status = ATT_ERR_NO_ERROR;
                ke_msg_send(cfm);
                break;
            }

            // Calculate actual characteristic handles from service start handles
            // Handshake service: [0]=svc, [1]=char_decl, [2]=val
            // Timestamp REQUEST service: [0]=svc, [1]=char_decl, [2]=val
            // Timestamp RESPONSE service: [0]=svc, [1]=char_decl, [2]=val, [3]=ccc
            // Update service: [0]=svc, [1]=char_decl, [2]=val
            uint16_t handshake_val_handle = handshake_service_start_handle + 2;
            uint16_t timestamp_req_handle = timestamp_req_service_start_handle + 2;
            uint16_t timestamp_resp_handle = timestamp_resp_service_start_handle + 2;
            uint16_t update_val_handle = update_service_start_handle + 2;

            #ifdef CFG_PRINTF
                arch_printf("\n\r[DEBUG] Write handle check: received=%d, expected_handshake=%d", 
                           msg->handle, handshake_val_handle);
            #endif

            if (msg->handle == handshake_val_handle)
            {
                /* Handshake: mobile writes reminders + system time */
                #ifdef CFG_PRINTF
                    arch_printf("\n\r[HANDSHAKE] Write received to handle %d, length=%d", msg->handle, msg->length);
                    arch_printf("\n\r[HANDSHAKE] Data: ");
                    for (uint16_t i = 0; i < msg->length && i < 20; i++) {
                        arch_printf("%02X ", msg->value[i]);
                    }
                #endif
                handle_handshake_write(msg->value, msg->length);
            }
            else if (msg->handle == timestamp_req_handle)
            {
                /* Timestamp Request: mobile requests timestamp stream from a specific index.
                   Expect 4-byte big-endian index (matches GoLangServer protocol) */
                if (msg->length >= 4)
                {
                    uint32_t from_index = ((uint32_t)msg->value[0] << 24) |
                                          ((uint32_t)msg->value[1] << 16) |
                                          ((uint32_t)msg->value[2] << 8)  |
                                          ((uint32_t)msg->value[3]);
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] Timestamp Request: index=0x%08X (%u)", from_index, from_index);
                    #endif
                    handle_timestamp_request(from_index);
                }
                else
                {
                    #ifdef CFG_PRINTF
                        arch_printf("\n\r[GATT] ERROR: Timestamp Request too short (%d bytes, expected 4)",
                                   msg->length);
                    #endif
                }
            }
            else if (msg->handle == update_val_handle)
            {
                /* Update service: mobile writes epoch time for clock update */
                #ifdef CFG_PRINTF
                    arch_printf("\n\r[GATT] Update write");
                #endif
                handle_update_write(msg->value, msg->length);
            }
            else
            {
                #ifdef CFG_PRINTF
                    arch_printf("\n\r[GATT] WARNING: Write to unknown handle %d", msg->handle);
                #endif
            }

            /* Send write confirmation */
            struct gattc_write_cfm *cfm = KE_MSG_ALLOC(GATTC_WRITE_CFM,
                                                       src_id,
                                                       dest_id,
                                                       gattc_write_cfm);
            cfm->handle = msg->handle;
            cfm->status = ATT_ERR_NO_ERROR;
            ke_msg_send(cfm);
            
            #ifdef CFG_PRINTF
                arch_printf("\n\r[DEBUG] Write confirmation sent for handle %d", msg->handle);
            #endif
        } break;
        
        case 0x0E11:  // GATTC_READ_REQ_IND (explicit hex value)
        case GATTC_READ_REQ_IND:
        {
            // Read requests during service discovery - send empty response
            struct gattc_read_cfm *cfm = KE_MSG_ALLOC_DYN(GATTC_READ_CFM,
                                                          src_id,
                                                          dest_id,
                                                          gattc_read_cfm,
                                                          0);
            struct gattc_read_req_ind const *req = (struct gattc_read_req_ind const *)(param);
            cfm->handle = req->handle;
            cfm->length = 0;
            cfm->status = ATT_ERR_NO_ERROR;
            ke_msg_send(cfm);
            
            #ifdef CFG_PRINTF
                arch_printf("\n\r[GATT] Read request handle=%d", req->handle);
            #endif
        } break;

        default:
        {
            #ifdef CFG_PRINTF
                arch_printf("\n\r[GATT] *** UNHANDLED MESSAGE *** ID: 0x%04X", msgid);
                
                // Try to interpret as write request to debug
                if (msgid == 0x0D00 || msgid == 0x1E0A) 
                {
                    struct gattc_write_req_ind const *msg = (struct gattc_write_req_ind const *)(param);
                    arch_printf("\n\r[DEBUG] Attempting to parse as write: handle=%d, length=%d", msg->handle, msg->length);
                    
                    // Show first 20 bytes of data
                    if (msg->length > 0 && msg->length < 100) {
                        arch_printf("\n\r[DEBUG] Data: ");
                        for (uint16_t i = 0; i < msg->length && i < 20; i++) {
                            arch_printf("%02X ", msg->value[i]);
                        }
                    }
                }
            #endif
        } break;
    }
}

/// @} APP
