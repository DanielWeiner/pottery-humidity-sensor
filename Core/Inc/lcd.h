#ifndef INC_LCD_H_
#define INC_LCD_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "i2c_lcd.h"

#define LCD_WIDTH 16
#define LCD_BUFFER_SIZE (LCD_WIDTH + 1)	 // 16 bytes + '\0'
#define LCD_ADDRESS (0x27 << 1)

void printf_lcd(I2C_LCD_HandleTypeDef *, int, const char *, ...)
	__attribute__((format(printf, 3, 4)));	// uses format args

#endif /* INC_LCD_H_ */
