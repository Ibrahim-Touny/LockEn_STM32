/**
 * @file    stm32f4xx_hal_timebase_tim.c
 * @brief   HAL time base generation using TIM5 for FreeRTOS tick.
 *          Configures 1 ms tick interrupt for task scheduling.
 */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_tim.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef        htim5;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Configure TIM5 for 1ms tick generation (FreeRTOS scheduler).
 *
 * @param TickPriority Interrupt priority for the tick
 * @return HAL_OK on success, HAL_ERROR on failure
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock, uwAPB1Prescaler = 0U;

  uint32_t              uwPrescalerValue = 0U;
  uint32_t              pFLatency;

  HAL_StatusTypeDef     status;

  /* Enable TIM5 clock */
  __HAL_RCC_TIM5_CLK_ENABLE();

  /* Get clock configuration */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

  /* Get APB1 prescaler */
  uwAPB1Prescaler = clkconfig.APB1CLKDivider;
  /* Compute TIM5 clock */
  if (uwAPB1Prescaler == RCC_HCLK_DIV1)
  {
    uwTimclock = HAL_RCC_GetPCLK1Freq();
  }
  else
  {
    uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
  }

  /* Compute the prescaler value to have TIM5 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

  /* Initialize TIM5 */
  htim5.Instance = TIM5;

  /* Initialize TIMx peripheral as follow:
   * Period = [(TIM5CLK/1000) - 1]. to have a (1/1000) s time base.
   * Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
   * ClockDivision = 0
   * Counter direction = Up
   */
  htim5.Init.Period = (1000000U / 1000U) - 1U;
  htim5.Init.Prescaler = uwPrescalerValue;
  htim5.Init.ClockDivision = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  status = HAL_TIM_Base_Init(&htim5);
  if (status == HAL_OK)
  {

    /* Start the TIM time Base generation in interrupt mode */
    status = HAL_TIM_Base_Start_IT(&htim5);
    if (status == HAL_OK)
    {
    /* Enable the TIM5 global Interrupt */
        HAL_NVIC_EnableIRQ(TIM5_IRQn);
      /* Configure the SysTick IRQ priority */
      if (TickPriority < (1UL << __NVIC_PRIO_BITS))
      {
        /* Configure the TIM IRQ priority */
        HAL_NVIC_SetPriority(TIM5_IRQn, TickPriority, 0U);
        uwTickPrio = TickPriority;
      }
      else
      {
        status = HAL_ERROR;
      }
    }
  }

 /* Return function status */
  return status;
}

/**
 * @brief Suspend tick generation (disable TIM5 update interrupt).
 */
void HAL_SuspendTick(void)
{
  /* Disable TIM5 update Interrupt */
  __HAL_TIM_DISABLE_IT(&htim5, TIM_IT_UPDATE);
}

/**
 * @brief Resume tick generation (enable TIM5 update interrupt).
 */
void HAL_ResumeTick(void)
{
  /* Enable TIM5 Update interrupt */
  __HAL_TIM_ENABLE_IT(&htim5, TIM_IT_UPDATE);
}

