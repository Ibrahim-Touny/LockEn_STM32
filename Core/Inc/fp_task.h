#pragma once
#include "cmsis_os.h"

/* Set to 1 by PwdTask when user presses C; FpTask consumes (clears) it */
extern volatile uint8_t g_fp_scan_requested;

extern const osThreadAttr_t fpTask_attr;
void FpTask(void *argument);
