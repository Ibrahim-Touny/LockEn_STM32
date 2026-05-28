#pragma once
#include "stm32f4xx_hal.h"
#include "main.h"

/* Returns the pressed key character, or 0 if no key is currently held.
 * Spins until the key is released before returning (hardware debounce). */
static inline char Keypad_Read(void)
{
    /* Row 1 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '1'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '4'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '7'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '*'; }
    /* Row 2 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '2'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '5'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '8'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '0'; }
    /* Row 3 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '3'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '6'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '9'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '#'; }
    /* Row 4 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_RESET);
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return 'A'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return 'B'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return 'C'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'D'; }
    return 0;
}
