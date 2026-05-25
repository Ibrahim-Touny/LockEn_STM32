#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdint.h>

/* Queue of uint8_t events posted by TamperTask when motion is detected.
 * AuthTask drains this queue at the start of each idle cycle.          */
extern QueueHandle_t    g_tamperEvt;

/* Set to 1 by AuthTask after MPU is initialised; TamperTask waits for it
 * before taking its baseline and starting to poll.                      */
extern volatile uint8_t g_mpu_ok;

extern const osThreadAttr_t tamperTask_attr;
void TamperTask(void *argument);
