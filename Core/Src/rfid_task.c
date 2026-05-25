#include "rfid_task.h"
#include "auth_task.h"
#include "display.h"
#include "buzzer.h"
#include "rc522.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>

const osThreadAttr_t rfidTask_attr = {
    .name       = "RfidTask",
    .stack_size = 512,
    .priority   = (osPriority_t) osPriorityNormal,
};

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

        if (memcmp(str, g_creds.uid, 5) != 0)
        {
            /* Wrong card — rate limit beeps to avoid spam */
            uint32_t now = HAL_GetTick();
            if ((now - last_unknown_tick) > 1500U)
            {
                last_unknown_tick = now;
                Buzzer_BeepDenied();
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
