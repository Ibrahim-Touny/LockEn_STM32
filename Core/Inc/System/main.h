/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/*
 * ============================================================================
 * main.h
 * ============================================================================
 * Main header file for the LockEn STM32 project
 * Defines GPIO pin mappings for all peripherals and core initialization
 * ============================================================================
 */

/* Define to prevent recursive inclusion */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32F4 HAL library includes
 */
#include "stm32f4xx_hal.h"

/*
 * ============================================================================
 * GPIO Pin Definitions for 4x4 Keypad Matrix
 * ============================================================================
 * Rows: R1-R4 on GPIOB pins 15, 14, 13, 12
 * Columns: C1-C4 on GPIOB pins 5, 4 and GPIOA pins 1, 0
 */

/* Keypad Column Pins */
#define C4_Pin GPIO_PIN_0
#define C4_GPIO_Port GPIOA

#define C3_Pin GPIO_PIN_1
#define C3_GPIO_Port GPIOA

#define C2_Pin GPIO_PIN_4
#define C2_GPIO_Port GPIOB

#define C1_Pin GPIO_PIN_5
#define C1_GPIO_Port GPIOB

/* Keypad Row Pins */
#define R4_Pin GPIO_PIN_12
#define R4_GPIO_Port GPIOB

#define R3_Pin GPIO_PIN_13
#define R3_GPIO_Port GPIOB

#define R2_Pin GPIO_PIN_14
#define R2_GPIO_Port GPIOB

#define R1_Pin GPIO_PIN_15
#define R1_GPIO_Port GPIOB

/*
 * Error handler function for runtime errors
 */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
