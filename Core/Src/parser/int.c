#include "parser/int.h"
#include "digits.h"

void reset_int8_parse_state(Int8ParseState *state) {
	state->sign = 1;
	state->value = 0;
	state->hasDigits = false;
}

void reset_uint8_parse_state(Uint8ParseState *state) {
	state->value = 0;
	state->hasDigits = false;
}

ParseStatus parse_int8_t(Int8ParseState *state, int8_t *dest, char ch, char endChar) {
	if (state->hasDigits && ch == endChar) {
		*dest = state->value;
		reset_int8_parse_state(state);
		return PARSE_STATUS_COMPLETE;
	}
	if (!state->hasDigits && state->sign == 1 && ch == '-') {
		state->sign = -1;
		return PARSE_STATUS_INCOMPLETE;
	}
	if (!add_digit_if_valid_int8_t(ch, &state->value, state->sign)) {
		reset_int8_parse_state(state);
		return PARSE_STATUS_INVALID;
	}
	state->hasDigits = true;
	return PARSE_STATUS_INCOMPLETE;
}

ParseStatus parse_uint8_t(Uint8ParseState *state, uint8_t *dest, char ch, char endChar) {
	if (state->hasDigits && ch == endChar) {
		*dest = state->value;
		reset_uint8_parse_state(state);
		return PARSE_STATUS_COMPLETE;
	}
	if (!add_digit_if_valid_uint8_t(ch, &state->value)) {
		reset_uint8_parse_state(state);
		return PARSE_STATUS_INVALID;
	}
	state->hasDigits = true;
	return PARSE_STATUS_INCOMPLETE;
}