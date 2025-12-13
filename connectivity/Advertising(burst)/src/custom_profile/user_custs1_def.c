/**
 ****************************************************************************************
 *
 * @file user_custs1_def.c
 *
 * @brief Custom Server 1 (CUSTS1) profile database definitions for Advertising(burst)
 *
 ****************************************************************************************
 */

#include <stdint.h>
#include "co_utils.h"
#include "prf_types.h"
#include "attm_db_128.h"
#include "user_custs1_def.h"

/* Attribute specifications */
static const uint16_t att_decl_svc       = ATT_DECL_PRIMARY_SERVICE;
static const uint16_t att_decl_char      = ATT_DECL_CHARACTERISTIC;
static const uint16_t att_desc_cfg       = ATT_DESC_CLIENT_CHAR_CFG;
static const uint16_t att_desc_user_desc = ATT_DESC_CHAR_USER_DESCRIPTION;

/* Timestamp service UUIDs */
static const att_svc_desc128_t tsvc_svc = DEF_TSVC_UUID_128;
static const uint8_t TSVC_REQ_UUID_128[ATT_UUID_128_LEN]  = DEF_TSVC_REQ_UUID_128;
static const uint8_t TSVC_RESP_UUID_128[ATT_UUID_128_LEN] = DEF_TSVC_RESP_UUID_128;

/* Update service UUIDs */
static const att_svc_desc128_t usvc_svc = DEF_UPDATE_SVC_UUID_128;
static const uint8_t USVC_UPDATE_UUID_128[ATT_UUID_128_LEN] = DEF_UPDATE_CHAR_UUID_128;

/* Services list and sizes */
/* NOTE: Custom profile database disabled - using direct GATT messaging instead.
 * These definitions are kept for reference but not used at runtime. We handle
 * GATT operations directly using GATTC_WRITE_REQ_IND and GATTC_SEND_EVT_CMD.
 * The attribute handles (TSVC_IDX_*) are still used to identify characteristics.
 */

#if 0  /* Disabled - not using attm_db profile system */
const uint8_t custs1_services[]  = {TSVC_IDX_SVC, USVC_IDX_SVC, CUSTS1_IDX_NB};
const uint8_t custs1_services_size = ARRAY_LEN(custs1_services) - 1;
const uint16_t custs1_att_max_nb = CUSTS1_IDX_NB;

/* Full CUSTS1 Database Description - used by the ATT DB loader */
const struct attm_desc_128 custs1_att_db[CUSTS1_IDX_NB] =
{
    /* Timestamp Service Declaration */
    [TSVC_IDX_SVC] = {(uint8_t*)&att_decl_svc, ATT_UUID_128_LEN, PERM(RD, ENABLE),
                      sizeof(tsvc_svc), sizeof(tsvc_svc), (uint8_t*)&tsvc_svc},

    /* Timestamp Request Characteristic Declaration */
    [TSVC_IDX_TIMESTAMP_REQ_CHAR] = {(uint8_t*)&att_decl_char, ATT_UUID_16_LEN, PERM(RD, ENABLE), 0, 0, NULL},

    /* Timestamp Request Characteristic Value (Write, 4 bytes) */
    [TSVC_IDX_TIMESTAMP_REQ_VAL] = {TSVC_REQ_UUID_128, ATT_UUID_128_LEN, PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE),
                                    DEF_TSVC_REQ_CHAR_LEN, 0, NULL},

    /* Timestamp Response Characteristic Declaration */
    [TSVC_IDX_TIMESTAMP_RESP_CHAR] = {(uint8_t*)&att_decl_char, ATT_UUID_16_LEN, PERM(RD, ENABLE), 0, 0, NULL},

    /* Timestamp Response Characteristic Value (Notify, 20 bytes) */
    [TSVC_IDX_TIMESTAMP_RESP_VAL] = {TSVC_RESP_UUID_128, ATT_UUID_128_LEN, PERM(NTF, ENABLE),
                                     DEF_TSVC_RESP_CHAR_LEN, 0, NULL},

    /* Timestamp Response Client Char Config (CCC) */
    [TSVC_IDX_TIMESTAMP_RESP_NTF_CFG] = {(uint8_t*)&att_desc_cfg, ATT_UUID_16_LEN, PERM(RD, ENABLE) | PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE),
                                         sizeof(uint16_t), 0, NULL},

    /* Timestamp Response User Description */
    [TSVC_IDX_TIMESTAMP_RESP_USER_DESC] = {(uint8_t*)&att_desc_user_desc, ATT_UUID_16_LEN, PERM(RD, ENABLE),
                                           sizeof("Timestamp Response") - 1, sizeof("Timestamp Response") - 1,
                                           (uint8_t *) "Timestamp Response"},

    /* Update Service Declaration */
    [USVC_IDX_SVC] = {(uint8_t*)&att_decl_svc, ATT_UUID_128_LEN, PERM(RD, ENABLE),
                      sizeof(usvc_svc), sizeof(usvc_svc), (uint8_t*)&usvc_svc},

    /* Update Characteristic Declaration */
    [USVC_IDX_UPDATE_CHAR] = {(uint8_t*)&att_decl_char, ATT_UUID_16_LEN, PERM(RD, ENABLE), 0, 0, NULL},

    /* Update Characteristic Value (Write, 4 bytes - e.g., clock update) */
    [USVC_IDX_UPDATE_VAL] = {USVC_UPDATE_UUID_128, ATT_UUID_128_LEN, PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE),
                             DEF_UPDATE_CHAR_LEN, 0, NULL},

    /* Update Characteristic User Description */
    [USVC_IDX_UPDATE_USER_DESC] = {(uint8_t*)&att_desc_user_desc, ATT_UUID_16_LEN, PERM(RD, ENABLE),
                                   sizeof("Update") - 1, sizeof("Update") - 1, (uint8_t *) "Update"},
};
#endif  /* Disabled */
