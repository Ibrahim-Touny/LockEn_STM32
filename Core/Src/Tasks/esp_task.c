/**
 * @file    esp_task.c
 * @brief   ESP8266 WiFi and remote face recognition task - maintains SoftAP,
 *          TCP connection to laptop, handles face enrollment/verification,
 *          and syncs LCD display state and access logs to remote server.
 */

#include "Tasks/esp_task.h"
#include "Tasks/auth_task.h"
#include "Abstractions/display.h"
#include "Drivers/ESP8266.h"
#include "Abstractions/periph_init.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

#define LAPTOP_IP    "192.168.4.2"
#define LAPTOP_PORT  3000
#define AP_SSID      "LockEn_Safe"
#define AP_PASS      "locken123"
#define AP_CHANNEL   6
#define PWD_CMP_LEN  5   /* PASSWORD_LEN (4) + null terminator */

const osThreadAttr_t espTask_attr = {
    .name       = "EspTask",
    .stack_size = 1024,
    .priority   = (osPriority_t) osPriorityBelowNormal,
};

QueueHandle_t g_esp_log_queue = NULL;

volatile char    g_lcd_line0[17] = "                ";
volatile char    g_lcd_line1[17] = "                ";
volatile uint8_t g_lcd_dirty     = 0;

volatile uint8_t g_face_enroll_requested = 0;
volatile uint8_t g_face_enrolled         = 0;

static volatile uint8_t s_connected            = 0;
static volatile uint8_t s_face_scan_req        = 0;
static volatile uint8_t s_waiting_enroll_done  = 0;

/**
 * @brief Request a face scan from the remote server.
 */
void EspTask_RequestFaceScan(void) { s_face_scan_req = 1; }

/**
 * @brief Check if the device is currently connected to the remote server.
 *
 * @return 1 if connected, 0 otherwise
 */
uint8_t EspTask_IsConnected(void)  { return s_connected; }

/* Forward declarations */
static void esp_check_incoming(void);
static uint8_t esp_at_cmd(const char *cmd, const char *expect, uint32_t timeout_ms);

/**
 * @brief Send a JSON command string to the remote server via AT+CIPSEND.
 *
 * Processes any pending incoming data first, sends the JSON line,
 * waits for the prompt, and transmits the data.
 *
 * @param json Null-terminated JSON string to send
 * @return 1 on success, 0 on failure (connection lost)
 */
static uint8_t esp_send_json(const char *json)
{
    if (!s_connected) return 0;

    /* Process any data that arrived before we clear the buffer.
     * Without this, enroll_done / face_ok can be wiped by Wifi_RxClear(). */
    esp_check_incoming();

    uint16_t len = (uint16_t)strlen(json);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", len);

    Wifi_RxClear();
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);

    uint8_t got_prompt = 0;
    for (uint8_t attempt = 0; attempt < 3 && !got_prompt; attempt++) {
        for (uint16_t i = 0; i < 100; i++) {
            esp_check_incoming();
            if (strstr((char *)Wifi.RxBuffer, ">")) { got_prompt = 1; break; }
            osDelay(10);
        }
        if (!got_prompt) {
            HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
        }
    }
    if (!got_prompt) { s_connected = 0; return 0; }

    HAL_UART_Transmit(&huart2, (uint8_t *)json, len, 1000);

    for (uint16_t i = 0; i < 30; i++) {
        esp_check_incoming();
        osDelay(10);
    }
    return 1;
}

/**
 * @brief Attempt to establish a TCP client connection to the remote server.
 *
 * Sends AT+CIPSTART command and waits for CONNECT or OK response.
 *
 * @return 1 on success, 0 on failure or timeout
 */
static uint8_t esp_try_connect(void)
{
    char cmd[64];

    /* Close stale socket before opening a new one */
    esp_at_cmd("AT+CIPCLOSE\r\n", "OK", 1500);
    osDelay(200);

    snprintf(cmd, sizeof(cmd),
             "AT+CIPSTART=\"TCP\",\"%s\",%d,10\r\n", LAPTOP_IP, LAPTOP_PORT);

    Wifi_RxClear();
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);

    for (uint8_t i = 0; i < 80; i++) {
        char *rx = (char *)Wifi.RxBuffer;
        if (strstr(rx, "CONNECT") || strstr(rx, "\r\nOK\r\n")) return 1;
        if (strstr(rx, "ERROR")   || strstr(rx, "FAIL"))     return 0;
        osDelay(100);
    }
    return 0;
}

/**
 * @brief Parse and handle incoming data from the remote server.
 *
 * Checks for face_ok, enroll_done, and pwd (remote PIN) messages.
 * Updates state variables or posts auth factors to the auth queue.
 */
static void esp_erase_from(char *start)
{
    for (char *p = start; p < (char *)Wifi.RxBuffer + _WIFI_RX_SIZE && *p; p++)
        *p = ' ';
}

static void esp_check_incoming(void)
{
    char *rx = (char *)Wifi.RxBuffer;

    if (strstr(rx, "CLOSED")) {
        s_connected = 0;
        Wifi_RxClear();
        return;
    }

    /* Match anywhere in the buffer — resilient to partial +IPD headers */
    if (strstr(rx, "enroll_done")) {
        g_face_enrolled         = 1;
        s_waiting_enroll_done   = 0;
        esp_erase_from(rx);
        return;
    }

    if (strstr(rx, "face_ok")) {
        if (g_authEvt) {
            AuthFactor_t f = FACTOR_FACE;
            xQueueSend(g_authEvt, &f, 0);
        }
        Display_Line(0, "Face: OK!");
        esp_erase_from(rx);
        return;
    }

    char *ipd = strstr(rx, "+IPD,");
    if (!ipd) return;

    char *colon = strchr(ipd, ':');
    if (!colon) return;

    char *payload = colon + 1;

    /* pwd — remote PIN entry from browser */
    if (!strstr(payload, "\"t\":\"pwd\"")) {
        esp_erase_from(ipd);
        return;
    }

    char *pin_start = strstr(payload, "\"pin\":\"");
    if (!pin_start) { char *mark = ipd; while (mark <= colon) *mark++ = ' '; return; }
    pin_start += 7;

    char *pin_end = strchr(pin_start, '"');
    if (!pin_end) { char *mark = ipd; while (mark <= colon) *mark++ = ' '; return; }

    uint8_t pin_len = (uint8_t)(pin_end - pin_start);
    if (pin_len > 8) pin_len = 8;

    char pin[9] = {0};
    memcpy(pin, pin_start, pin_len);

    char *mark = ipd;
    while (mark <= pin_end) *mark++ = ' ';

    uint8_t ok = g_creds_ready &&
                 (strncmp(pin, g_creds.password, PWD_CMP_LEN) == 0);

    char result[48];
    snprintf(result, sizeof(result),
             "{\"t\":\"pwd_result\",\"ok\":%s}\n", ok ? "true" : "false");
    esp_send_json(result);

    if (ok) {
        AuthFactor_t f = FACTOR_PWD;
        xQueueSend(g_authEvt, &f, 0);
    }
}

/**
 * @brief Send a raw AT command and wait for an expected response.
 *
 * @param cmd AT command string (e.g., "AT+CWMODE_DEF=2\r\n")
 * @param expect Response string to wait for (e.g., "OK")
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return 1 if expected string was found, 0 on timeout
 */
static uint8_t esp_at_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    Wifi_RxClear();
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
    for (uint32_t t = 0; t < timeout_ms; t += 100) {
        if (strstr((char *)Wifi.RxBuffer, expect)) return 1;
        osDelay(100);
    }
    return 0;
}

/**
 * @brief ESP8266 task - initializes WiFi, manages SoftAP, TCP connection,
 *        and syncs display/log data with remote face recognition server.
 *
 * Initializes ESP8266 in SoftAP mode, attempts TCP connection to laptop.
 * Main loop syncs LCD display state, face enrollment/scan requests,
 * and access log entries to the remote server. Handles reconnection
 * with 10-second retry interval on disconnection.
 *
 * @param argument Unused (FreeRTOS task parameter)
 */
void EspTask(void *argument)
{
    g_esp_log_queue = xQueueCreate(8, sizeof(EspLogEntry_t));

    /* ── Step 1: power and handshake ── */
    Display_Line(1, "WiFi: starting..");
    Wifi_Enable();   /* pulls RST/EN lines, then waits 2 s */

    uint8_t module_ok = 0;
    for (uint8_t i = 0; i < 10; i++) {
        if (esp_at_cmd("AT\r\n", "OK", 1500)) { module_ok = 1; break; }
        osDelay(500);
    }
    if (!module_ok) {
        Display_Line(1, "WiFi: no module!");
        for (;;) osDelay(2000);
    }

    /* ── Step 2: set SoftAP mode and save to flash ── */
    Display_Line(1, "WiFi: set AP...");
    esp_at_cmd("AT+CWMODE_DEF=2\r\n", "OK", 3000);

    /* ── Step 3: restart so mode takes effect from flash ── */
    Display_Line(1, "WiFi: restarting");
    esp_at_cmd("AT+RST\r\n", "OK", 2000);
    osDelay(3000);

    for (uint8_t i = 0; i < 10; i++) {
        if (esp_at_cmd("AT\r\n", "OK", 1500)) break;
        osDelay(500);
    }

    /* ── Step 4: configure the AP (saved to flash) ── */
    Display_Line(1, "WiFi: config AP.");
    char ap_cmd[80];
    snprintf(ap_cmd, sizeof(ap_cmd),
             "AT+CWSAP_DEF=\"" AP_SSID "\",\"" AP_PASS "\",%d,3,4,0\r\n",
             AP_CHANNEL);
    if (!esp_at_cmd(ap_cmd, "OK", 5000)) {
        Display_Line(1, "WiFi: config fail");
    }

    Display_Line(1, "WiFi: AP Ready! ");
    osDelay(1500);

    /* ── Step 5: TCP client to laptop ── */
    Wifi_TcpIp_SetMultiConnection(false);
    s_connected = esp_try_connect();
    Display_Line(1, s_connected ? "Server: Online " : "Server: Offline");

    uint32_t reconnect_ts   = 0;
    uint32_t last_keepalive_ts = 0;
    uint8_t  prev_connected = s_connected;

    for (;;)
    {
        /* Always process incoming data first — before any send that would
         * call Wifi_RxClear() and discard unread responses (e.g. face_ok). */
        esp_check_incoming();

        /* Update LCD row 1 whenever connection state changes */
        if (s_connected != prev_connected) {
            prev_connected = s_connected;
            Display_Line(1, s_connected ? "Server: Online " : "Server: Offline");
        }

        if (!s_connected) {
            uint32_t retry_ms = s_waiting_enroll_done ? 2000u : 10000u;
            if (HAL_GetTick() - reconnect_ts > retry_ms) {
                reconnect_ts = HAL_GetTick();
                Wifi_TcpIp_SetMultiConnection(false);
                s_connected = esp_try_connect();
            }
            osDelay(s_waiting_enroll_done ? 100 : 500);
            continue;
        }

        /* Light keepalive while waiting for enroll_done (Python can take 15+ s) */
        if (s_waiting_enroll_done && (HAL_GetTick() - last_keepalive_ts) > 5000u) {
            last_keepalive_ts = HAL_GetTick();
            esp_send_json("{\"t\":\"ping\"}\n");
        }

        /* Enrollment trigger — sent once per setup/reset cycle */
        if (g_face_enroll_requested) {
            g_face_enroll_requested = 0;
            s_waiting_enroll_done   = 1;
            esp_send_json("{\"t\":\"enroll_reset\"}\n");
        }

        /* Face scan trigger — sent when user presses 'D' */
        if (s_face_scan_req) {
            s_face_scan_req = 0;
            esp_send_json("{\"t\":\"face_scan\"}\n");
        }

        /* Push LCD state — skip while waiting for enroll_done to keep TCP clear */
        if (g_lcd_dirty && !s_waiting_enroll_done) {
            g_lcd_dirty = 0;
            char l0[17], l1[17];
            osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
            memcpy(l0, (char *)g_lcd_line0, 17);
            memcpy(l1, (char *)g_lcd_line1, 17);
            osMutexRelease(g_lcd_mutex);

            char json[88];
            snprintf(json, sizeof(json),
                     "{\"t\":\"lcd\",\"l0\":\"%s\",\"l1\":\"%s\"}\n", l0, l1);
            esp_send_json(json);
        }

        /* Drain access log entries posted by AuthTask */
        EspLogEntry_t entry;
        while (xQueueReceive(g_esp_log_queue, &entry, 0) == pdTRUE) {
            char json[88];
            snprintf(json, sizeof(json),
                     "{\"t\":\"log\",\"ts\":%lu,\"ev\":\"%s\",\"fx\":\"%s\"}\n",
                     (unsigned long)entry.ts, entry.event, entry.factors);
            esp_send_json(json);
        }

        osDelay(s_waiting_enroll_done ? 20 : 100);
    }
}
