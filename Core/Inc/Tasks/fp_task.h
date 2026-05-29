/*
 * ============================================================================
 * fp_task.h
 * ============================================================================
 * This header declares the fingerprint sensor (R307) scanning task.
 * The FpTask waits for the g_fp_scan_requested flag (set by PwdTask when user
 * presses 'C'), then performs fingerprint capture and matching against the
 * stored template. On successful match, it posts the fingerprint authentication
 * factor to the auth queue. On failure or timeout, it shows error messages.
 * ============================================================================
 */

#pragma once
#include "cmsis_os.h"

/* Flag set to 1 by PwdTask when user presses C during password entry
 * FpTask reads this flag and triggers a fingerprint scan, then clears it */
extern volatile uint8_t g_fp_scan_requested;

/* FreeRTOS task attributes for FpTask (priority, stack size, etc.) */
extern const osThreadAttr_t fpTask_attr;

/* Entry point for the fingerprint scanning task
 * Called by FreeRTOS kernel when task is created
 * Never returns — waits for scan requests and performs fingerprint matching */
void FpTask(void *argument);
