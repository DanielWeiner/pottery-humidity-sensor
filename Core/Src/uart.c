#include "uart.h"

#include <stdarg.h>
#include <stdio.h>

void printf_uart(UART_HandleTypeDef *uart, const char *format, ...) {
	va_list args;
	char	buf[TX_BUFFER_SIZE] = {0};
	size_t	stringLength;

	va_start(args, format);
	stringLength = vsnprintf(buf, TX_BUFFER_SIZE, format, args);
	va_end(args);

	if (stringLength < 0 || stringLength > TX_BUFFER_SIZE) return;

	while (HAL_UART_Transmit(uart, (uint8_t *)buf, stringLength, HAL_MAX_DELAY) == HAL_BUSY) {
		HAL_Delay(1);
	}
}
