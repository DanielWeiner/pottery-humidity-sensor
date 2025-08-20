#include "lcd.h"

void printf_lcd(I2C_LCD_HandleTypeDef *lcd, int row, const char *format, ...) {
	va_list args;
	char	lcdBuffer[LCD_BUFFER_SIZE] = {0};
	va_start(args, format);
	int len = vsnprintf(lcdBuffer, sizeof lcdBuffer, format, args);
	va_end(args);
	// pad with spaces if the string is less than 16 chars
	if (len < LCD_WIDTH) {
		memset(lcdBuffer + len, ' ', LCD_WIDTH - len);
		lcdBuffer[LCD_WIDTH] = '\0';
	}
	lcd_gotoxy(lcd, 0, row);
	lcd_puts(lcd, lcdBuffer);
}
