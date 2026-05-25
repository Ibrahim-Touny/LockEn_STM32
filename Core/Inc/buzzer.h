#pragma once
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"

#define BUZZER_PORT  GPIOC
#define BUZZER_PIN   GPIO_PIN_15

static inline void Buzzer_On(void)  { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   }
static inline void Buzzer_Off(void) { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); }

static inline void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On(); osDelay(ms); Buzzer_Off();
}

static inline void Buzzer_BeepOK(void)
{
    Buzzer_Beep(80); osDelay(80); Buzzer_Beep(80);
}

static inline void Buzzer_BeepGranted(void) { Buzzer_Beep(400); }

static inline void Buzzer_BeepDenied(void)
{
    for (int i = 0; i < 3; i++) { Buzzer_Beep(120); osDelay(100); }
}

static inline void Buzzer_Alarm(uint32_t total_ms)
{
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < total_ms) {
        Buzzer_On();  osDelay(100);
        Buzzer_Off(); osDelay(100);
    }
}
