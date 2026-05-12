/* r307.h
 * Header for R307 fingerprint driver by controllerstech.com
 */

#ifndef __R307_H__
#define __R307_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include "i2c-lcd.h"

/* ------------------------ USER CONFIG ------------------------
 * If your UART handle is named something other than 'huart1',
 * define R307_UART_HANDLE to that handle before including this header:
 *
 *   #define R307_UART_HANDLE huart1
 *   #include "r307.h"
 *
 * Otherwise this header will extern 'huart1' and use it.
 * ----------------------------------------------------------- */
#define R307_UART huart1
extern UART_HandleTypeDef R307_UART;

#define USE_DEBUG_LOG 	0  // Debug all the steps
#define USE_MAIN_LOG	1  // only the user intractable Logs

/* Logging: override FP_LOG before include to route logs elsewhere */
#if USE_MAIN_LOG
#define FP_LOG(fmt, ...)  do {                     \
        char lcd_buf[17];                              \
        snprintf(lcd_buf, sizeof(lcd_buf), fmt, ##__VA_ARGS__); \
        lcd_put_cur(1, 0);                             \
        lcd_send_string("                ");           \
        lcd_put_cur(1, 0);                             \
        lcd_send_string(lcd_buf);                      \
    } while(0)
#else
    #define FP_LOG(fmt, ...)  ((void)0)
#endif

#if USE_DEBUG_LOG
    #define DB_LOG(fmt, ...)  do {                     \
        char lcd_buf[17];                              \
        snprintf(lcd_buf, sizeof(lcd_buf), fmt, ##__VA_ARGS__); \
        lcd_put_cur(1, 0);                             \
        lcd_send_string("                ");           \
        lcd_put_cur(1, 0);                             \
        lcd_send_string(lcd_buf);                      \
    } while(0)
#else
#define DB_LOG(fmt, ...)  ((void)0)
#endif

/* Recommended max response buffer size used in implementation */
#define R307_MAX_RESP_LEN   32

/* Instruction codes (used by implementation) */
#define R307_INS_GENIMG     0x01  /* GenImg: capture image */
#define R307_INS_IMG2TZ     0x02  /* Img2Tz: image -> char (buffer) */
#define R307_INS_REGMODEL   0x05  /* RegModel: merge buffers -> template */
#define R307_INS_STORE      0x06  /* Store template to flash */
#define R307_INS_SEARCH     0x04  /* Search database */
#define R307_INS_DELETE     0x0C  /* Delete templates */
#define R307_INS_EMPTY      0x0D  /* Empty library */
#define R307_INS_VERIFYPWD  0x13  /* Verify password */

/* Confirmation codes (common) */
#define R307_CONFIRM_OK         0x00
#define R307_CONFIRM_NOFINGER   0x02
#define R307_CONFIRM_NOMATCH    0x09

/* ------------------------ API (match r307.c) ------------------------ */

/* Enroll fingerprint (captures twice, creates model and stores at page_id). */
HAL_StatusTypeDef R307_Enroll(uint16_t page_id);

/* Verify fingerprint (capture -> convert -> search over library range 0..1000).
 * out_page_id / out_score are optional outputs.
 *
 Fingerprint Match Score Reference (R307):

 Score Range        Meaning
 -----------        -------
 0 - 50             Poor match, likely not the same finger
 51 - 100           Weak match, possible false match
 101 - 150          Acceptable match for casual use
 151 - 200          Good match, very likely the same finger
 >200               Excellent match, highly reliable

 Default threshold for considering a fingerprint as matched: 150
 */
HAL_StatusTypeDef R307_Verify(uint16_t *out_page_id, uint16_t *out_score);

/* Verify device password. Pass the 32-bit password (module default is often 0x00000000 or 0x00000001). */
HAL_StatusTypeDef R307_VerifyPassword(uint32_t password);

// Clear all templates (empty library)
HAL_StatusTypeDef R307_ClearAllFingerprints(void);

// Delete Fingerprints starting from -> from_ID, number of FP to be deleted -> num_fp
HAL_StatusTypeDef R307_DeleteFingerprints(uint16_t from_ID, uint16_t num_fp);

#ifdef __cplusplus
}
#endif

#endif /* __R307_H__ */
