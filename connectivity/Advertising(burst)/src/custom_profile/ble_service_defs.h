/**
 ****************************************************************************************
 *
 * @file ble_service_defs.h
 *
 * @brief BLE service UUID definitions and data structures for dynamic GATT registration
 *        Used with direct GATTM_ADD_SVC_REQ messaging (not static CUSTS1 profile)
 *
 ****************************************************************************************
 */

#ifndef _BLE_SERVICE_DEFS_H_
#define _BLE_SERVICE_DEFS_H_

/*
 * INCLUDE FILES
 */

#include <stdbool.h>
#include <stdint.h>

/*
 * DEFINES
 */

/* UUIDs (128-bit) taken from GoLangServer `ble_constants.go` (stored LSB-first for ATT DB) */

/* Handshake Service: 0000180D-0000-1000-8000-00805f9b34fb */
#define DEF_HSVC_UUID_128 {0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x0D,0x18,0x00,0x00}
#define DEF_HSVC_CHAR_UUID_128 {0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x1D,0x18,0x00,0x00}

/* Timestamp Request Service: 17b96791-4c66-4c01-9633-31b191590001 */
#define DEF_TSVC_UUID_128  {0x01,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x91,0x67,0xB9,0x17}
#define DEF_TSVC_REQ_UUID_128  {0x04,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x94,0x67,0xB9,0x17}

/* Timestamp Response Service: 17b96792-4c66-4c01-9633-31b191590002 */
#define DEF_TSVC_RESP_SVC_UUID_128 {0x02,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x92,0x67,0xB9,0x17}
#define DEF_TSVC_RESP_UUID_128 {0x05,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x95,0x67,0xB9,0x17}

/* Update Service: 16b96793-4c66-4c01-9633-31b191590003 */
#define DEF_UPDATE_SVC_UUID_128 {0x03,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x93,0x67,0xB9,0x16}
#define DEF_UPDATE_CHAR_UUID_128 {0x05,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x95,0x67,0xB9,0x17}

#define DEF_HSVC_CHAR_LEN      20   /* Max write size for handshake data */
#define DEF_TSVC_REQ_CHAR_LEN   4   /* 4-byte from_index */
#define DEF_TSVC_RESP_CHAR_LEN 20   /* 20-byte chunks (5 timestamps) */
#define DEF_UPDATE_CHAR_LEN     4   /* 4-byte epoch time */

#define MAX_REMINDERS 10            /* Increased to 10 to support actual schedule storage */

/* Reminder structure for scheduling (optimized for memory) */
typedef struct {
    uint16_t minutes_since_midnight;  /* 0-1439 (24*60-1), saves RAM vs hour+minute */
} reminder_t;

/* Device state structure */
typedef struct {
    uint32_t system_time;                    /* Current system time (epoch seconds) */
    uint8_t reminder_count;                  /* Number of active reminders (uint8 sufficient) */
    bool handshake_complete;                /* Set after successful handshake */
    reminder_t reminders[MAX_REMINDERS];    /* Reminder storage */
} device_state_t;

#endif // _BLE_SERVICE_DEFS_H_
