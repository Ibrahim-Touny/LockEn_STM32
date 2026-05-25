#include "display.h"
#include "i2c-lcd.h"
#include "cmsis_os.h"
#include <string.h>

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
}

void Display_Line(uint8_t row, const char *text)
{
    write_row(row, text);
}

void Display_Both(const char *top, const char *bot)
{
    write_row(0, top);
    write_row(1, bot);
}

void Display_Timed(const char *top, const char *bot, uint32_t ms)
{
    write_row(0, top);
    write_row(1, bot);
    if (ms > 0) osDelay(ms);
}

void Display_Idle(void)
{
    lcd_clear();
    write_row(0, "Scan your card");
    write_row(1, "");
}

void Display_Error(const char *msg)
{
    write_row(0, "** ERROR **");
    write_row(1, msg);
}
