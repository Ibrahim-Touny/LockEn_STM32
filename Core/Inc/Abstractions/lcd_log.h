/*
 * ============================================================================
 * lcd_log.h
 * ============================================================================
 * Formatted text output to LCD display (printf-style logging)
 * Provides LCD_Log() as an alternative to serial logging for debugging
 * ============================================================================
 */

#ifndef LCD_LOG_H
#define LCD_LOG_H

#include "i2c-lcd.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/*
 * Print formatted text to the LCD display
 * Similar to printf(), but output goes to the LCD instead of serial
 *
 * fmt: format string (supports standard printf specifiers)
 * ...: variable arguments according to format string
 *
 * Example: LCD_Log("Value: %d", 42);
 */
void LCD_Log(const char *fmt, ...);

#endif
