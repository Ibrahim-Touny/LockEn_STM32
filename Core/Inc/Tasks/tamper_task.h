/*
 * ============================================================================
 * tamper_task.h
 * ============================================================================
 * This header declares the physical tamper detection task.
 * The TamperTask monitors the MPU6050 IMU (accelerometer + gyroscope) to detect
 * sudden motion or impacts on the lock device. When motion exceeding thresholds
 * is detected, it posts events to g_tamperEvt. AuthTask drains this queue and
 * triggers an alarm (buzzer) when tamper events are received.
 * ============================================================================
 */

#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdint.h>

/* Queue that TamperTask posts motion detection events into
 * AuthTask periodically drains this queue and triggers an alarm if any events found */
extern QueueHandle_t    g_tamperEvt;

/* Flag set to 1 by AuthTask after MPU6050 initialization completes
 * TamperTask waits for this flag before taking its baseline reading and starting */
extern volatile uint8_t g_mpu_ok;

/* FreeRTOS task attributes for TamperTask (priority, stack size, etc.) */
extern const osThreadAttr_t tamperTask_attr;

/* Entry point for the tamper detection task
 * Called by FreeRTOS kernel when task is created
 * Never returns — continuously monitors MPU6050 for motion */
void TamperTask(void *argument);
