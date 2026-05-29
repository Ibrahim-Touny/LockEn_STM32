/*
 * ============================================================================
 * esp_task.h
 * ============================================================================
 * This header declares the WiFi (ESP8266) communication task.
 * The EspTask establishes a WiFi SoftAP and TCP connection to a laptop server,
 * then continuously:
 *   - Mirrors LCD display state for remote viewing
 *   - Forwards all authentication logs for remote storage/monitoring
 *   - Handles face recognition requests from PwdTask
 *   - Processes enrollment commands and results from the laptop
 * ============================================================================
 */

#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdint.h>

/* Structure for a single log entry sent to the remote server
 * Contains timestamp, event type (GRANTED/TIMEOUT/TAMPER), and factor combination */
typedef struct {
    uint32_t ts;              /* Timestamp of the event */
    char     event[12];       /* Event type: "GRANTED", "TIMEOUT", "TAMPER", etc. */
    char     factors[12];     /* Factor combination: "RFID+FP", "RFID+PWD", etc. */
} EspLogEntry_t;

/* Queue that all tasks post log entries into
 * EspTask drains this queue and forwards entries to the laptop server */
extern QueueHandle_t        g_esp_log_queue;

/* LCD display mirror (row 0) — written by display.c, read by EspTask
 * EspTask sends this over TCP whenever it changes (g_lcd_dirty = 1) */
extern volatile char    g_lcd_line0[17];

/* LCD display mirror (row 1) — written by display.c, read by EspTask
 * EspTask sends this over TCP whenever it changes (g_lcd_dirty = 1) */
extern volatile char    g_lcd_line1[17];

/* Flag set to 1 by display.c when LCD content changes
 * EspTask checks this flag and clears it after forwarding the new display state */
extern volatile uint8_t g_lcd_dirty;

/* Flag set by AuthTask when starting face enrollment/reset process
 * EspTask sends enroll_reset command to server when this is set */
extern volatile uint8_t g_face_enroll_requested;

/* Flag set by EspTask when server confirms face enrollment is complete
 * AuthTask checks this flag to know when the face learning is done */
extern volatile uint8_t g_face_enrolled;

/* FreeRTOS task attributes for EspTask (priority, stack size, etc.) */
extern const osThreadAttr_t espTask_attr;

/* Entry point for the WiFi communication task
 * Called by FreeRTOS kernel when task is created
 * Never returns — continuously handles WiFi and server communication */
void EspTask(void *argument);

/* Called by PwdTask when user presses 'D' key during password entry
 * Signals EspTask to start capturing face images and send them to the server
 * Opens a 15-second window for the face scan */
void EspTask_RequestFaceScan(void);

/* Returns 1 if the TCP connection to the laptop server is currently active
 * Returns 0 if disconnected or not yet connected */
uint8_t EspTask_IsConnected(void);
