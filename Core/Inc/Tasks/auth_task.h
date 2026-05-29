/*
 * ============================================================================
 * auth_task.h
 * ============================================================================
 * This header declares the central authentication task and shared authentication
 * globals. The AuthTask is responsible for:
 *   - Initializing all hardware (RFID, fingerprint, display, etc.)
 *   - Managing credential storage (setup wizard on first boot, load from flash)
 *   - Collecting authentication factors from sensor tasks via g_authEvt queue
 *   - Triggering solenoid unlock when two distinct factors are received
 *   - Logging all events to the WiFi task for remote transmission
 *   - Detecting physical tampering and triggering alarm
 *
 * Authentication factors are represented as bitmask values to enable checking
 * if two distinct factors have been verified in the current session.
 * ============================================================================
 */

#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "Abstractions/storage.h"
#include "cmsis_os.h"
#include <stdint.h>

/*
 * Authentication factor enum
 * Each bit represents one authentication method (RFID, fingerprint, password, or face)
 * Used as queue payload and in bitmask operations to check for two distinct factors
 */
typedef enum {
    FACTOR_RFID = (1u << 0),  /* RFID card scan successful */
    FACTOR_FP   = (1u << 1),  /* Fingerprint match successful */
    FACTOR_PWD  = (1u << 2),  /* Password entry correct */
    FACTOR_FACE = (1u << 3),  /* Face recognition successful */
} AuthFactor_t;

/* Queue that sensor tasks post into when authentication succeeds
 * Contains AuthFactor_t values — one per successful factor detection */
extern QueueHandle_t      g_authEvt;

/* Flag set to 1 by AuthTask after setup wizard and initial load complete
 * Sensor tasks wait (spin) on this flag before starting their main loops */
extern volatile uint8_t   g_creds_ready;

/* Session ID counter — incremented after each unlock attempt (success or timeout)
 * Sensor tasks track the last session ID they posted for to prevent the same
 * factor from being counted twice in one authentication session */
extern volatile uint32_t  g_session_id;

/* Credentials structure loaded from flash at startup
 * Contains RFID UID, password, and placeholder for face/fingerprint data
 * Read-only from sensor task perspective */
extern CredentialStore_t  g_creds;

/* FreeRTOS task attributes for AuthTask (priority, stack size, etc.) */
extern const osThreadAttr_t authTask_attr;

/* Entry point for the authentication task
 * Called by FreeRTOS kernel when task is created
 * Never returns — runs forever in loop */
void AuthTask(void *argument);
