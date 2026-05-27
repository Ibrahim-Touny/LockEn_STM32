#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "storage.h"
#include "cmsis_os.h"
#include <stdint.h>

/* Which factor was verified — used as queue payload and bitmask bits */
typedef enum {
    FACTOR_RFID = (1u << 0),
    FACTOR_FP   = (1u << 1),
    FACTOR_PWD  = (1u << 2),
    FACTOR_FACE = (1u << 3),
} AuthFactor_t;

/* Sensor tasks post AuthFactor_t values here on success */
extern QueueHandle_t      g_authEvt;

/* Set to 1 by AuthTask after setup completes; sensor tasks spin until set */
extern volatile uint8_t   g_creds_ready;

/* Incremented by AuthTask at end of every session (success OR timeout).
 * Sensor tasks track which session they last posted for to prevent
 * double-counting the same factor in one session.                        */
extern volatile uint32_t  g_session_id;

/* Credentials loaded from flash after setup — read-only for sensor tasks */
extern CredentialStore_t  g_creds;

extern const osThreadAttr_t authTask_attr;
void AuthTask(void *argument);
