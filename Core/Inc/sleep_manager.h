#pragma once
#include <stdint.h>

#define SLEEP_TIMEOUT_MS  30000U

extern volatile uint8_t  g_system_sleeping;
extern volatile uint32_t g_last_activity_tick;

/* Call on any keypress or card scan to reset the 30-second idle timer. */
void    Sleep_UpdateActivity(void);

/* Returns 1 if no activity for SLEEP_TIMEOUT_MS. Only call when bitmask == 0. */
uint8_t Sleep_IsTimeoutExpired(void);

/* Turn off LCD backlight and set g_system_sleeping = 1.
 * Safe to call multiple times — ignored if already sleeping. */
void    Sleep_Enter(void);

/* Turn on LCD backlight, restore "Scan 2 factors" display, reset idle timer.
 * Safe to call multiple times — ignored if already awake. */
void    Sleep_Exit(void);
