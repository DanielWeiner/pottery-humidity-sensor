#include "parser/token.h"

#include <stdarg.h>

#include "chars.h"

void reset_token_state(TokenParseState *state) {
	state->index = 0;
}

ParseStatus parse_token_or_reset(TokenParseState *state, char ch) {
	return parse_token_aligned_or_reset(state, ch, state->index);
}

ParseStatus parse_token(TokenParseState *state, char ch) {
	if (state->token->caseInsensitive) {
		if (lower(ch) != lower(state->token->str[state->index++])) return PARSE_STATUS_INVALID;
	} else {
		if (ch != state->token->str[state->index++]) return PARSE_STATUS_INVALID;
	}
	if (state->index == state->token->length) return PARSE_STATUS_COMPLETE;

	return PARSE_STATUS_INCOMPLETE;
}

ParseStatus parse_token_aligned_or_reset(TokenParseState *state, char ch, size_t alignment) {
	if (alignment != state->index) reset_token_state(state);  // if misaligned, reset the state and test alignment 0

	size_t		tokenIndex = state->index;
	ParseStatus status = parse_token(state, ch);

	if (parse_status_invalid(status)) {
		reset_token_state(state);
		if (tokenIndex) status = parse_token(state, ch);  // if not at token start, try matching token start
	}

	if (!parse_status_incomplete(status)) reset_token_state(state);

	return status;
}

ParseStatus set_state_on_token_int(int *destState, TokenParseState *tokenState, int state, char ch) {
	ParseStatus status = parse_token_or_reset(tokenState, ch);
	if (parse_status_complete(status)) {
		*destState = state;
	}
	return status;
}
