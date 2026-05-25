#pragma once
#include <stdint.h>

/* Write text to one LCD row (safe: truncates/pads to exactly 16 chars) */
void Display_Line(uint8_t row, const char *text);

/* Write both rows at once */
void Display_Both(const char *top, const char *bot);

/* Write both rows then osDelay(ms). Pass ms=0 to display without waiting */
void Display_Timed(const char *top, const char *bot, uint32_t ms);

/* Show idle prompt: "Scan your card" on row 0, blank row 1 */
void Display_Idle(void);

/* Show "** ERROR **" on row 0 and msg on row 1 */
void Display_Error(const char *msg);
