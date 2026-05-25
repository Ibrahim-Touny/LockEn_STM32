#include "auth_task.h"
#include "storage.h"
#include "display.h"
#include "tamper_task.h"
#include "main.h"
#include "rc522.h"
#include "R307.h"
#include "mpu6050.h"
#include "i2c-lcd.h"
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
#define BUZZER_PORT     GPIOC
#define BUZZER_PIN      GPIO_PIN_15

/* ── Config ─────────────────────────────────────────────────────────────── */
#define PASSWORD_LEN      4
#define MAX_ATTEMPTS      3
#define NUM_FINGERS       1
#define UNLOCK_MS         3000U
#define LOCKOUT_ALARM_MS  1500U

/* ── State machine ──────────────────────────────────────────────────────── */
typedef enum {
    AUTH_ST_INIT,
    AUTH_ST_SETUP,
    AUTH_ST_IDLE,
    AUTH_ST_FP,
    AUTH_ST_PWD,
    AUTH_ST_GRANTED,
    AUTH_ST_ERROR,
} AuthState_t;

/* ── Module state ───────────────────────────────────────────────────────── */
static uint8_t          s_rfid_ok;
static CredentialStore_t s_creds;

/* ── External handles ───────────────────────────────────────────────────── */
extern I2C_HandleTypeDef  hi2c1;
extern I2C_HandleTypeDef  hi2c2;
extern UART_HandleTypeDef huart1;

/* ═══════════════════════════════════════════════════════════════════════════
 *  BUZZER
 * ═══════════════════════════════════════════════════════════════════════════ */
static void Buzzer_On(void)  { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET); }
static void Buzzer_Off(void) { HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); }

static void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    osDelay(ms);
    Buzzer_Off();
}

static void Buzzer_BeepOK(void)
{
    Buzzer_Beep(80); osDelay(80); Buzzer_Beep(80);
}

static void Buzzer_BeepGranted(void) { Buzzer_Beep(400); }

static void Buzzer_BeepDenied(void)
{
    for (int i = 0; i < 3; i++) { Buzzer_Beep(120); osDelay(100); }
}

static void Buzzer_Alarm(uint32_t total_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < total_ms)
    {
        Buzzer_On();  osDelay(100);
        Buzzer_Off(); osDelay(100);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SOLENOID / RESET BUTTON
 * ═══════════════════════════════════════════════════════════════════════════ */
static void Solenoid_Unlock(void) { HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_SET); }
static void Solenoid_Lock(void)   { HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_RESET); }

static uint8_t ResetButton_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  KEYPAD
 * ═══════════════════════════════════════════════════════════════════════════ */
static char read_keypad(void)
{
    /* Row 1 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '1'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '2'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '3'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'A'; }
    /* Row 2 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '4'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '5'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '6'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'B'; }
    /* Row 3 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '7'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '8'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '9'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'C'; }
    /* Row 4 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_RESET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '*'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '0'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '#'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'D'; }

    return 0x01;
}

/* '#' confirms, '*' is backspace, shows '*' on LCD row 1 for each digit.
 * Returns when '#' pressed with at least one digit entered.             */
static void get_password(char *entered, uint8_t max_len)
{
    uint8_t idx = 0;
    memset(entered, 0, max_len + 1);
    Display_Line(1, "");

    while (1)
    {
        char key = read_keypad();
        if (key == 0x01) { osDelay(10); continue; }

        Buzzer_Beep(40);

        if (key == '#')
        {
            if (idx == 0) continue;   /* require at least one digit */
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
        osDelay(120);   /* key debounce */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SETUP HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */
static void setup_register_card(void)
{
    Display_Both("Scan Admin Card", "to Register...");

    uint8_t str[MAX_LEN];
    while (1)
    {
        uint8_t status = MFRC522_Request(PICC_REQIDL, str);
        if (status != MI_OK) { osDelay(200); continue; }

        status = MFRC522_Anticoll(str);
        if (status != MI_OK) { osDelay(200); continue; }

        memcpy(s_creds.uid, str, 5);

        Buzzer_BeepOK();

        /* UID as 10-char hex string — always fits in 16 chars */
        char uidMsg[17];
        snprintf(uidMsg, sizeof(uidMsg), "%02X%02X%02X%02X%02X",
                 s_creds.uid[0], s_creds.uid[1], s_creds.uid[2],
                 s_creds.uid[3], s_creds.uid[4]);
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
    get_password(s_creds.password, PASSWORD_LEN);
    Buzzer_BeepOK();
    Display_Timed("Password Saved!", "", 1500);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STATE HANDLERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static AuthState_t do_init(void)
{
    Buzzer_Off();
    Solenoid_Lock();

    /* RFID init with post-reset settling time */
    MFRC522_Init();
    osDelay(500);

    /* I2C1 bus reset — clears any lock-up from cold boot */
    __HAL_RCC_I2C1_FORCE_RESET();
    osDelay(100);
    __HAL_RCC_I2C1_RELEASE_RESET();
    osDelay(100);
    hi2c1.State = HAL_I2C_STATE_RESET;
    HAL_I2C_Init(&hi2c1);
    osDelay(100);

    lcd_init();
    lcd_clear();

    /* RFID version sanity check */
    s_rfid_ok = 1;
    uint8_t rfid_ver = Read_MFRC522(VersionReg);
    if (rfid_ver == 0x00 || rfid_ver == 0xFF)
    {
        s_rfid_ok = 0;
        Buzzer_BeepDenied();
        Display_Timed("RFID NOT FOUND", "RFID Disabled", 4000);
        lcd_clear();
    }

    /* MPU6050 init — sets g_mpu_ok which unblocks TamperTask */
    if (MPU6050_Init(&hi2c2, MPU6050_ACCEL_RANGE_8G, MPU6050_GYRO_RANGE_1000) == INIT_SUCCESS)
    {
        g_mpu_ok = 1;
    }
    else
    {
        Buzzer_BeepDenied();
        Display_Timed("MPU NOT FOUND", "Tamper Disabled", 4000);
        lcd_clear();
    }

    /* Fingerprint sensor init:
     * R307 emits a 12-byte startup packet 500–2000 ms after power-on.
     * Drain the UART until silent, then retry VerifyPassword up to 5×. */
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
        /* Hard fault: FP sensor not responding — show message and halt */
        Buzzer_BeepDenied();
        Display_Error("Check FP sensor!");
        for (;;) osDelay(1000);
    }

    Buzzer_Beep(80);
    Display_Timed("FP Ready!", "", 1000);

    /* Load stored credentials unless reset button held at boot */
    if (Storage_HasValid() && !ResetButton_IsPressed())
    {
        Storage_Load(&s_creds);
        return AUTH_ST_IDLE;
    }

    return AUTH_ST_SETUP;
}

static AuthState_t do_setup(void)
{
    memset(&s_creds, 0, sizeof(s_creds));
    R307_ClearAllFingerprints();
    Storage_Erase();

    if (s_rfid_ok) setup_register_card();
    setup_enroll_fingerprints();
    setup_register_password();

    /* Persist credentials */
    s_creds.magic = STORAGE_MAGIC;
    Storage_Save(&s_creds);

    /* Re-init RFID to clear any stale IRQ bits from setup activity */
    MFRC522_Init();
    osDelay(100);

    Display_Timed("Setup Done!", "Scan your card", 2000);
    return AUTH_ST_IDLE;
}

static AuthState_t do_idle(void)
{
    Display_Idle();

    /* Drain stale tamper events (may have queued during setup/unlock) */
    if (g_tamperEvt)
    {
        uint8_t evt;
        while (xQueueReceive(g_tamperEvt, &evt, 0) == pdTRUE) {}
    }

    while (1)
    {
        /* ── Reset button (debounced) → re-enroll everything ─────────── */
        if (ResetButton_IsPressed())
        {
            osDelay(50);
            if (ResetButton_IsPressed())
            {
                Solenoid_Lock();
                Buzzer_Beep(200);
                Display_Timed("Resetting...", "Please wait", 1500);
                MFRC522_Init();
                osDelay(100);
                Storage_Erase();
                return AUTH_ST_SETUP;
            }
        }

        /* ── Tamper event from TamperTask ──────────────────────────── */
        if (g_tamperEvt)
        {
            uint8_t evt;
            if (xQueueReceive(g_tamperEvt, &evt, 0) == pdTRUE)
            {
                Display_Both("!! TAMPER !!", "Device moved");
                Buzzer_Alarm(1500);
                Display_Idle();
                continue;
            }
        }

        /* ── RFID card scan ────────────────────────────────────────── */
        if (s_rfid_ok)
        {
            uint8_t str[MAX_LEN];
            uint8_t status = MFRC522_Request(PICC_REQIDL, str);
            if (status != MI_OK) { osDelay(200); continue; }

            status = MFRC522_Anticoll(str);
            if (status != MI_OK) { osDelay(200); continue; }

            uint8_t sNum[5];
            memcpy(sNum, str, 5);

            if (memcmp(sNum, s_creds.uid, 5) != 0)
            {
                Buzzer_BeepDenied();
                Display_Timed("Access Denied!", "Unknown Card", 2000);
                Display_Idle();
                continue;
            }

            Buzzer_BeepOK();
            Display_Timed("Card OK!", "Place finger...", 1000);
        }
        else
        {
            /* RFID disabled — go straight to fingerprint */
            Display_Timed("Place finger...", "", 800);
        }

        return AUTH_ST_FP;
    }
}

static AuthState_t do_fp(void)
{
    uint16_t matched_id, score;
    HAL_StatusTypeDef fp_status = R307_Verify(&matched_id, &score);

    if (fp_status != HAL_OK)
    {
        Buzzer_BeepDenied();
        Display_Timed("Bad Finger!", "Access Denied", 2000);
        return AUTH_ST_IDLE;
    }

    Buzzer_BeepOK();
    Display_Timed("Finger OK!", "Enter Password:", 1000);
    return AUTH_ST_PWD;
}

static AuthState_t do_pwd(void)
{
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
    {
        char msg[17];
        snprintf(msg, sizeof(msg), "Pass (%d/%d):", attempt, MAX_ATTEMPTS);
        Display_Line(0, msg);
        Display_Line(1, "");

        char entered[PASSWORD_LEN + 1];
        get_password(entered, PASSWORD_LEN);

        if (strcmp(entered, s_creds.password) == 0)
        {
            return AUTH_ST_GRANTED;
        }

        int left = MAX_ATTEMPTS - attempt;
        char remain[17];
        if (left > 0) snprintf(remain, sizeof(remain), "%d tries left", left);
        else          snprintf(remain, sizeof(remain), "Locked out!");

        Buzzer_BeepDenied();
        Display_Timed("Wrong Password!", remain, 1500);
    }

    return AUTH_ST_ERROR;
}

static AuthState_t do_granted(void)
{
    Display_Both("Access Granted!", "Welcome!");
    Buzzer_BeepGranted();

    Solenoid_Unlock();
    osDelay(UNLOCK_MS);
    Solenoid_Lock();

    /* Re-init RFID to clear stale IRQ bits from the transaction */
    MFRC522_Init();
    osDelay(50);

    return AUTH_ST_IDLE;
}

static AuthState_t do_error(void)
{
    /* Locked out after too many attempts.
     * Alarm + display, then require a reset button press to retry.
     * Credentials are kept — no re-enrollment needed.               */
    Display_Both("Access Denied!", "Too many tries");
    Buzzer_Alarm(LOCKOUT_ALARM_MS);

    Display_Error("Press reset btn");

    /* Wait for reset button press and release */
    while (!ResetButton_IsPressed()) osDelay(100);
    osDelay(50);
    while (ResetButton_IsPressed()) osDelay(50);

    return AUTH_ST_IDLE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TASK ENTRY
 * ═══════════════════════════════════════════════════════════════════════════ */
void AuthTask(void *argument)
{
    AuthState_t state = AUTH_ST_INIT;

    for (;;)
    {
        switch (state)
        {
            case AUTH_ST_INIT:    state = do_init();    break;
            case AUTH_ST_SETUP:   state = do_setup();   break;
            case AUTH_ST_IDLE:    state = do_idle();    break;
            case AUTH_ST_FP:      state = do_fp();      break;
            case AUTH_ST_PWD:     state = do_pwd();     break;
            case AUTH_ST_GRANTED: state = do_granted(); break;
            case AUTH_ST_ERROR:   state = do_error();   break;
            default:              state = AUTH_ST_INIT; break;
        }
    }
}
