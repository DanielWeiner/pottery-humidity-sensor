#include "esp8266.h"

#include <stdio.h>
#include <string.h>

ParseStatus esp8266_wait_until_ready(ESP8266State *state, char ch) {
	if (!ch) return PARSE_STATUS_INCOMPLETE;
	if (token_complete(&state->readyToken, ch)) {
		reset_token_state(&state->readyToken);
		esp8266_set_status(state, ESP8266_STATE_READY);
		return PARSE_STATUS_COMPLETE;
	}

	return PARSE_STATUS_INCOMPLETE;
}

void esp8266_transmit_complete(ESP8266State *state) {
	esp8266_set_status(state, ESP8266_STATE_TRANSMITTING_DONE);
}

bool esp8266_can_transmit(ESP8266State *state) {
	return esp8266_has_status(state, ESP8266_STATE_TRANSMITTING_DONE) || esp8266_has_status(state, ESP8266_STATE_READY);
}

void esp8266_set_status(ESP8266State *state, ESP8266Status status) {
	state->status = status;
}

ESP8266Status esp8266_get_status(ESP8266State *state) {
	return state->status;
}

bool esp8266_has_status(ESP8266State *state, ESP8266Status status) {
	return esp8266_get_status(state) == status;
}

bool esp8266_send(ESP8266State *state, char *buf, size_t len) {
	if (!esp8266_can_transmit(state) || len <= 0) return false;	 // Cannot send command if not ready
	esp8266_set_status(state, ESP8266_STATE_TRANSMITTING);

	HAL_UART_Transmit_IT(state->uart, (uint8_t *)buf, len);
	return true;
}

bool esp8266_printf(ESP8266State *state, char *buf, size_t len, const char *format, ...) {
	if (!esp8266_can_transmit(state)) return false;	 // Cannot send command if not ready
	esp8266_set_status(state, ESP8266_STATE_TRANSMITTING);

	va_list args;
	size_t	stringLength;

	va_start(args, format);
	stringLength = vsnprintf(buf, len, format, args);
	va_end(args);

	if (stringLength <= 0 || stringLength > len) {
		esp8266_set_status(state, ESP8266_STATE_TRANSMITTING_DONE);
		return false;
	}

	HAL_UART_Transmit_IT(state->uart, (uint8_t *)buf, stringLength);
	return true;
}

bool esp8266_printf_default(ESP8266State *state, const char *format, ...) {
	static char defaultBuf[ESP8266_TX_DEFAULT_BUFFER_SIZE] = {0};
	if (!esp8266_can_transmit(state)) return false;	 // Cannot send command if not ready
	esp8266_set_status(state, ESP8266_STATE_TRANSMITTING);

	va_list args;
	size_t	stringLength;

	va_start(args, format);
	stringLength = vsnprintf(defaultBuf, ESP8266_TX_DEFAULT_BUFFER_SIZE, format, args);
	va_end(args);

	if (stringLength <= 0 || stringLength > ESP8266_TX_DEFAULT_BUFFER_SIZE) {
		esp8266_set_status(state, ESP8266_STATE_TRANSMITTING_DONE);
		return false;
	}

	HAL_UART_Transmit_IT(state->uart, (uint8_t *)defaultBuf, stringLength);
	return true;
}
