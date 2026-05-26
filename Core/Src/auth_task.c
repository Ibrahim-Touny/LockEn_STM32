#include "auth_task.h"
#include "storage.h"
#include "display.h"
#include "buzzer.h"
#include "keypad.h"
#include "tamper_task.h"
#include "main.h"
#include "rc522.h"
#include "R307.h"
#include "mpu6050.h"
#include "i2c-lcd.h"
#include "periph_init.h"
#include "esp_task.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>

/* ── Hardware aliases ───────────────────────────────────────────────────── */
#define RESET_BTN_PORT  GPIOB
#define RESET_BTN_PIN   GPIO_PIN_1
#define SOLENOID_PORT   GPIOB
#define SOLENOID_PIN    GPIO_PIN_2

#define PASSWORD_LEN  4
#define NUM_FINGERS   1
#define UNLOCK_MS     3000U
#define SESSION_TIMEOUT_MS  30000U

/* ── Task attributes ────────────────────────────────────────────────────── */
const osThreadAttr_t authTask_attr = {
    .name       = "AuthTask",
    .stack_size = 2048,
    .priority   = (osPriority_t) osPriorityNormal,
};

/* ── Shared globals (declared extern in auth_task.h) ────────────────────── */
QueueHandle_t     g_authEvt   = NULL;
volatile uint8_t  g_creds_ready = 0;
volatile uint32_t g_session_id  = 0;
CredentialStore_t g_creds;

/* ── Module state ───────────────────────────────────────────────────────── */
static uint8_t s_rfid_ok = 0;

/* ════════════════════════════════════════════════════════════════════════════
 *  ESP LOG HELPER
 * ════════════════════════════════════════════════════════════════════════════ */
static void esp_log(const char *event, uint8_t fx_mask)
{
    if (!g_esp_log_queue) return;
    EspLogEntry_t entry;
    entry.ts = HAL_GetTick();
    strncpy(entry.event, event, sizeof(entry.event) - 1);
    entry.event[sizeof(entry.event) - 1] = '\0';

    char fx[12] = {0};
    if (fx_mask & FACTOR_RFID) strncat(fx, "RFID+", sizeof(fx) - strlen(fx) - 1);
    if (fx_mask & FACTOR_FP)   strncat(fx, "FP+",   sizeof(fx) - strlen(fx) - 1);
    if (fx_mask & FACTOR_PWD)  strncat(fx, "PWD+",  sizeof(fx) - strlen(fx) - 1);
    uint8_t fxlen = (uint8_t)strlen(fx);
    if (fxlen > 0) fx[fxlen - 1] = '\0'; /* strip trailing '+' */
    strncpy(entry.factors, fx, sizeof(entry.factors) - 1);
    entry.factors[sizeof(entry.factors) - 1] = '\0';

    xQueueSend(g_esp_log_queue, &entry, 0);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  HARDWARE HELPERS
 * ════════════════════════════════════════════════════════════════════════════ */
static void Solenoid_Unlock(void) { HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_SET);   }
static void Solenoid_Lock(void)   { HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_RESET); }

static uint8_t ResetButton_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PASSWORD ENTRY  (setup only — runs before g_creds_ready, no mutex needed)
 * ════════════════════════════════════════════════════════════════════════════ */
static void get_password(char *entered, uint8_t max_len)
{
    uint8_t idx = 0;
    memset(entered, 0, max_len + 1u);
    Display_Line(1, "");

    for (;;)
    {
        char key = Keypad_Read();
        if (key == 0) { osDelay(10); continue; }

        Buzzer_Beep(40);

        if (key == '#')
        {
            if (idx == 0) continue;
            entered[idx] = '\0';
            return;
        }
        else if (key == '*')
        {
            if (idx > 0)
            {
                idx--;
                entered[idx] = '\0';
                lcd_put_cur(1, idx);
                lcd_send_data(' ');
                lcd_put_cur(1, idx);
            }
        }
        else if (idx < max_len)
        {
            entered[idx] = key;
            idx++;
            lcd_put_cur(1, idx - 1);
            lcd_send_data('*');
        }
        osDelay(120);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  SETUP HELPERS
 * ════════════════════════════════════════════════════════════════════════════ */
static void setup_register_card(void)
{
    Display_Both("Scan Admin Card", "to Register...");

    uint8_t str[MAX_LEN];
    for (;;)
    {
        if (MFRC522_Request(PICC_REQIDL, str) != MI_OK) { osDelay(200); continue; }
        if (MFRC522_Anticoll(str)             != MI_OK) { osDelay(200); continue; }

        memcpy(g_creds.uid, str, 5);
        Buzzer_BeepOK();

        char uidMsg[17];
        snprintf(uidMsg, sizeof(uidMsg), "%02X%02X%02X%02X%02X",
                 g_creds.uid[0], g_creds.uid[1], g_creds.uid[2],
                 g_creds.uid[3], g_creds.uid[4]);
        Display_Timed("Card Saved!", uidMsg, 2000);
        return;
    }
}

static void setup_enroll_fingerprints(void)
{
    for (int i = 1; i <= NUM_FINGERS; i++)
    {
        char msg[17];
        snprintf(msg, sizeof(msg), "Enroll Finger %d", i);
        Display_Line(0, msg);
        Display_Line(1, "");

        int retries = 3;
        HAL_StatusTypeDef result;
        do {
            result = R307_Enroll((uint16_t)i);
        } while (result != HAL_OK && --retries > 0);

        if (result == HAL_OK)
        {
            Buzzer_BeepOK();
            Display_Timed("Enroll OK!", "", 1500);
        }
        else
        {
            Buzzer_BeepDenied();
            Display_Timed("Enroll Failed!", "Try again", 1500);
        }
    }
}

static void setup_register_password(void)
{
    Display_Timed("Set Password:", "Then press #", 2000);
    Display_Line(0, "Enter Password:");
    get_password(g_creds.password, PASSWORD_LEN);
    Buzzer_BeepOK();
    Display_Timed("Password Saved!", "", 1500);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  INIT
 * ════════════════════════════════════════════════════════════════════════════ */
static void do_init(void)
{
    Buzzer_Off();
    Solenoid_Lock();

    /* RFID init with settling time */
    MFRC522_Init();
    osDelay(500);

    /* I2C1 bus reset — clears lock-up from cold boot */
    __HAL_RCC_I2C1_FORCE_RESET();
    osDelay(100);
    __HAL_RCC_I2C1_RELEASE_RESET();
    osDelay(100);
    hi2c1.State = HAL_I2C_STATE_RESET;
    MX_I2C1_Init();
    osDelay(100);

    /* LCD init + create mutex */
    Display_Init();

    /* RFID version sanity check */
    uint8_t rfid_ver = Read_MFRC522(VersionReg);
    if (rfid_ver == 0x00 || rfid_ver == 0xFF)
    {
        s_rfid_ok = 0;
        Buzzer_BeepDenied();
        Display_Timed("RFID NOT FOUND", "RFID Disabled", 4000);
        lcd_clear();
    }
    else
    {
        s_rfid_ok = 1;
    }

    /* MPU6050 init — sets g_mpu_ok which unblocks TamperTask */
    if (MPU6050_Init(&hi2c2, MPU6050_ACCEL_RANGE_8G, MPU6050_GYRO_RANGE_1000) == INIT_SUCCESS)
    {
        g_mpu_ok = 1;
    }
    else
    {
        Display_Timed("MPU NOT FOUND", "Tamper Disabled", 4000);
        lcd_clear();
    }

    /* Fingerprint sensor: drain startup packet then retry VerifyPassword */
    Display_Both("FP Init...", "");
    osDelay(2000);
    {
        uint8_t b;
        while (HAL_UART_Receive(&huart1, &b, 1, 500) == HAL_OK) {}
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }

    uint8_t fp_ok = 0;
    for (int fp_try = 0; fp_try < 5 && !fp_ok; fp_try++)
    {
        if (R307_VerifyPassword(0x00000000) == HAL_OK)
        {
            fp_ok = 1;
        }
        else
        {
            osDelay(300);
            uint8_t b;
            while (HAL_UART_Receive(&huart1, &b, 1, 300) == HAL_OK) {}
            __HAL_UART_CLEAR_OREFLAG(&huart1);
        }
    }

    if (!fp_ok)
    {
        Buzzer_BeepDenied();
        Display_Error("Check FP sensor!");
        for (;;) osDelay(1000);
    }

    R307_SetUiLogEnabled(0);

    Buzzer_Beep(80);
    Display_Timed("FP Ready!", "", 1000);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  SETUP
 * ════════════════════════════════════════════════════════════════════════════ */
static void do_setup(void)
{
    memset(&g_creds, 0, sizeof(g_creds));
    R307_ClearAllFingerprints();
    Storage_Erase();

    if (s_rfid_ok) setup_register_card();
    R307_SetUiLogEnabled(1);
    setup_enroll_fingerprints();
    R307_SetUiLogEnabled(0);
    setup_register_password();

    g_creds.magic = STORAGE_MAGIC;
    Storage_Save(&g_creds);

    MFRC522_Init();
    osDelay(100);

    Display_Timed("Setup Done!", "Scan 2 factors", 2000);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  TASK ENTRY
 * ════════════════════════════════════════════════════════════════════════════ */
void AuthTask(void *argument)
{
    /* Queue must exist before sensor tasks check g_creds_ready */
    g_authEvt = xQueueCreate(10, sizeof(AuthFactor_t));

    do_init();

    if (Storage_HasValid() && !ResetButton_IsPressed())
        Storage_Load(&g_creds);
    else
        do_setup();

    /* Drain any stale tamper events from init/setup */
    if (g_tamperEvt) {
        uint8_t discard;
        while (xQueueReceive(g_tamperEvt, &discard, 0) == pdTRUE) {}
    }

    g_session_id  = 1;
    g_creds_ready = 1;

    Display_Both("Scan 2 factors", "Any order OK");
    osDelay(1500);
    Display_Both("Scan 2 factors", "");

    uint8_t  bitmask      = 0;
    uint32_t session_start = 0;

    for (;;)
    {
        /* ── Reset button → full re-setup ──────────────────────────────── */
        if (ResetButton_IsPressed())
        {
            osDelay(50);
            if (ResetButton_IsPressed())
            {
                g_creds_ready = 0;
                osDelay(300);           /* let sensor tasks reach idle check */
                Solenoid_Lock();
                Buzzer_Beep(200);
                Display_Timed("Resetting...", "Please wait", 1500);
                MFRC522_Init();
                osDelay(100);

                /* Drain queue before new session */
                AuthFactor_t discard;
                while (xQueueReceive(g_authEvt, &discard, 0) == pdTRUE) {}
                bitmask      = 0;
                session_start = 0;

                do_setup();

                /* Drain stale tamper events accumulated during setup */
                if (g_tamperEvt) {
                    uint8_t tevt;
                    while (xQueueReceive(g_tamperEvt, &tevt, 0) == pdTRUE) {}
                }

                g_session_id++;
                g_creds_ready = 1;
                Display_Both("Scan 2 factors", "");
                continue;
            }
        }

        /* ── Tamper event ──────────────────────────────────────────────── */
        if (g_tamperEvt)
        {
            uint8_t tevt;
            if (xQueueReceive(g_tamperEvt, &tevt, 0) == pdTRUE)
            {
                Display_Both("!! TAMPER !!", "Device moved");
                Buzzer_Alarm(1500);
                esp_log("TAMPER", 0);
                bitmask      = 0;
                session_start = 0;
                g_session_id++;
                Display_Both("Scan 2 factors", "");
                continue;
            }
        }

        /* ── Session timeout (30 s after first factor received) ────────── */
        if (bitmask != 0 && (HAL_GetTick() - session_start) > SESSION_TIMEOUT_MS)
        {
            Display_Timed("Session timeout", "Try again", 1500);
            esp_log("TIMEOUT", bitmask);
            bitmask      = 0;
            session_start = 0;
            g_session_id++;
            Display_Both("Scan 2 factors", "");
            continue;
        }

        /* ── Receive a verified factor from sensor tasks ───────────────── */
        AuthFactor_t factor;
        if (xQueueReceive(g_authEvt, &factor, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        if (bitmask == 0) session_start = HAL_GetTick();
        bitmask |= (uint8_t)factor;

        /* popcount >= 2 means at least two different factors received */
        if ((bitmask & (bitmask - 1u)) != 0u)
        {
            Display_Both("Access Granted!", "Welcome!");
            Buzzer_BeepGranted();
            esp_log("GRANTED", bitmask);
            Solenoid_Unlock();
            osDelay(UNLOCK_MS);
            Solenoid_Lock();

            MFRC522_Init();
            osDelay(50);

            bitmask      = 0;
            session_start = 0;
            g_session_id++;
            Display_Both("Scan 2 factors", "");
        }
        else
        {
            Display_Line(0, "1 done, 1 more!");
        }
    }
}
