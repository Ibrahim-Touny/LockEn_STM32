/*
 * ============================================================================
 * rfid_task.h
 * ============================================================================
 * This header declares the RFID (RC522) scanning task.
 * The RfidTask continuously scans for RFID cards and compares them against
 * the stored credential UID. On match, it posts the RFID authentication factor
 * to the auth queue. On mismatch, it logs an error and beeps.
 * ============================================================================
 */

#pragma once
#include "cmsis_os.h"

/* FreeRTOS task attributes for RfidTask (priority, stack size, etc.) */
extern const osThreadAttr_t rfidTask_attr;

/* Entry point for the RFID scanning task
 * Called by FreeRTOS kernel when task is created
 * Never returns — continuously scans for cards */
void RfidTask(void *argument);
