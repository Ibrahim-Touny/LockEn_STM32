/*
 * ============================================================================
 * buzzer.h
 * ============================================================================
 * Buzzer/piezo speaker control for audio feedback
 * Provides simple beep patterns for user notifications (OK, granted, denied, alarm)
 * ============================================================================
 */

#pragma once
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

/*
 * GPIO port and pin where buzzer is connected
 */
#define BUZZER_PORT  GPIOC
#define BUZZER_PIN   GPIO_PIN_15

/*
 * Turn on the buzzer (set pin HIGH)
 */
static inline void Buzzer_On(void)  { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   }

/*
 * Turn off the buzzer (set pin LOW)
 */
static inline void Buzzer_Off(void) { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); }

/*
 * Create a single beep of specified duration
 * ms: beep duration in milliseconds
 */
static inline void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On(); osDelay(ms); Buzzer_Off();
}

/*
 * Two short beeps pattern (OK/success confirmation)
 * Total duration: ~240ms (80ms beep, 80ms pause, 80ms beep)
 */
static inline void Buzzer_BeepOK(void)
{
    Buzzer_Beep(80); osDelay(80); Buzzer_Beep(80);
}

/*
 * Single long beep pattern (access granted)
 * Duration: 400ms
 */
static inline void Buzzer_BeepGranted(void) { Buzzer_Beep(400); }

/*
 * Three short beeps pattern (access denied/error)
 * Total duration: ~460ms (three 120ms beeps with 100ms pauses)
 */
static inline void Buzzer_BeepDenied(void)
{
    for (int i = 0; i < 3; i++) { Buzzer_Beep(120); osDelay(100); }
}

/*
 * Repeating alarm pattern (100ms on, 100ms off)
 * total_ms: total alarm duration in milliseconds
 */
static inline void Buzzer_Alarm(uint32_t total_ms)
{
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < total_ms) {
        Buzzer_On();  osDelay(100);
        Buzzer_Off(); osDelay(100);
    }
}
