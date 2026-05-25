#pragma once
#include "stm32f4xx_hal.h"

/* Peripheral handles — defined in periph_init.c, used everywhere via extern */
extern I2C_HandleTypeDef  hi2c1;
extern I2C_HandleTypeDef  hi2c2;
extern RTC_HandleTypeDef  hrtc;
extern SPI_HandleTypeDef  hspi1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart6;

/* All non-static so auth_task can re-call MX_I2C1_Init after bus reset */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_I2C1_Init(void);
void MX_I2C2_Init(void);
void MX_RTC_Init(void);
void MX_SPI1_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART6_UART_Init(void);
