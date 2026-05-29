/*
 * ============================================================================
 * pwd_task.h
 * ============================================================================
 * This header declares the password entry (PIN keypad) task.
 * The PwdTask reads 4-digit PINs from the keypad matrix, compares them against
 * the stored credential password, and posts the password authentication factor
 * on successful match. It also handles special keys:
 *   - 'C' to request fingerprint scan
 *   - 'D' to request face recognition scan
 * ============================================================================
 */

#pragma once
#include "cmsis_os.h"

/* FreeRTOS task attributes for PwdTask (priority, stack size, etc.) */
extern const osThreadAttr_t pwdTask_attr;

/* Entry point for the password entry task
 * Called by FreeRTOS kernel when task is created
 * Never returns — continuously waits for keypad input */
void PwdTask(void *argument);
