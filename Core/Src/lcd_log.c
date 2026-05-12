#include "lcd_log.h"

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
