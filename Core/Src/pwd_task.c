#include "pwd_task.h"
#include "auth_task.h"
#include "esp_task.h"
#include "fp_task.h"
#include "display.h"
#include "buzzer.h"
#include "keypad.h"
#include "i2c-lcd.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

#define PASSWORD_LEN  4
#define MAX_ATTEMPTS  3

const osThreadAttr_t pwdTask_attr = {
    .name       = "PwdTask",
    .stack_size = 1024,
    .priority   = (osPriority_t) osPriorityNormal,
};

static uint8_t wait_for_first_key(uint32_t expected_session, char *out_key)
{
    for (;;)
    {
        if (g_session_id != expected_session) return 0;

        char key = Keypad_Read();
        if (key == 0) { osDelay(20); continue; }

        if (key == '#' || key == '*') { osDelay(20); continue; }

        if (key == 'D') {
            if (EspTask_IsConnected()) {
                EspTask_RequestFaceScan();
                Display_Line(0, "Scanning Cam..");
            } else {
                Display_Line(0, "No WiFi!");
            }
            Display_Line(1, "");
            osDelay(20);
            continue;
        }

        if (key == 'C') {
            g_fp_scan_requested = 1;
            Display_Line(0, "Place Finger..");
            Display_Line(1, "");
            osDelay(20);
            continue;
        }

        /* A and B are not valid first-key inputs */
        if (key == 'A' || key == 'B') { osDelay(20); continue; }

        *out_key = key;
        return 1;
    }
}

/* Read up to max_len keypresses, echo '*' per digit on LCD row 1.
 * '#' confirms, '*' is backspace. Returns 1 on success, 0 if the
 * session ID changes mid-entry (another factor pair already passed). */
static uint8_t read_password(char *out, uint8_t max_len,
                              uint32_t expected_session,
                              char first_key, uint8_t has_first)
{
    uint8_t idx = 0;
    memset(out, 0, max_len + 1u);

    if (has_first && idx < max_len)
    {
        out[idx++] = first_key;
        osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
        lcd_put_cur(1, idx - 1);
        lcd_send_data('*');
        osMutexRelease(g_lcd_mutex);
        Buzzer_Beep(40);
    }

    for (;;)
    {
        if (g_session_id != expected_session) return 0;

        char key = Keypad_Read();
        if (key == 0) { osDelay(20); continue; }

        Buzzer_Beep(40);

        if (key == '#')
        {
            if (idx == 0) continue;
            out[idx] = '\0';
            return 1;
        }
        else if (key == '*')
        {
            if (idx > 0)
            {
                idx--;
                out[idx] = '\0';
                osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
                lcd_put_cur(1, idx);
                lcd_send_data(' ');
                lcd_put_cur(1, idx);
                osMutexRelease(g_lcd_mutex);
            }
        }
        else if (key == 'A' || key == 'B' || key == 'C' || key == 'D')
        {
            /* ignore function keys during password entry */
        }
        else if (idx < max_len)
        {
            out[idx] = key;
            idx++;
            osMutexAcquire(g_lcd_mutex, portMAX_DELAY);
            lcd_put_cur(1, idx - 1);
            lcd_send_data('*');
            osMutexRelease(g_lcd_mutex);
        }
        osDelay(120);
    }
}

void PwdTask(void *argument)
{
    while (!g_creds_ready) osDelay(50);

    uint32_t posted_session = g_session_id - 1u;
    uint32_t track_session  = g_session_id - 1u;
    uint8_t  attempts  = 0;
    uint8_t  pwd_locked = 0;

    for (;;)
    {
        if (!g_creds_ready) { osDelay(100); continue; }

        uint32_t cur = g_session_id;

        /* New session: reset per-session attempt tracking */
        if (cur != track_session)
        {
            track_session = cur;
            attempts  = 0;
            pwd_locked = 0;
        }

        /* Already contributed this session */
        if (cur == posted_session) { osDelay(100); continue; }

        /* Locked out for this session */
        if (pwd_locked) { osDelay(100); continue; }

        /* Wait for user to start password entry */
        char first_key = 0;
        if (!wait_for_first_key(cur, &first_key)) continue;

        /* Show attempt prompt only after first keypress */
        char prompt[17];
        snprintf(prompt, sizeof(prompt), "Pass (%d/%d):",
             attempts + 1, MAX_ATTEMPTS);
        Display_Line(0, prompt);
        Display_Line(1, "");

        /* Collect password — aborts if session advances */
        char entered[PASSWORD_LEN + 1];
        if (!read_password(entered, PASSWORD_LEN, cur, first_key, 1)) continue;

        /* Session may have changed while we were in read_password */
        if (g_session_id != cur) continue;

        if (strncmp(entered, g_creds.password, PASSWORD_LEN + 1) == 0)
        {
            posted_session = cur;
            Display_Line(1, "Pass OK!");
            Buzzer_BeepOK();
            AuthFactor_t f = FACTOR_PWD;
            xQueueSend(g_authEvt, &f, portMAX_DELAY);
        }
        else
        {
            attempts++;
            if (attempts >= MAX_ATTEMPTS)
            {
                pwd_locked = 1;
                Buzzer_BeepDenied();
                Display_Timed("Pwd locked!", "", 2000);
            }
            else
            {
                char remain[17];
                snprintf(remain, sizeof(remain), "%d tries left",
                         MAX_ATTEMPTS - attempts);
                Buzzer_BeepDenied();
                Display_Timed("Wrong password!", remain, 1500);
            }
        }
    }
}
