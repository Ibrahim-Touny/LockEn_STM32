#ifndef LCD_LOG_H
#define LCD_LOG_H

#include "i2c-lcd.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Call this instead of FP_LOG
void LCD_Log(const char *fmt, ...);

#endif
