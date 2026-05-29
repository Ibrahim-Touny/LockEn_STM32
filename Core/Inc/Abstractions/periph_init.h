/*
 * ============================================================================
 * periph_init.h
 * ============================================================================
 * Peripheral hardware initialization and global handles
 * Declares all MCU peripherals (UART, I2C, SPI, RTC) for system-wide use
 * ============================================================================
 */

#pragma once
#include "stm32f4xx_hal.h"

/*
 * ============================================================================
 * Global Peripheral Handles
 * ============================================================================
 * Defined in periph_init.c, declared here for use throughout the project
 */

/*
 * I2C1: LCD display (via I2C backpack)
 */
extern I2C_HandleTypeDef  hi2c1;

/*
 * I2C2: RFID card reader module
 */
extern I2C_HandleTypeDef  hi2c2;

/*
 * RTC: Real-time clock for timestamps
 */
extern RTC_HandleTypeDef  hrtc;

/*
 * SPI1: Serial Peripheral Interface (reserved for expansion)
 */
extern SPI_HandleTypeDef  hspi1;

/*
 * UART1: Debug serial output
 */
extern UART_HandleTypeDef huart1;

/*
 * UART2: WiFi module (ESP8266) communication
 */
extern UART_HandleTypeDef huart2;

/*
 * UART6: (Reserved for future use)
 */
extern UART_HandleTypeDef huart6;

/*
 * ============================================================================
 * Initialization Functions
 * ============================================================================
 * All functions are non-static to allow re-initialization (e.g., I2C bus reset)
 */

/*
 * Configure the system clock and PLL settings
 */
void SystemClock_Config(void);

/*
 * Initialize all GPIO pins for the system
 */
void MX_GPIO_Init(void);

/*
 * Initialize I2C1 for LCD display communication
 * Non-static to allow re-initialization after bus errors
 */
void MX_I2C1_Init(void);

/*
 * Initialize I2C2 for RFID module communication
 */
void MX_I2C2_Init(void);

/*
 * Initialize the real-time clock
 */
void MX_RTC_Init(void);

/*
 * Initialize SPI1 peripheral
 */
void MX_SPI1_Init(void);

/*
 * Initialize UART1 for debug output
 */
void MX_USART1_UART_Init(void);

/*
 * Initialize UART2 for WiFi module (ESP8266)
 */
void MX_USART2_UART_Init(void);

/*
 * Initialize UART6 peripheral
 */
void MX_USART6_UART_Init(void);
