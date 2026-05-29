/**
 * @file    i2c-lcd.c
 * @brief   I2C-based 16x2 character LCD driver with HD44780 interface
 *          for user interface display and debugging.
 */

#include "Drivers/i2c-lcd.h"
extern I2C_HandleTypeDef hi2c1;

#define SLAVE_ADDRESS_LCD 0x4E  /* 7-bit: 0x27 */

/**
 * @brief Send a command byte to the LCD via I2C.
 *
 * Sends command in 4-bit mode with proper enable strobing.
 *
 * @param cmd Command byte (e.g., 0x01 to clear display)
 */
void lcd_send_cmd (char cmd)
{
  char data_u, data_l;
	uint8_t data_t[4];
	data_u = (cmd&0xf0);
	data_l = ((cmd<<4)&0xf0);
	data_t[0] = data_u|0x0C;  //en=1, rs=0 -> bxxxx1100
	data_t[1] = data_u|0x08;  //en=0, rs=0 -> bxxxx1000
	data_t[2] = data_l|0x0C;  //en=1, rs=0 -> bxxxx1100
	data_t[3] = data_l|0x08;  //en=0, rs=0 -> bxxxx1000
	HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 100);
}

/**
 * @brief Send a data byte (character) to the LCD via I2C.
 *
 * @param data Character code to display
 */
void lcd_send_data (char data)
{
	char data_u, data_l;
	uint8_t data_t[4];
	data_u = (data&0xf0);
	data_l = ((data<<4)&0xf0);
	data_t[0] = data_u|0x0D;  //en=1, rs=0 -> bxxxx1101
	data_t[1] = data_u|0x09;  //en=0, rs=0 -> bxxxx1001
	data_t[2] = data_l|0x0D;  //en=1, rs=0 -> bxxxx1101
	data_t[3] = data_l|0x09;  //en=0, rs=0 -> bxxxx1001
	HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 100);
}

/**
 * @brief Clear the entire LCD display and reset cursor to home (0,0).
 */
void lcd_clear (void)
{
	lcd_send_cmd (0x01);  /* HD44780 clear display — resets all DDRAM including row 1 @ 0x40 */
	HAL_Delay(2);         /* clear command requires ≥1.52 ms to execute */
}

/**
 * @brief Position the LCD cursor at a specific row and column.
 *
 * @param row Row number (0 or 1)
 * @param col Column number (0-15)
 */
void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0xC0;
            break;
    }

    lcd_send_cmd (col);
}


/**
 * @brief Initialize the LCD in 4-bit mode with 2 lines.
 *
 * Performs HD44780 initialization sequence, enables display, cursor off, blink off.
 */
void lcd_init (void)
{
	// 4 bit initialisation
	HAL_Delay(50);  // wait for >40ms
	lcd_send_cmd (0x30);
	HAL_Delay(5);  // wait for >4.1ms
	lcd_send_cmd (0x30);
	HAL_Delay(1);  // wait for >100us
	lcd_send_cmd (0x30);
	HAL_Delay(10);
	lcd_send_cmd (0x20);  // 4bit mode
	HAL_Delay(10);

  // dislay initialisation
	lcd_send_cmd (0x28); // Function set --> DL=0 (4 bit mode), N = 1 (2 line display) F = 0 (5x8 characters)
	HAL_Delay(1);
	lcd_send_cmd (0x08); //Display on/off control --> D=0,C=0, B=0  ---> display off
	HAL_Delay(1);
	lcd_send_cmd (0x01);  // clear display
	HAL_Delay(2);         // clear requires ≥1.52 ms
	lcd_send_cmd (0x06); //Entry mode set --> I/D = 1 (increment cursor) & S = 0 (no shift)
	HAL_Delay(1);
	lcd_send_cmd (0x0C); //Display on/off control --> D = 1, C and B = 0. (Cursor and blink, last two bits)
}

/**
 * @brief Send a null-terminated string to the LCD.
 *
 * @param str Pointer to string to display
 */
void lcd_send_string (char *str)
{
	while (*str) lcd_send_data (*str++);
}

/**
 * @brief Turn off the LCD backlight (for sleep mode).
 */
void lcd_backlight_off(void)
{
    uint8_t d = 0x00;
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, &d, 1, 100);
}

/**
 * @brief Turn on the LCD backlight (for waking from sleep).
 */
void lcd_backlight_on(void)
{
    uint8_t d = 0x08;
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, &d, 1, 100);
}
