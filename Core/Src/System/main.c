/**
 * @file    main.c
 * @brief   STM32F4 system startup and FreeRTOS task initialization.
 *          Sets up hardware peripherals and creates all application tasks.
 *          Application logic resides in individual task files.
 */

#include "System/main.h"
#include "Abstractions/periph_init.h"
#include "cmsis_os.h"
#include "Tasks/auth_task.h"
#include "Tasks/tamper_task.h"
#include "Tasks/rfid_task.h"
#include "Tasks/fp_task.h"
#include "Tasks/pwd_task.h"
#include "Tasks/esp_task.h"
#include "Drivers/dwt_stm32_delay.h"

/**
 * @brief Main entry point - initializes hardware and starts FreeRTOS scheduler.
 *
 * Configures the system clock, initializes all peripheral interfaces (UART,
 * SPI, I2C), and creates the FreeRTOS task instances. Starts the scheduler,
 * which never returns.
 *
 * @return Never returns (FreeRTOS scheduler loops forever)
 */
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

/**
 * @brief Timer callback for FreeRTOS tick generation (TIM5 interrupt).
 *
 * Increments the HAL tick counter used by FreeRTOS for task scheduling.
 *
 * @param htim Pointer to timer handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) HAL_IncTick();
}

/**
 * @brief Error handler - called on fatal errors (e.g., assert failures).
 *
 * Disables interrupts and enters an infinite loop.
 */
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
