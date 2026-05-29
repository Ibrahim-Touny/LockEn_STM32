/**
 * @file    lcd_log.c
 * @brief   Formatted logging output to LCD row 1 for debugging.
 */

#include "Abstractions/lcd_log.h"

/**
 * @brief Print a formatted message to the LCD (row 1 only).
 *
 * Pads output to 16 characters and clears leftover characters.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
void LCD_Log(const char *fmt, ...)
{
    char buf[17];  // 16 chars + null
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Pad with spaces to clear leftover characters
    int len = strlen(buf);
    while (len < 16) buf[len++] = ' ';
    buf[16] = '\0';

    lcd_put_cur(1, 0);         // Always write to row 1
    lcd_send_string(buf);
}
