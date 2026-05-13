/* USER CODE BEGIN Header */
/**
  **************************
  * @file           : main.c
  * @brief          : Card + Fingerprint + Password Access Control
  *                   - RFID (MFRC522) admin card registration & check
  *                   - R307 fingerprint enrollment & verification
  *                   - Keypad password entry
  *                   - Solenoid lock control (PB2)
  *                   - Reset button (PB1) to re-run setup
  *                   - Buzzer (PC15) for audible feedback
  *                   - MPU6050 tamper-detect alarm (I2C2)
  **************************
  */
/* USER CODE END Header */

#include "stdio.h"
#include "string.h"
#include "main.h"
#include "i2c-lcd.h"
#include "rc522.h"
#include "R307.h"
#include "mpu6050.h"
#include "ESP8266.h"

/* ── MPU6050 motion / tamper detection tuning ───────────────────────────── */
/* For AFS=8G (4096 LSB/g) and FS=1000 dps (32.8 LSB/dps).
 * Increase if false triggers, decrease if it never trips.                  */
#define MPU_ACCEL_MOVED_THRESHOLD   1200U   /* ~0.29 g */
#define MPU_GYRO_MOVED_THRESHOLD    2000U   /* ~61 dps */
#define MPU_POLL_PERIOD_MS          200U    /* poll rate while idle */
#define TAMPER_ALARM_MS             1500U   /* buzzer alarm duration on tamper */

/* ── Peripheral handles ─────────────────────────────────────────────────── */
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* ── RFID working buffers ───────────────────────────────────────────────── */
uint8_t status;           /* MFRC522 status return */
uint8_t str[MAX_LEN];     /* raw bytes from RFID request/anticoll */
uint8_t sNum[5];          /* scanned card UID */

/* ── Configuration ──────────────────────────────────────────────────────── */
#define PASSWORD_LEN     4
#define MAX_ATTEMPTS     3
#define NUM_FINGERS      1

/* ── GPIO aliases (match your CubeMX labels) ────────────────────────────── */
/* Reset button: PB1, active LOW (internal pull-up) */
#define RESET_BTN_PORT   GPIOB
#define RESET_BTN_PIN    GPIO_PIN_1

/* Solenoid relay: PB2, active HIGH = unlock */
#define SOLENOID_PORT    GPIOB
#define SOLENOID_PIN     GPIO_PIN_2

/* Buzzer: PC15, active HIGH = ON */
#define BUZZER_PORT      GPIOC
#define BUZZER_PIN       GPIO_PIN_15

/* ── Registered credential storage ──────────────────────────────────────── */
uint8_t AUTH_CARD[5]                      = {0};
char    SAVED_PASSWORD[PASSWORD_LEN + 1]  = {0};

/* ── Web mirror for LCD states ─────────────────────────────────────────── */
#define WIFI_WEB_AP_SSID   "LockEn_LCD"
#define WIFI_WEB_AP_PASS   ""
#define WIFI_WEB_PORT      80U

typedef struct
{
  char line[2][17];
  uint8_t ready;
} WifiWebState_t;

static WifiWebState_t wifi_web = {0};

/* ── Private prototypes ─────────────────────────────────────────────────── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART6_UART_Init(void);

/* ── Small helper: |int16| → uint16 ─────────────────────────────────────── */
static uint16_t u16_abs_i16(int16_t v)
{
    return (v < 0) ? (uint16_t)(-v) : (uint16_t)v;
}

static void Wifi_Web_CopyLine(char *dst, const char *src)
{
  memset(dst, ' ', 16);
  dst[16] = '\0';

  if (src == NULL)
  {
    return;
  }

  size_t len = strlen(src);
  if (len > 16) len = 16;
  memcpy(dst, src, len);
}

static void Wifi_Web_SetLine(uint8_t row, const char *text)
{
  if (row > 1)
  {
    return;
  }

  Wifi_Web_CopyLine(wifi_web.line[row], text);
}

static void Wifi_Web_Clear(void)
{
  Wifi_Web_CopyLine(wifi_web.line[0], "");
  Wifi_Web_CopyLine(wifi_web.line[1], "");
}

static void Wifi_Web_BuildPage(char *page, size_t page_size)
{
  snprintf(page, page_size,
       "HTTP/1.1 200 OK\r\n"
       "Content-Type: text/html\r\n"
       "Connection: close\r\n\r\n"
       "<meta http-equiv=\"refresh\" content=\"1\"><pre>"
       "LOCKEN LCD\n"
       "1: %s\n"
       "2: %s\n"
       "</pre>",
       wifi_web.line[0], wifi_web.line[1]);
}

static void Wifi_Web_Task(void)
{
  if (!wifi_web.ready)
  {
    return;
  }

  if (Wifi.RxBuffer[0] == '\0')
  {
    return;
  }

  char *ipd = strstr((char *)Wifi.RxBuffer, "+IPD,");
  if (ipd == NULL)
  {
    return;
  }

  ipd += 5;
  uint8_t link_id = (uint8_t)strtoul(ipd, NULL, 10);

  char page[256];
  Wifi_Web_BuildPage(page, sizeof(page));
  Wifi_TcpIp_SendDataTcp(link_id, (uint16_t)strlen(page), (uint8_t *)page);
  Wifi_TcpIp_Close(link_id);
  Wifi_RxClear();
}

static void Wifi_Web_Init(void)
{
  wifi_web.ready = 0;

  Wifi_Enable();
  HAL_Delay(1000);

  if (Wifi_Init() == false)
  {
    return;
  }

  if (Wifi_SetMode(WifiMode_SoftAp) == false)
  {
    return;
  }

  if (Wifi_SoftAp_Create((char *)WIFI_WEB_AP_SSID,
               (char *)WIFI_WEB_AP_PASS,
               1,
               WifiEncryptionType_Open,
               4,
               false) == false)
  {
    return;
  }

  if (Wifi_TcpIp_SetMultiConnection(true) == false)
  {
    return;
  }

  if (Wifi_TcpIp_SetEnableTcpServer(WIFI_WEB_PORT) == false)
  {
    return;
  }

  Wifi_RxClear();
  wifi_web.ready = 1;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  BUZZER HELPERS
 * ───────────────────────────────────────────────────────────────────────── */
static inline void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

static inline void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

/* Single short tick — used for keypad feedback / card scan confirm */
static void Buzzer_Beep(uint16_t ms)
{
    Buzzer_On();
    HAL_Delay(ms);
    Buzzer_Off();
}

/* Two short beeps — "step OK" (card matched, finger matched) */
static void Buzzer_BeepOK(void)
{
    Buzzer_Beep(80);
    HAL_Delay(80);
    Buzzer_Beep(80);
}

/* Long beep — "access granted" */
static void Buzzer_BeepGranted(void)
{
    Buzzer_Beep(400);
}

/* Three short beeps — "access denied / error" */
static void Buzzer_BeepDenied(void)
{
    for (int i = 0; i < 3; i++)
    {
        Buzzer_Beep(120);
        HAL_Delay(100);
    }
}

/* Pulsing alarm — "tamper detected" */
static void Buzzer_Alarm(uint16_t total_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < total_ms)
    {
        Buzzer_On();
        HAL_Delay(100);
        Buzzer_Off();
        HAL_Delay(100);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  LCD HELPERS
 * ───────────────────────────────────────────────────────────────────────── */
/* Send a string capped/padded to exactly 16 chars so leftovers are cleared */
void lcd_send_string_safe(const char *s)
{
    char buf[17];
    memset(buf, ' ', 16);
    buf[16] = '\0';

    uint8_t len = (uint8_t)strlen(s);
    if (len > 16) len = 16;
    memcpy(buf, s, len);

    lcd_send_string(buf);
}

/* Convenience: write 16-char-safe string at (row, col=0) */
static void lcd_show(uint8_t row, const char *s)
{
    lcd_put_cur(row, 0);
    lcd_send_string_safe(s);
  Wifi_Web_SetLine(row, s);
}

static void lcd_clear_sync(void)
{
  lcd_clear();
  Wifi_Web_Clear();
}

#define lcd_clear lcd_clear_sync

/* Show idle prompt */
static void lcd_show_idle(void)
{
    lcd_clear();
    lcd_show(0, "Scan your card");
    lcd_show(1, "");
}

/* ─────────────────────────────────────────────────────────────────────────
 *  KEYPAD
 * ───────────────────────────────────────────────────────────────────────── */
/* Returns the pressed key character, or 0x01 if none */
char read_keypad(void)
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

/* ── Read a password from keypad ────────────────────────────────────────── */
/* '#' confirms entry, '*' is backspace, digits fill the buffer.            */
/* Each keypress beeps briefly and shows '*' on row 1.                      */
uint8_t get_password(char *entered, uint8_t max_len)
{
    uint8_t idx = 0;
    memset(entered, 0, max_len + 1);

    lcd_show(1, "");

    while (1)
    {
      Wifi_Web_Task();
        char key = read_keypad();

        if (key == 0x01) continue;          /* no key */

        Buzzer_Beep(40);                    /* keypress click */

        if (key == '#')
        {
            if (idx == 0) continue;         /* nothing typed yet */
            entered[idx] = '\0';
            return 1;
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

        HAL_Delay(120);                     /* debounce */
    }
}

/* ─────────────────────────────────────────────────────────────────────────
 *  SOLENOID
 * ───────────────────────────────────────────────────────────────────────── */
void Solenoid_Unlock(void)
{
    HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_SET);
}

void Solenoid_Lock(void)
{
    HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_RESET);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  RESET BUTTON
 * ───────────────────────────────────────────────────────────────────────── */
uint8_t ResetButton_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(RESET_BTN_PORT, RESET_BTN_PIN) == GPIO_PIN_RESET);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  SETUP PHASES
 * ───────────────────────────────────────────────────────────────────────── */
void Setup_RegisterCard(void)
{
    lcd_clear();
    lcd_show(0, "Scan Admin Card");
    lcd_show(1, "to Register...");

    while (1)
    {
      Wifi_Web_Task();
        status = MFRC522_Request(PICC_REQIDL, str);
        if (status != MI_OK) { HAL_Delay(200); continue; }

        status = MFRC522_Anticoll(str);
        if (status != MI_OK) { HAL_Delay(200); continue; }

        memcpy(AUTH_CARD, str, 5);

        Buzzer_BeepOK();

        lcd_clear();
        lcd_show(0, "Card Saved!");

        char uidMsg[17];
        snprintf(uidMsg, sizeof(uidMsg), "%u %u %u %u %u",
                 AUTH_CARD[0], AUTH_CARD[1],
                 AUTH_CARD[2], AUTH_CARD[3], AUTH_CARD[4]);
        lcd_show(1, uidMsg);

        HAL_Delay(2000);
        return;
    }
}

void Setup_EnrollFingerprints(void)
{
    lcd_clear();
    lcd_show(0, "Enrolling FP...");

    for (int i = 1; i <= NUM_FINGERS; i++)
    {
      Wifi_Web_Task();
        char msg[17];
        snprintf(msg, sizeof(msg), "Enroll Finger %d", i);
        lcd_show(0, msg);

        HAL_StatusTypeDef result;
        int retries = 3;

        do {
            result = R307_Enroll(i);
        } while (result != HAL_OK && --retries > 0);

        if (result == HAL_OK)
        {
            lcd_show(0, "Enroll OK!");
            Buzzer_BeepOK();
        }
        else
        {
            lcd_show(0, "Enroll Failed!");
            Buzzer_BeepDenied();
        }
        HAL_Delay(1000);
    }
}

void Setup_RegisterPassword(void)
{
    lcd_clear();
    lcd_show(0, "Set Password:");
    lcd_show(1, "Then press #");
    HAL_Delay(1500);

    lcd_show(0, "Enter Password:");
    get_password(SAVED_PASSWORD, PASSWORD_LEN);

    Buzzer_BeepOK();
    lcd_clear();
    lcd_show(0, "Password Saved!");
    HAL_Delay(1500);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  MPU6050 TAMPER CHECK
 *  Reads accel + gyro and compares to baseline. Returns 1 if motion exceeds
 *  thresholds. Baseline is rolled forward on each call.
 * ───────────────────────────────────────────────────────────────────────── */
static uint8_t MPU_CheckTamper(int16_t accel_prev[3], int16_t gyro_prev[3])
{
    int16_t accel_now[3], gyro_now[3];
    uint8_t moved = 0;

    MPU6050_getAccelValue(&hi2c2, accel_now);
    MPU6050_getGyroValue(&hi2c2, gyro_now);

    for (int i = 0; i < 3; i++)
    {
        if (u16_abs_i16((int16_t)(accel_now[i] - accel_prev[i])) > MPU_ACCEL_MOVED_THRESHOLD) moved = 1;
        if (u16_abs_i16((int16_t)(gyro_now[i]  - gyro_prev[i]))  > MPU_GYRO_MOVED_THRESHOLD)  moved = 1;
        accel_prev[i] = accel_now[i];
        gyro_prev[i]  = gyro_now[i];
    }
    return moved;
}

/* ════════════════════════════════════════════════════════════════════════
 *  MAIN
 * ════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    /* ── Core HAL + clocks + peripherals ─────────────────────────────── */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_USART6_UART_Init();

    /* Make sure outputs are in a safe state */
    Buzzer_Off();
    Solenoid_Lock();

    /* ── RFID module init ────────────────────────────────────────────── */
    MFRC522_Init();
    HAL_Delay(500);

    /* ── I2C1 bus reset (prevents LCD lockup at boot) ────────────────── */
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(100);
    __HAL_RCC_I2C1_RELEASE_RESET();
    HAL_Delay(100);
    MX_I2C1_Init();
    HAL_Delay(100);

    /* ── LCD init ────────────────────────────────────────────────────── */
    lcd_init();
    lcd_clear();

    /* ── ESP8266 web mirror ─────────────────────────────────────────── */
    Wifi_Web_Init();

    /* ── MPU6050 init on I2C2 (non-blocking, just disable tamper if it fails) */
    uint8_t mpu_ok = 0;
    int16_t accel_prev[3] = {0};
    int16_t gyro_prev[3]  = {0};
    uint32_t last_mpu_poll = 0;

    if (MPU6050_Init(&hi2c2, MPU6050_ACCEL_RANGE_8G, MPU6050_GYRO_RANGE_1000) == INIT_SUCCESS)
    {
        MPU6050_getAccelValue(&hi2c2, accel_prev);
        MPU6050_getGyroValue(&hi2c2, gyro_prev);
        mpu_ok = 1;
    }

    /* ── Fingerprint sensor init ─────────────────────────────────────── */
    lcd_show(0, "FP Init...");

    /* On cold boot the R307 sends an unsolicited startup notification into
     * the UART RX buffer. If we transmit a VerifyPassword while that startup
     * packet is still queued, our response stacks behind it and every later
     * command/response pair is misaligned. Drain it first.                */
    {
        uint8_t fp_flush[32];
        HAL_UART_Receive(&huart1, fp_flush, sizeof(fp_flush), 150);
    }

    /* Retry up to 3 times — first attempt may hit the sensor's self-test. */
    uint8_t fp_ok = 0;
    for (int fp_try = 0; fp_try < 3 && !fp_ok; fp_try++)
    {
        if (R307_VerifyPassword(0x00000000) == HAL_OK)
        {
            fp_ok = 1;
        }
        else
        {
            HAL_Delay(300);
            uint8_t fp_retry_flush[32];
            HAL_UART_Receive(&huart1, fp_retry_flush, sizeof(fp_retry_flush), 50);
        }
    }

    if (!fp_ok)
    {
        lcd_show(0, "FP Init FAILED!");
        lcd_show(1, "Check sensor!");
        Buzzer_BeepDenied();
        Error_Handler();
    }

    lcd_show(0, "FP Ready!");
    Buzzer_Beep(80);
    HAL_Delay(1000);

    /* ════════════════════════════════════════════════════════════════════
     *  SETUP PHASE (also re-entered on reset button)
     * ════════════════════════════════════════════════════════════════════ */
SETUP:
    memset(AUTH_CARD, 0, sizeof(AUTH_CARD));
    memset(SAVED_PASSWORD, 0, sizeof(SAVED_PASSWORD));

    R307_ClearAllFingerprints();

    Setup_RegisterCard();
    Setup_EnrollFingerprints();
    Setup_RegisterPassword();

    lcd_clear();
    lcd_show(0, "Setup Done!");
    lcd_show(1, "Scan your card");
    HAL_Delay(2000);

    lcd_show_idle();

    /* Re-baseline MPU after physical setup activity */
    if (mpu_ok)
    {
        MPU6050_getAccelValue(&hi2c2, accel_prev);
        MPU6050_getGyroValue(&hi2c2, gyro_prev);
        last_mpu_poll = HAL_GetTick();
    }

    /* ════════════════════════════════════════════════════════════════════
     *  ACCESS CONTROL LOOP
     * ════════════════════════════════════════════════════════════════════ */
    while (1)
    {
      Wifi_Web_Task();

        /* ── Reset button check ──────────────────────────────────────── */
        if (ResetButton_IsPressed())
        {
            HAL_Delay(50);
            if (ResetButton_IsPressed())
            {
                Solenoid_Lock();
                Buzzer_Beep(200);
                lcd_clear();
                lcd_show(0, "Resetting...");
                lcd_show(1, "Please wait");
                HAL_Delay(1500);

                /* MFRC522_Init() does a full chip software reset, clearing
                 * CommIrqReg. Without it, stale IRQ bits (e.g. TimerIRq) make
                 * MFRC522_ToCard() return MI_NOTAGERR immediately every time. */
                MFRC522_Init();
                HAL_Delay(100);
                goto SETUP;
            }
        }

        /* ── MPU6050 tamper poll while idle ──────────────────────────── */
        if (mpu_ok && (HAL_GetTick() - last_mpu_poll) >= MPU_POLL_PERIOD_MS)
        {
            last_mpu_poll = HAL_GetTick();
            if (MPU_CheckTamper(accel_prev, gyro_prev))
            {
                lcd_clear();
                lcd_show(0, "!! TAMPER !!");
                lcd_show(1, "Device moved");
                Buzzer_Alarm(TAMPER_ALARM_MS);
                lcd_show_idle();
                /* Re-baseline after alarm so we don't loop on residual motion */
                MPU6050_getAccelValue(&hi2c2, accel_prev);
                MPU6050_getGyroValue(&hi2c2, gyro_prev);
            }
        }

        /* ── STEP 1: Scan RFID card ──────────────────────────────────── */
        status = MFRC522_Request(PICC_REQIDL, str);
        if (status != MI_OK) { HAL_Delay(200); continue; }

        status = MFRC522_Anticoll(str);
        if (status != MI_OK) { HAL_Delay(200); continue; }

        memcpy(sNum, str, 5);

        if (memcmp(sNum, AUTH_CARD, 5) != 0)
        {
            /* Unknown card */
            lcd_clear();
            lcd_show(0, "Access Denied!");
            lcd_show(1, "Unknown Card");
            Buzzer_BeepDenied();
            HAL_Delay(2000);
            lcd_show_idle();
            continue;
        }

        /* Card matched */
        Buzzer_BeepOK();
        lcd_clear();
        lcd_show(0, "Card OK!");
        lcd_show(1, "Place finger...");
        HAL_Delay(1000);

        /* ── STEP 2: Fingerprint ─────────────────────────────────────── */
        uint16_t matched_id, score;
        HAL_StatusTypeDef fp_status = R307_Verify(&matched_id, &score);

        if (fp_status != HAL_OK)
        {
            lcd_clear();
            lcd_show(0, "Bad Fingerprint!");
            lcd_show(1, "Access Denied");
            Buzzer_BeepDenied();
            HAL_Delay(2000);
            lcd_show_idle();
            continue;
        }

        Buzzer_BeepOK();
        lcd_clear();
        lcd_show(0, "Finger OK!");
        lcd_show(1, "Enter Password:");
        HAL_Delay(1000);

        /* ── STEP 3: Password ────────────────────────────────────────── */
        uint8_t pass_ok = 0;

        for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++)
        {
            char attempt_msg[17];
            snprintf(attempt_msg, sizeof(attempt_msg), "Pass (%d/%d):", attempt, MAX_ATTEMPTS);
            lcd_show(0, attempt_msg);
            lcd_show(1, "");

            char entered[PASSWORD_LEN + 1];
            get_password(entered, PASSWORD_LEN);

            if (strcmp(entered, SAVED_PASSWORD) == 0)
            {
                pass_ok = 1;
                break;
            }
            else
            {
                lcd_clear();
                lcd_show(0, "Wrong Password!");

                char remain[17];
                int left = MAX_ATTEMPTS - attempt;
                if (left > 0) snprintf(remain, sizeof(remain), "%d tries left", left);
                else          snprintf(remain, sizeof(remain), "Locked out!");
                lcd_show(1, remain);

                Buzzer_BeepDenied();
                HAL_Delay(1500);
            }
        }

        /* ── Final outcome ───────────────────────────────────────────── */
        lcd_clear();

        if (pass_ok)
        {
            lcd_show(0, "Access Granted!");
            lcd_show(1, "Welcome!");
            Buzzer_BeepGranted();

            Solenoid_Unlock();
            HAL_Delay(3000);
            Solenoid_Lock();
        }
        else
        {
            lcd_show(0, "Access Denied!");
            lcd_show(1, "Too many tries");
            Buzzer_Alarm(1500);
            HAL_Delay(1500);
        }

        /* RFID reset between transactions */
        HAL_Delay(100);
        MFRC522_Init();
        HAL_Delay(50);

        memset(str, 0, sizeof(str));
        memset(sNum, 0, sizeof(sNum));

        /* Re-baseline MPU since user just interacted with the device */
        if (mpu_ok)
        {
            MPU6050_getAccelValue(&hi2c2, accel_prev);
            MPU6050_getGyroValue(&hi2c2, gyro_prev);
            last_mpu_poll = HAL_GetTick();
        }

        lcd_show_idle();
    }
}

    void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
    {
      if (huart->Instance == USART2)
      {
        Wifi_RxCallBack();
      }
    }
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_2|R4_Pin|R3_Pin
                          |R2_Pin|R1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : C4_Pin C3_Pin */
  GPIO_InitStruct.Pin = C4_Pin|C3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB2 R4_Pin R3_Pin
                           R2_Pin R1_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|R4_Pin|R3_Pin
                          |R2_Pin|R1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 C2_Pin C1_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_1|C2_Pin|C1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
