/**
 * @file    display.c
 * @brief   High-level LCD display interface for user-facing messages.
 *          Provides thread-safe display updates with mutex protection.
 */

#include "Abstractions/display.h"
#include "Drivers/i2c-lcd.h"
#include "cmsis_os.h"
#include <string.h>

osMutexId_t g_lcd_mutex = NULL;

/**
 * @brief Initialize the display system and mutex.
 *
 * Creates the LCD mutex and initializes the LCD hardware.
 */
void Display_Init(void)
{
    g_lcd_mutex = osMutexNew(NULL);
    lcd_init();
    lcd_clear();
}

/**
 * @brief Write a single row to the LCD with padding and mirroring.
 *
 * Pads text with spaces to 16 characters, sends to LCD, and mirrors to
 * EspTask global variables for remote display synchronization.
 *
 * @param row Row number (0 or 1)
 * @param text Text to display (will be padded/truncated to 16 chars)
 */
static void write_row(uint8_t row, const char *text)
{
    char buf[17];
    memset(buf, ' ', 16);
    buf[16] = '\0';
    uint8_t len = (uint8_t)strlen(text);
    if (len > 16) len = 16;
    memcpy(buf, text, len);
    lcd_put_cur(row, 0);
    lcd_send_string(buf);

    /* Mirror to EspTask so it can push LCD state to the laptop */
    if (row == 0) memcpy((char *)g_lcd_line0, buf, 17);
    else           memcpy((char *)g_lcd_line1, buf, 17);
    g_lcd_dirty = 1;
}

/**
 * @brief Display text on a single LCD row with mutex protection.
 *
 * @param row Row number (0 or 1)
 * @param text Text to display
 */
void Display_Line(uint8_t row, const char *text)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(row, text);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

/**
 * @brief Display text on both LCD rows simultaneously.
 *
 * @param top Text for row 0
 * @param bot Text for row 1
 */
void Display_Both(const char *top, const char *bot)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, top);
    write_row(1, bot);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

/**
 * @brief Display text for a specified duration, then clear.
 *
 * @param top Text for row 0
 * @param bot Text for row 1
 * @param ms Duration in milliseconds (0 = no delay)
 */
void Display_Timed(const char *top, const char *bot, uint32_t ms)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, top);
    write_row(1, bot);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
    if (ms > 0) osDelay(ms);
}

/**
 * @brief Display the idle prompt ("Scan 2 factors").
 */
void Display_Idle(void)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    lcd_clear();
    write_row(0, "Scan 2 factors");
    write_row(1, "");
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

/**
 * @brief Display an error message on the LCD.
 *
 * @param msg Error message to display on row 1
 */
void Display_Error(const char *msg)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, "** ERROR **");
    write_row(1, msg);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}
