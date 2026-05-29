/*
 * ============================================================================
 * dwt_stm32_delay.h
 * ============================================================================
 * High-precision microsecond delay using STM32 Data Watchpoint and Trace (DWT)
 * Provides accurate sub-millisecond timing for time-critical operations
 * ============================================================================
 */

#ifndef DWT_STM32_DELAY_H
#define DWT_STM32_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*
 * Initialize the DWT counter for microsecond delay operations
 *
 * Returns:
 *   0: DWT counter initialized successfully
 *   1: DWT counter unavailable or initialization failed
 *
 * Must be called once at startup before using DWT_Delay_us()
 */
uint32_t DWT_Delay_Init(void);

/*
 * Create a delay with microsecond precision
 * Uses the CPU cycle counter for accurate timing
 *
 * microseconds: delay duration in microseconds
 */
void DWT_Delay_us(volatile uint32_t microseconds);

#ifdef __cplusplus
}
#endif

#endif