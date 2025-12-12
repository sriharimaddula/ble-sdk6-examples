/**
 ****************************************************************************************
 *
 * @file user_custs1_def.h
 *
 * @brief Custom Server 1 (CUSTS1) profile database definitions for Advertising(burst)
 *
 ****************************************************************************************
 */

#ifndef _USER_CUSTS1_DEF_H_
#define _USER_CUSTS1_DEF_H_

/*
 * INCLUDE FILES
 */

#include "attm_db_128.h"

/*
 * DEFINES
 */

/* UUIDs (128-bit) taken from GoLangServer `ble_constants.go` (stored LSB-first for ATT DB) */
#define DEF_TSVC_UUID_128  {0x01,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x01,0x4C,0x66,0x4C,0x91,0x67,0xB9,0x17}
#define DEF_TSVC_REQ_UUID_128  {0x04,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x04,0x4C,0x66,0x4C,0x94,0x67,0xB9,0x17}
#define DEF_TSVC_RESP_UUID_128 {0x05,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x05,0x4C,0x66,0x4C,0x95,0x67,0xB9,0x17}

#define DEF_UPDATE_SVC_UUID_128 {0x03,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x03,0x4C,0x66,0x4C,0x93,0x67,0xB9,0x16}
#define DEF_UPDATE_CHAR_UUID_128 {0x05,0x00,0x59,0x91,0xB1,0x31,0x33,0x96,0x05,0x4C,0x66,0x4C,0x95,0x67,0xB9,0x17}

#define DEF_TSVC_REQ_CHAR_LEN   4
#define DEF_TSVC_RESP_CHAR_LEN 20
#define DEF_UPDATE_CHAR_LEN     4

/* CUSTS1 index enum */
enum
{
    /* Timestamp Service */
    TSVC_IDX_SVC = 0,
    TSVC_IDX_TIMESTAMP_REQ_CHAR,
    TSVC_IDX_TIMESTAMP_REQ_VAL,
    TSVC_IDX_TIMESTAMP_RESP_CHAR,
    TSVC_IDX_TIMESTAMP_RESP_VAL,
    TSVC_IDX_TIMESTAMP_RESP_NTF_CFG,
    TSVC_IDX_TIMESTAMP_RESP_USER_DESC,

    /* Update Service */
    USVC_IDX_SVC,
    USVC_IDX_UPDATE_CHAR,
    USVC_IDX_UPDATE_VAL,
    USVC_IDX_UPDATE_USER_DESC,

    CUSTS1_IDX_NB
};

#endif // _USER_CUSTS1_DEF_H_
