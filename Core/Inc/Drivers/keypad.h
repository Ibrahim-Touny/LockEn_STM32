/*
 * ============================================================================
 * keypad.h
 * ============================================================================
 * Driver for 4x4 matrix keypad (16 buttons arranged in rows and columns)
 * Implements row-by-row scanning to detect key presses with debouncing
 * ============================================================================
 */

#pragma once
#include "stm32f4xx_hal.h"
#include "System/main.h"

/*
 * Read a single keypress from the 4x4 matrix keypad
 *
 * Scans each row in sequence, checking all column inputs for a press.
 * When a key is detected, waits until it is released (debouncing).
 *
 * Returns:
 *   Character of pressed key: '0'-'9', '*', '#', 'A'-'D'
 *   0: No key currently pressed
 *
 * Key layout:
 *   1 2 3 A
 *   4 5 6 B
 *   7 8 9 C
 *   * 0 # D
 */
static inline char Keypad_Read(void)
{
    /* Activate Row 1: bring R1 low, others high */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);

    /* Check each column in Row 1 for a press (active LOW) */
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '1'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '4'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '7'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '*'; }

    /* Activate Row 2 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);

    /* Check each column in Row 2 */
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '2'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '5'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '8'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '0'; }

    /* Activate Row 3 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_SET);

    /* Check each column in Row 3 */
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return '3'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return '6'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return '9'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return '#'; }

    /* Activate Row 4 */
    HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R3_GPIO_Port, R3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(R4_GPIO_Port, R4_Pin, GPIO_PIN_RESET);

    /* Check each column in Row 4 */
    if (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)) { while (!HAL_GPIO_ReadPin(C1_GPIO_Port, C1_Pin)); return 'A'; }
    if (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)) { while (!HAL_GPIO_ReadPin(C2_GPIO_Port, C2_Pin)); return 'B'; }
    if (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)) { while (!HAL_GPIO_ReadPin(C3_GPIO_Port, C3_Pin)); return 'C'; }
    if (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)) { while (!HAL_GPIO_ReadPin(C4_GPIO_Port, C4_Pin)); return 'D'; }

    return 0;
}
