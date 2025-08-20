#ifndef INC_LOG_H_
#define INC_LOG_H_

#include "uart.h"

extern UART_HandleTypeDef huart2;

#define LOG(...) printf_uart(&huart2, __VA_ARGS__)

#endif /* INC_LOG_H_ */
