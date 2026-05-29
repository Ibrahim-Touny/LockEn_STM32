/**
 * @file    dwt_stm32_delay.c
 * @brief   High-precision microsecond delay using ARM Cortex-M DWT counter.
 *          Provides accurate timing for ESP8266 AT command delays.
 */

#include "Drivers/dwt_stm32_delay.h"

/**
 * @brief Initialize the Data Watchpoint and Trace (DWT) cycle counter.
 *
 * Must be called once at startup before using DWT_Delay_us().
 * Enables the DWT counter for cycle-accurate timing.
 *
 * @return 0 if counter initialized successfully, 1 if initialization failed
 */
uint32_t DWT_Delay_Init(void) {
  /* Disable TRC */
  CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk; // ~0x01000000;
  /* Enable TRC */
  CoreDebug->DEMCR |=  CoreDebug_DEMCR_TRCENA_Msk; // 0x01000000;

  /* Disable clock cycle counter */
  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk; //~0x00000001;
  /* Enable  clock cycle counter */
  DWT->CTRL |=  DWT_CTRL_CYCCNTENA_Msk; //0x00000001;

  /* Reset the clock cycle counter value */
  DWT->CYCCNT = 0;

     /* 3 NO OPERATION instructions */
     __ASM volatile ("NOP");
     __ASM volatile ("NOP");
  __ASM volatile ("NOP");

  /* Check if clock cycle counter has started */
     if(DWT->CYCCNT)
     {
       return 0; /*clock cycle counter started*/
     }
     else
  {
    return 1; /*clock cycle counter not started*/
  }
}


/**
 * @brief Block for a specified number of microseconds using DWT counter.
 *
 * Provides cycle-accurate delays using the ARM DWT counter.
 * Must call DWT_Delay_Init() first.
 *
 * @param microseconds Number of microseconds to delay
 */
void DWT_Delay_us(volatile uint32_t microseconds)
{
  uint32_t clk_cycle_start = DWT->CYCCNT;

  /* Go to number of cycles for system */
  microseconds *= (HAL_RCC_GetHCLKFreq() / 1000000);

  /* Delay till end */
  while ((DWT->CYCCNT - clk_cycle_start) < microseconds);
}

/* Use DWT_Delay_Init (); and DWT_Delay_us (microseconds) in the main */