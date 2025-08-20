#ifndef INC_ESP8266_H_
#define INC_ESP8266_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stm32l4xx.h>

#include "parser/parse.h"
#include "parser/token.h"

#define ESP8266_TX_DEFAULT_BUFFER_SIZE 256
#define ESP8266_TOKEN_OK "OK"
#define ESP8266_TOKEN_ERROR "ERROR"
#define ESP8266_TOKEN_READY "ready"
#define ESP8266_TOKEN_CLOSED "CLOSED"
#define ESP8266_TOKEN_WIFI_DISCONNECT "WIFI DISCONNECT"

#define esp8266_send_message(state, buf, len, fmt, ...) esp8266_printf(state, buf, len, (fmt CRLF), ##__VA_ARGS__)
#define esp8266_send_message_default(state, fmt, ...) esp8266_printf_default(state, (fmt CRLF), ##__VA_ARGS__)

typedef enum ESP8266Status {
	ESP8266_STATE_STARTING,
	ESP8266_STATE_READY,
	ESP8266_STATE_TRANSMITTING,
	ESP8266_STATE_TRANSMITTING_DONE
} ESP8266Status;

typedef struct ESP8266State {
	ESP8266Status		status;
	TokenParseState		readyToken;
	UART_HandleTypeDef *uart;
} ESP8266State;

ParseStatus	  esp8266_wait_until_ready(ESP8266State *, char);
void		  esp8266_transmit_complete(ESP8266State *);
bool		  esp8266_can_transmit(ESP8266State *);
void		  esp8266_set_status(ESP8266State *state, ESP8266Status status);
ESP8266Status esp8266_get_status(ESP8266State *state);
bool		  esp8266_has_status(ESP8266State *state, ESP8266Status status);
bool		  esp8266_send(ESP8266State *, char *, size_t);
bool		  esp8266_printf(ESP8266State *, char *, size_t, const char *, ...)
	__attribute__((format(printf, 4, 5)));	// uses format args
bool esp8266_printf_default(ESP8266State *, const char *, ...)
	__attribute__((format(printf, 2, 3)));	// uses format args

#endif /* INC_ESP8266_H_ */
