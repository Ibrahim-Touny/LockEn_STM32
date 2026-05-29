/**
 * @file    sleep_manager.c
 * @brief   Idle timeout and sleep management - powers down LCD after inactivity
 *          and re-enables on user activity.
 */

#include "Abstractions/sleep_manager.h"
#include "Abstractions/display.h"
#include "Drivers/i2c-lcd.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

volatile uint8_t  g_system_sleeping    = 0;
volatile uint32_t g_last_activity_tick = 0;

/**
 * @brief Update the activity timestamp to reset idle timeout.
 *
 * Called whenever the user interacts with the device (button press, card scan, etc.).
 */
void Sleep_UpdateActivity(void)
{
    g_last_activity_tick = HAL_GetTick();
}

/**
 * @brief Check if the idle timeout has been exceeded.
 *
 * @return 1 if timeout expired, 0 if still within the timeout window
 */
uint8_t Sleep_IsTimeoutExpired(void)
{
    return (HAL_GetTick() - g_last_activity_tick) >= SLEEP_TIMEOUT_MS;
}

/**
 * @brief Enter sleep mode by turning off the LCD backlight.
 *
 * Clears display and disables backlight to save power.
 */
void Sleep_Enter(void)
{
    if (g_system_sleeping) return;
    g_system_sleeping = 1;

    if (g_lcd_mutex) osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
    lcd_clear();
    lcd_backlight_off();
    if (g_lcd_mutex) osMutexRelease(g_lcd_mutex);
}

/**
 * @brief Exit sleep mode by turning on the LCD backlight.
 *
 * Re-enables backlight and displays idle prompt on wakeup.
 */
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
