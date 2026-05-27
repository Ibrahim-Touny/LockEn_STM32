#pragma once
#include "FreeRTOS.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdint.h>

typedef struct {
    uint32_t ts;
    char     event[12];   /* "GRANTED", "TIMEOUT", "TAMPER" */
    char     factors[12]; /* "RFID+FP", "RFID+PWD", ""      */
} EspLogEntry_t;

/* Created early in EspTask — safe to post from any task once non-NULL */
extern QueueHandle_t        g_esp_log_queue;

/* LCD mirror — written by display.c under g_lcd_mutex, read by EspTask */
extern volatile char    g_lcd_line0[17];
extern volatile char    g_lcd_line1[17];
extern volatile uint8_t g_lcd_dirty;

/* Set by AuthTask during setup/reset — EspTask sends enroll_reset to server */
extern volatile uint8_t g_face_enroll_requested;
/* Set by EspTask when server confirms enrollment is done */
extern volatile uint8_t g_face_enrolled;

extern const osThreadAttr_t espTask_attr;
void EspTask(void *argument);
/* Called by PwdTask when 'D' is pressed — opens a 15-second face scan window */
void EspTask_RequestFaceScan(void);
