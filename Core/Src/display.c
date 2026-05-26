#include "display.h"
#include "i2c-lcd.h"
#include "cmsis_os.h"
#include <string.h>

osMutexId_t g_lcd_mutex = NULL;

void Display_Init(void)
{
    g_lcd_mutex = osMutexNew(NULL);
    lcd_init();
    lcd_clear();
}

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

void Display_Line(uint8_t row, const char *text)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(row, text);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

void Display_Both(const char *top, const char *bot)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, top);
    write_row(1, bot);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

void Display_Timed(const char *top, const char *bot, uint32_t ms)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, top);
    write_row(1, bot);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
    if (ms > 0) osDelay(ms);
}

void Display_Idle(void)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    lcd_clear();
    write_row(0, "Scan 2 factors");
    write_row(1, "");
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

void Display_Error(const char *msg)
{
    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    write_row(0, "** ERROR **");
    write_row(1, msg);
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}
