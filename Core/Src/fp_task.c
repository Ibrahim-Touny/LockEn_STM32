#include "fp_task.h"
#include "auth_task.h"
#include "display.h"
#include "buzzer.h"
#include "R307.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"

const osThreadAttr_t fpTask_attr = {
    .name       = "FpTask",
    .stack_size = 1024,
    .priority   = (osPriority_t) osPriorityNormal,
};

void FpTask(void *argument)
{
    /* Wait for setup to finish */
    while (!g_creds_ready) osDelay(50);

    uint32_t last_session = g_session_id - 1u;
    uint32_t last_fail_tick = 0;

    for (;;)
    {
        if (!g_creds_ready) { osDelay(100); continue; }

        /* Already posted for this session — wait until it ends */
        if (g_session_id == last_session) { osDelay(100); continue; }

        /* R307_Verify blocks until a finger is placed (up to 20 s) or times out.
         * FreeRTOS preemption keeps other tasks running during the UART waits. */
        uint16_t id = 0, score = 0;
        uint32_t session_at_start = g_session_id;
        HAL_StatusTypeDef st = R307_Verify(&id, &score);

        if (!g_creds_ready || g_session_id != session_at_start) continue;

        if (st == HAL_TIMEOUT)
        {
            /* No finger present — stay silent */
            osDelay(50);
            continue;
        }

        if (st != HAL_OK)
        {
            /* Bad fingerprint — rate limit feedback */
            uint32_t now = HAL_GetTick();
            if ((now - last_fail_tick) > 1500U)
            {
                last_fail_tick = now;
                Buzzer_BeepDenied();
            }
            osDelay(100);
            continue;
        }

        /* Fingerprint matched — post once per session */
        uint32_t cur = g_session_id;
        if (cur != last_session)
        {
            last_session = cur;
            Buzzer_BeepOK();
            AuthFactor_t f = FACTOR_FP;
            xQueueSend(g_authEvt, &f, portMAX_DELAY);
        }
    }
}
