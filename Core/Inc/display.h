#pragma once
#include <stdint.h>
#include "cmsis_os.h"

/* Created by Display_Init(); used by PwdTask for direct LCD character echo */
extern osMutexId_t g_lcd_mutex;

/* Must be called once from AuthTask before g_creds_ready is set */
void Display_Init(void);

void Display_Line(uint8_t row, const char *text);
void Display_Both(const char *top, const char *bot);

/* Shows both rows then yields for ms. Mutex released BEFORE the delay. */
void Display_Timed(const char *top, const char *bot, uint32_t ms);

void Display_Idle(void);
void Display_Error(const char *msg);
