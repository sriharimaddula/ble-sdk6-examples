/**
 ****************************************************************************************
 *
 * @file user_custs_config.c
 *
 * @brief Custom1 Server (CUSTS1) profile database structure and initialization
 *        for Advertising(burst) example.
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "app_prf_types.h"
#include "app_customs.h"
#include "user_custs1_def.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/* NOTE: Custom profile registration disabled - using direct GATT messaging instead.
 * The CUSTS1 profile system is excluded (EXCLUDE_DLG_CUSTS1=1) so TASK_ID_CUSTS1
 * and BLE_CUSTOM1_SERVER are not defined. We handle GATT writes/notifications
 * directly in user_catch_rest_hndl() using GATTC_WRITE_REQ_IND and GATTC_SEND_EVT_CMD.
 */

#if 0  /* Disabled - using direct GATT messaging */
#if (BLE_CUSTOM1_SERVER)
extern const struct attm_desc_128 custs1_att_db[CUSTS1_IDX_NB];
#endif

/// Custom1 server function callback table
const struct cust_prf_func_callbacks cust_prf_funcs[] =
{
#if (BLE_CUSTOM1_SERVER)
    {   TASK_ID_CUSTS1,
        custs1_att_db,
        CUSTS1_IDX_NB,
        #if (BLE_APP_PRESENT)
        app_custs1_create_db, NULL,
        #else
        NULL, NULL,
        #endif
        NULL, NULL,
    },
#endif
#if (BLE_CUSTOM2_SERVER)
    {   TASK_ID_CUSTS2,
        NULL,
        0,
        #if (BLE_APP_PRESENT)
        app_custs2_create_db, NULL,
        #else
        NULL, NULL,
        #endif
        NULL, NULL,
    },
#endif
    {TASK_ID_INVALID, NULL, 0, NULL, NULL, NULL, NULL},   // DO NOT MOVE. Must always be last
};
#endif  /* Disabled */

/// Stub array to satisfy linker when custom profile is not used
const struct cust_prf_func_callbacks cust_prf_funcs[] =
{
    {TASK_ID_INVALID, NULL, 0, NULL, NULL, NULL, NULL},
};
