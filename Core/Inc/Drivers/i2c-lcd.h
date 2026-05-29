/*
 * ============================================================================
 * i2c-lcd.h
 * ============================================================================
 * I2C-based LCD display driver for 16x2 character LCD modules
 * ============================================================================
 */

#include "System/main.h"

/*
 * Initialize the I2C LCD module and prepare it for use
 */
void lcd_init (void);

/*
 * Send a command byte to the LCD (e.g., cursor movement, display control)
 */
void lcd_send_cmd (char cmd);

/*
 * Send a data byte to the LCD (a character to display)
 */
void lcd_send_data (char data);

/*
 * Send a null-terminated string to the LCD
 */
void lcd_send_string (char *str);

/*
 * Move the LCD cursor to a specific position
 * row: 0 or 1 (top or bottom line)
 * col: 0-15 (column position)
 */
void lcd_put_cur(int row, int col);

/*
 * Clear the LCD display and return cursor to home position
 */
void lcd_clear (void);

/*
 * Turn on the LCD backlight
 */
void lcd_backlight_on  (void);

/*
 * Turn off the LCD backlight
 */
void lcd_backlight_off (void);
