/*
 * ============================================================================
 * sleep_manager.h
 * ============================================================================
 * Power management for sleep/idle mode when no user activity is detected
 * Automatically enters sleep after 30 seconds of inactivity to save power
 * ============================================================================
 */

#pragma once
#include <stdint.h>

/*
 * Timeout duration before entering sleep mode (milliseconds)
 * 30 seconds = 30000 ms
 */
#define SLEEP_TIMEOUT_MS  30000U

/*
 * Global flag indicating if system is currently in sleep mode
 * 0 = awake, 1 = sleeping
 */
extern volatile uint8_t  g_system_sleeping;

/*
 * Timestamp of the last user activity (keypress or card scan)
 * Updated whenever activity occurs
 */
extern volatile uint32_t g_last_activity_tick;

/*
 * Record that user activity occurred (keypress, card scan, etc.)
 * Resets the idle timeout counter to prevent sleep
 * Safe to call from interrupt handlers
 */
void    Sleep_UpdateActivity(void);

/*
 * Check if the idle timeout has expired
 * Returns 1 if no activity for SLEEP_TIMEOUT_MS, 0 otherwise
 * Should only be called when the system is awake (g_system_sleeping == 0)
 */
uint8_t Sleep_IsTimeoutExpired(void);

/*
 * Enter sleep mode: turn off LCD backlight and disable scanning
 * Sets g_system_sleeping = 1
 * Safe to call multiple times; ignored if already sleeping
 */
void    Sleep_Enter(void);

/*
 * Exit sleep mode: restore LCD backlight and resume normal operation
 * Restores the "Scan 2 factors" display and resets the idle timer
 * Safe to call multiple times; ignored if already awake
 */
void    Sleep_Exit(void);
