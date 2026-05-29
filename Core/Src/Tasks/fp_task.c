/**
 * @file    fp_task.c
 * @brief   Fingerprint scanner task - waits for user keypad trigger (C button)
 *          then captures and verifies a finger against the enrolled template.
 */

#include "Tasks/fp_task.h"
#include "Tasks/auth_task.h"
#include "Abstractions/display.h"
#include "Drivers/buzzer.h"
#include "Drivers/R307.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"

const osThreadAttr_t fpTask_attr = {
    .name       = "FpTask",
    .stack_size = 1024,
    .priority   = (osPriority_t) osPriorityNormal,
};

volatile uint8_t g_fp_scan_requested = 0;

/**
 * @brief Fingerprint scanning task - triggered by keypad button C.
 *
 * Waits for the user to press the 'C' key on the keypad, then attempts
 * a fingerprint verification. Posts FACTOR_FP to auth queue on match.
 * Handles invalid or no-finger scenarios gracefully.
 *
 * @param argument Unused (FreeRTOS task parameter)
 */
void FpTask(void *argument)
{
    while (!g_creds_ready) osDelay(50);

    uint32_t last_session = g_session_id - 1u;

    for (;;)
    {
        if (!g_creds_ready) { osDelay(100); continue; }

        /* Already posted fingerprint for this session — clear any stale
         * C-press request so it does not auto-fire on the next session. */
        if (g_session_id == last_session)
        {
            g_fp_scan_requested = 0;
            osDelay(100);
            continue;
        }

        /* Do nothing until the user explicitly presses C */
        if (!g_fp_scan_requested) { osDelay(100); continue; }

        g_fp_scan_requested = 0;  /* consume the request */

        uint16_t id = 0, score = 0;
        uint32_t session_at_start = g_session_id;
        HAL_StatusTypeDef st = R307_Verify(&id, &score);

        if (!g_creds_ready || g_session_id != session_at_start) continue;

        if (st == HAL_TIMEOUT)
        {
            Display_Both("No Finger", "Detected (C=retry)");
            continue;
        }

        if (st != HAL_OK)
        {
            Display_Both("Bad Finger!", "Try again (C)");
            Buzzer_BeepDenied();
            continue;
        }

        /* Match — post once per session */
        uint32_t cur = g_session_id;
        if (cur != last_session)
        {
            last_session = cur;
            Display_Line(0, "Finger OK!");
            Buzzer_BeepOK();
            AuthFactor_t f = FACTOR_FP;
            xQueueSend(g_authEvt, &f, portMAX_DELAY);
        }
    }
}
