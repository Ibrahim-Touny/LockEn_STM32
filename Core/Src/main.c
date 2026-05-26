/* USER CODE BEGIN Header */
/**
  **************************
  * @file           : main.c
  * @brief          : Hardware init + FreeRTOS task creation only.
  *                   All application logic lives in the task source files.
  **************************
  */
/* USER CODE END Header */

#include "main.h"
#include "periph_init.h"
#include "cmsis_os.h"
#include "auth_task.h"
#include "tamper_task.h"
#include "rfid_task.h"
#include "fp_task.h"
#include "pwd_task.h"
#include "esp_task.h"
#include "dwt_stm32_delay.h"

/* ════════════════════════════════════════════════════════════════════════════
 *  MAIN
 * ════════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    DWT_Delay_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_USART6_UART_Init();
    MX_USART2_UART_Init();
    MX_RTC_Init();

    osKernelInitialize();

    osThreadNew(AuthTask,   NULL, &authTask_attr);
    osThreadNew(TamperTask, NULL, &tamperTask_attr);
    osThreadNew(RfidTask,   NULL, &rfidTask_attr);
    osThreadNew(FpTask,     NULL, &fpTask_attr);
    osThreadNew(PwdTask,    NULL, &pwdTask_attr);
    osThreadNew(EspTask,    NULL, &espTask_attr);

    osKernelStart();

    /* osKernelStart() never returns */
    while (1) {}
}

/* ── HAL callbacks ──────────────────────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
