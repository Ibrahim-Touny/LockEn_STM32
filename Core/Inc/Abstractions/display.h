/*
 * ============================================================================
 * display.h
 * ============================================================================
 * High-level interface for displaying text on the 16x2 LCD screen
 * Provides thread-safe display functions with mutex protection
 * ============================================================================
 */

#pragma once
#include <stdint.h>
#include "cmsis_os.h"
#include "Tasks/esp_task.h"

/*
 * Mutex protecting LCD access from multiple tasks
 * Created by Display_Init() and used by all Display functions
 */
extern osMutexId_t g_lcd_mutex;

/*
 * Initialize the display subsystem
 * Sets up the LCD hardware and creates the mutex for thread safety
 * Must be called once before other Display functions are used
 */
void Display_Init(void);

/*
 * Display text on a single LCD row
 * row: 0 for top line, 1 for bottom line
 * text: text to display (padded/truncated to 16 characters)
 */
void Display_Line(uint8_t row, const char *text);

/*
 * Display text on both LCD rows
 * top: text for top row
 * bot: text for bottom row
 */
void Display_Both(const char *top, const char *bot);

/*
 * Display text on both rows, then wait before continuing
 * Releases the LCD mutex before the delay, allowing other tasks to use it
 * top: text for top row
 * bot: text for bottom row
 * ms: delay duration in milliseconds
 */
void Display_Timed(const char *top, const char *bot, uint32_t ms);

/*
 * Display the idle/ready state
 * Shows the default "ready to scan" message
 */
void Display_Idle(void);

/*
 * Display an error message
 * msg: error description
 */
void Display_Error(const char *msg);
