/*
 * ============================================================================
 * ESP8266.h
 * ============================================================================
 * WiFi module driver for ESP8266 microcontroller
 * Handles UART communication, WiFi modes, AP/Station configuration, and TCP/IP
 * ============================================================================
 */

#ifndef	_ESP8266_H
#define	_ESP8266_H

#include "ESP8266Config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "stm32f4xx_hal.h"
#include "dwt_stm32_delay.h"

/*
 * Custom boolean type for compatibility
 */
typedef enum{false = 0, true = 1}bool;

extern UART_HandleTypeDef 	_WIFI_USART;

/*
 * ============================================================================
 * Enumeration Types for WiFi Configuration
 * ============================================================================
 */

/*
 * WiFi operational mode (Station, SoftAP, or both)
 */
typedef	enum
{
	WifiMode_Error                        =     0,
	WifiMode_Station                      =     1,
	WifiMode_SoftAp                       =     2,
	WifiMode_StationAndSoftAp             =     3,
}WifiMode_t;

/*
 * WiFi encryption methods for access point
 */
typedef enum
{
  WifiEncryptionType_Open                 =     0,
  WifiEncryptionType_WPA_PSK              =     2,
  WifiEncryptionType_WPA2_PSK             =     3,
  WifiEncryptionType_WPA_WPA2_PSK         =     4,
}WifiEncryptionType_t;

/*
 * Connection status for WiFi network
 */
typedef enum
{
  WifiConnectionStatus_Error              =     0,
  WifiConnectionStatus_GotIp              =     2,
  WifiConnectionStatus_Connected          =     3,
  WifiConnectionStatus_Disconnected       =     4,
  WifiConnectionStatus_ConnectionFail     =     5,
}WifiConnectionStatus_t;

/*
 * Information about a TCP/IP connection (client or server)
 */
typedef struct
{
  WifiConnectionStatus_t      status;
  uint8_t                     LinkId;
  char                        Type[4];
  char                        RemoteIp[17];
  uint16_t                    RemotePort;
  uint16_t                    LocalPort;
  bool                        RunAsServer;
}WifiConnection_t;
/*
 * Main state structure for WiFi module
 * Contains buffers, configuration, and connection status
 */
typedef struct
{
	/* UART communication buffers and state */
	uint8_t                       usartBuff;
	uint8_t                       RxBuffer[_WIFI_RX_SIZE];
	uint8_t                       TxBuffer[_WIFI_TX_SIZE];
	uint16_t                      RxIndex;
	bool                          RxIsData;

	/* General WiFi parameters */
	WifiMode_t                    Mode;
	char                          MyIP[16];
	char                          MyGateWay[16];

	/* Station mode configuration */
	char						  SSID_Connected[20];
	bool                          StationDhcp;
	char                          StationIp[16];

	/* SoftAP mode configuration */
	bool                          SoftApDhcp;
	char                          SoftApConnectedDevicesIp[6][16];
	char                          SoftApConnectedDevicesMac[6][18];

	/* TCP/IP connection management */
	bool                          TcpIpMultiConnection;
	uint16_t                      TcpIpPingAnswer;
	WifiConnection_t              TcpIpConnections[5];
}Wifi_t;

/*
 * Global WiFi module state
 */
extern Wifi_t	Wifi;

/*
 * ============================================================================
 * Low-level UART Communication Functions
 * ============================================================================
 */

/*
 * Clear the receive buffer and reset receive state
 */
void Wifi_RxClear(void);

/*
 * Send a string to the ESP8266 module
 */
bool Wifi_SendString(char *data);

/*
 * Wait for the module to respond with expected string(s)
 * TimeOut_ms: maximum time to wait in milliseconds
 * result: pointer to store the index of matched parameter
 * CountOfParameter: number of expected responses
 * Returns: true if any response matched within timeout
 */
bool Wifi_WaitForString(uint32_t TimeOut_ms,uint8_t *result,uint8_t CountOfParameter,...);

/*
 * Callback function for UART receive interrupt
 */
void Wifi_RxCallBack(void);

/*
 * ============================================================================
 * Basic Module Control Functions
 * ============================================================================
 */

/*
 * Initialize the ESP8266 module (send AT commands, check response)
 */
bool Wifi_Init(void);

/*
 * Enable (power on) the WiFi module
 */
void Wifi_Enable(void);

/*
 * Disable (power off) the WiFi module
 */
void Wifi_Disable(void);

/*
 * Restart the module (software reset)
 */
bool Wifi_Restart(void);

/*
 * Put module into deep sleep mode
 * DelayMs: milliseconds to sleep
 */
bool Wifi_DeepSleep(uint16_t DelayMs);

/*
 * Reset the module to factory settings
 */
bool Wifi_FactoryReset(void);

/*
 * Update module firmware
 */
bool Wifi_Update(void);

/*
 * Set RF transmit power level
 * Power_0_to_82: power level (0-82)
 */
bool Wifi_SetRfPower(uint8_t Power_0_to_82);

/*
 * ============================================================================
 * WiFi Mode Functions (Station / SoftAP / Both)
 * ============================================================================
 */

/*
 * Set the WiFi operating mode
 * WifiMode_: desired mode (Station, SoftAP, or both)
 */
bool Wifi_SetMode(WifiMode_t	WifiMode_);

/*
 * Read the current WiFi operating mode
 */
bool Wifi_GetMode(void);

/*
 * Query the module for its current IP address
 */
bool Wifi_GetMyIp(void);

/*
 * ============================================================================
 * Station Mode Functions (Client Mode)
 * ============================================================================
 */

/*
 * Connect to a WiFi access point
 * SSID: network name
 * Pass: network password
 * MAC: (optional) specific AP MAC address
 */
bool Wifi_Station_ConnectToAp(char *SSID,char *Pass,char *MAC);

/*
 * Disconnect from the current access point
 */
bool Wifi_Station_Disconnect(void);

/*
 * Enable or disable DHCP in station mode
 * Enable: true to enable DHCP, false to disable
 */
bool Wifi_Station_DhcpEnable(bool Enable);

/*
 * Check if DHCP is currently enabled in station mode
 */
bool Wifi_Station_DhcpIsEnable(void);

/*
 * Set a static IP address in station mode
 * IP: static IP address
 * GateWay: gateway IP address
 * NetMask: network mask
 */
bool Wifi_Station_SetIp(char *IP,char *GateWay,char *NetMask);

/*
 * ============================================================================
 * SoftAP Mode Functions (Access Point)
 * ============================================================================
 */

/*
 * Create a software access point
 * SSID: network name to broadcast
 * password: network password
 * channel: WiFi channel to use
 * WifiEncryptionType: encryption type (Open, WPA, WPA2, or WPA/WPA2)
 * MaxConnections_1_to_4: maximum devices allowed to connect
 * HiddenSSID: true to hide the network name, false to broadcast
 */
bool Wifi_SoftAp_Create(char *SSID,char *password,uint8_t channel,
		WifiEncryptionType_t WifiEncryptionType,uint8_t MaxConnections_1_to_4,
		bool HiddenSSID);

/*
 * Get information about connected devices to the SoftAP
 */
bool Wifi_GetApConnection(void);

/*
 * Retrieve list of devices connected to the SoftAP
 */
bool Wifi_SoftAp_GetConnectedDevices(void);

/*
 * ============================================================================
 * TCP/IP Connection Functions
 * ============================================================================
 */

/*
 * Query connection status for all active connections
 */
bool Wifi_TcpIp_GetConnectionStatus(void);

/*
 * Send a PING packet to a remote host
 * PingTo: IP address to ping
 */
bool Wifi_TcpIp_Ping(char *PingTo);

/*
 * Enable or disable multiple simultaneous TCP/IP connections
 * EnableMultiConnections: true to allow multiple connections
 */
bool Wifi_TcpIp_SetMultiConnection(bool EnableMultiConnections);

/*
 * Check if multiple connections are enabled
 */
bool Wifi_TcpIp_GetMultiConnection(void);

/*
 * Start a TCP client connection
 * LinkId: connection ID (0-4)
 * RemoteIp: remote server IP address
 * RemotePort: remote server port
 * TimeOut_S: connection timeout in seconds
 */
bool Wifi_TcpIp_StartTcpConnection(uint8_t LinkId,char *RemoteIp,uint16_t RemotePort,
		uint16_t TimeOut_S);

/*
 * Start a UDP connection
 * LinkId: connection ID (0-4)
 * RemoteIp: remote IP address
 * RemotePort: remote port
 * LocalPort: local port to bind
 */
bool Wifi_TcpIp_StartUdpConnection(uint8_t LinkId,char *RemoteIp,uint16_t RemotePort,
		uint16_t LocalPort);

/*
 * Close a TCP/UDP connection
 * LinkId: connection ID to close
 */
bool Wifi_TcpIp_Close(uint8_t LinkId);

/*
 * Start a TCP server on the specified port
 * PortNumber: port to listen on
 */
bool Wifi_TcpIp_SetEnableTcpServer(uint16_t PortNumber);

/*
 * Stop the TCP server
 * PortNumber: port to stop listening on
 */
bool Wifi_TcpIp_SetDisableTcpServer(uint16_t PortNumber);

/*
 * Send data over a UDP connection
 * LinkId: connection ID
 * dataLen: number of bytes to send
 * data: pointer to data buffer
 */
bool Wifi_TcpIp_SendDataUdp(uint8_t LinkId,uint16_t dataLen,uint8_t *data);

/*
 * Send data over a TCP connection
 * LinkId: connection ID
 * dataLen: number of bytes to send
 * data: pointer to data buffer
 */
bool Wifi_TcpIp_SendDataTcp(uint8_t LinkId,uint16_t dataLen,uint8_t *data);

#endif
