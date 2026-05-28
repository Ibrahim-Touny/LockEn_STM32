#include "sleep_manager.h"
#include "display.h"
#include "i2c-lcd.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

volatile uint8_t  g_system_sleeping    = 0;
volatile uint32_t g_last_activity_tick = 0;

void Sleep_UpdateActivity(void)
{
    g_last_activity_tick = HAL_GetTick();
}

uint8_t Sleep_IsTimeoutExpired(void)
{
    return (HAL_GetTick() - g_last_activity_tick) >= SLEEP_TIMEOUT_MS;
}

void Sleep_Enter(void)
{
    if (g_system_sleeping) return;
    g_system_sleeping = 1;

    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    lcd_clear();
    lcd_backlight_off();
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

void Sleep_Exit(void)
{
    if (!g_system_sleeping) return;
    g_system_sleeping = 0;
    Sleep_UpdateActivity();

    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    lcd_backlight_on();
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);

    Display_Both("Scan 2 factors", "");
}
