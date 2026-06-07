/*
 * ============================================================================
 * ESP8266Config.h
 * ============================================================================
 * Configuration file for ESP8266 WiFi module
 * Defines UART settings, buffer sizes, GPIO pins, and timing parameters
 * ============================================================================
 */

#ifndef	_WIFICONFIG_H
#define	_WIFICONFIG_H

/*
 * ============================================================================
 * UART and Buffer Configuration
 * ============================================================================
 */

/*
 * UART interface used for ESP8266 communication
 */
#define 	_WIFI_USART  					huart2

/*
 * Size of UART receive buffer (bytes)
 */
#define		_WIFI_RX_SIZE					2048

/*
 * Size of buffer for receiving data payload (bytes)
 */
#define		_WIFI_RX_FOR_DATA_SIZE			1024

/*
 * Size of UART transmit buffer (bytes)
 */
#define		_WIFI_TX_SIZE					256

/*
 * Size of WiFi task stack (bytes)
 */
#define		_WIFI_TASK_SIZE					512

/*
 * Maximum number of bytes to send in one transmission
 */
#define 	_MAX_SEND_BYTES					2048

/*
 * ============================================================================
 * GPIO Pin Configuration for ESP8266 Control
 * ============================================================================
 */

/*
 * GPIO port for ESP8266 control buttons
 */
#define		_BANK_WIFI_BUTTONS				GPIOB

/*
 * Reset button pin (PB11) - pull low to reset the module
 */
#define		_BUTTON_RST						GPIO_PIN_11

/*
 * Enable button pin (PB6) - controls module power
 */
#define		_BUTTON_ENABLE					GPIO_PIN_6

/*
 * ============================================================================
 * Timing Configuration (All times in milliseconds)
 * ============================================================================
 */

/*
 * Short timeout for quick responses (1 second)
 */
#define		_WIFI_WAIT_TIME_LOW				1000

/*
 * Medium timeout for standard operations (5 seconds)
 */
#define		_WIFI_WAIT_TIME_MED				5000

/*
 * Long timeout for network operations (15 seconds)
 */
#define		_WIFI_WAIT_TIME_HIGH			15000

/*
 * Extended timeout for slow operations (25 seconds)
 */
#define		_WIFI_WAIT_TIME_VERYHIGH		25000

#endif