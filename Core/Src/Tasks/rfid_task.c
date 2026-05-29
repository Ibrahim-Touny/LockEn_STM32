/**
 * @file    rfid_task.c
 * @brief   RFID card scanning task - continuously monitors for valid RFID cards
 *          and posts authentication factor events to AuthTask queue.
 */

#include "Tasks/rfid_task.h"
#include "Tasks/auth_task.h"
#include "Abstractions/display.h"
#include "Drivers/buzzer.h"
#include "Drivers/rc522.h"
#include "Abstractions/sleep_manager.h"
#include "Tasks/esp_task.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>

const osThreadAttr_t rfidTask_attr = {
    .name       = "RfidTask",
    .stack_size = 1024,
    .priority   = (osPriority_t) osPriorityNormal,
};

/**
 * @brief RFID scanning task - waits for valid cards and posts auth factors.
 *
 * Continuously polls for RFID cards. When a valid card is detected, wakes the
 * display and posts FACTOR_RFID to the auth queue once per session. Invalid
 * cards are logged with rate limiting to avoid spam.
 *
 * @param argument Unused (FreeRTOS task parameter)
 */
void RfidTask(void *argument)
{
    /* Wait for setup to finish */
    while (!g_creds_ready) osDelay(50);

    uint32_t last_session = g_session_id - 1u; /* ensure first session can fire */
    uint32_t last_unknown_tick = 0;

    for (;;)
    {
        /* If credentials were wiped (re-setup in progress), pause */
        if (!g_creds_ready) { osDelay(100); continue; }

        uint8_t str[MAX_LEN];
        if (MFRC522_Request(PICC_REQIDL, str) != MI_OK) { osDelay(200); continue; }
        if (MFRC522_Anticoll(str)             != MI_OK) { osDelay(200); continue; }

        /* Card in range — wake display and reset idle timer regardless of card validity */
        if (g_system_sleeping) Sleep_Exit();
        Sleep_UpdateActivity();

        if (memcmp(str, g_creds.uid, 5) != 0)
        {
            /* Wrong card — rate limit beeps + display to avoid spam */
            uint32_t now = HAL_GetTick();
            if ((now - last_unknown_tick) > 1500U)
            {
                last_unknown_tick = now;
                Buzzer_BeepDenied();
                Display_Both("Wrong Card!", "Access Denied");

                if (g_esp_log_queue)
                {
                    EspLogEntry_t entry;
                    entry.ts = HAL_GetTick();
                    strncpy(entry.event,   "WRONG_RFID", sizeof(entry.event)   - 1);
                    entry.event[sizeof(entry.event) - 1] = '\0';
                    strncpy(entry.factors, "RFID",       sizeof(entry.factors) - 1);
                    entry.factors[sizeof(entry.factors) - 1] = '\0';
                    xQueueSend(g_esp_log_queue, &entry, 0);
                }
            }
            osDelay(200);
            continue;
        }

        /* Valid card: post once per session */
        uint32_t cur = g_session_id;
        if (cur != last_session)
        {
            last_session = cur;
            Buzzer_BeepOK();
            AuthFactor_t f = FACTOR_RFID;
            xQueueSend(g_authEvt, &f, portMAX_DELAY);
        }
        else
        {
            /* Already contributed this session — wait until session advances */
            osDelay(500);
        }
    }
}
