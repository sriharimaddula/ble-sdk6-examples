/**
 ****************************************************************************************
 *
 * @file user_ble_burst_adv.h
 *
 * @brief BLE burst advertising project header file.
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

#ifndef _USER_BLE_BURST_ADV_H_
#define _USER_BLE_BURST_ADV_H_

/**
 ****************************************************************************************
 * @addtogroup APP
 * @ingroup RICOW
 *
 * @brief 
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwble_config.h"
#include "app_task.h"                  // application task
#include "gapc_task.h"                 // gap functions and messages
#include "gapm_task.h"                 // gap functions and messages
#include "app.h"                       // application definitions
#include "app_easy_gap.h"              // easy gap functions
#include "co_error.h"                  // error code definitions

#ifdef CFG_PRINTF
    #include "arch_console.h"
#endif

/****************************************************************************
Add here supported profiles' application header files.
i.e.
#if (BLE_DIS_SERVER)
#include "app_dis.h"
#include "app_dis_task.h"
#endif
*****************************************************************************/

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/* Placeholder UUID / identifier macros for prototype GATT implementation.
    Replace these with real 128-bit UUIDs and use the SDK's attm_db_128 APIs
    when creating the real profile.
*/
#define UUID_HANDSHAKE_SERVICE_ID        0xA1A1A1A1
#define UUID_TIMESTAMP_SERVICE_ID        0xA1A1A1A2
#define UUID_TIMESTAMP_REQ_CHAR_ID       0xA1A1A1A3
#define UUID_TIMESTAMP_RESP_CHAR_ID      0xA1A1A1A4
#define UUID_UPDATE_SERVICE_ID           0xA1A1A1A5

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

void user_on_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param);
void user_on_disconnect(struct gapc_disconnect_ind const *param);
void user_on_set_dev_config_complete(void);
void user_on_adv_undirect_complete(uint8_t status);

/* Prototype handlers for timestamp requests/notify streaming (implemented in .c)
    - `from_index` is the requested start index for timestamps (uint32_t)
*/
void handle_timestamp_request(uint32_t from_index);
void notify_timestamp_chunk(void);

/// @} APP

#endif // _USER_BLE_BURST_ADV_H_
