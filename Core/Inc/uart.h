#ifndef INC_UART_H_
#define INC_UART_H_

#include <stm32l4xx_hal.h>

#define TX_BUFFER_SIZE 2048

void printf_uart(UART_HandleTypeDef *, const char *, ...) __attribute__((format(printf, 2, 3)));  // uses format args

#endif /* INC_UART_H_ */
