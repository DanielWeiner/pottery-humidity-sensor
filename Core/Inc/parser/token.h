#ifndef INC_PARSER_TOKEN_H_
#define INC_PARSER_TOKEN_H_

#include <stdbool.h>
#include <stddef.h>

#include "constants.h"
#include "parser/parse.h"

#define DECLARE_TOKEN(name, tok) \
	static const Token name = {.str = (tok), .length = sizeof(tok) - 1, .caseInsensitive = false}
#define DECLARE_TOKEN_CI(name, tok) \
	static const Token name = {.str = (tok), .length = sizeof(tok) - 1, .caseInsensitive = true}

#define TOKEN_PARSE_STATE(tk) {.index = 0, .token = &(tk)}

#define set_state_on_token(destState, tokenState, newState, ch) \
	set_state_on_token_int((int *)(destState), (tokenState), (int)(newState), (ch))

typedef struct Token {
	const size_t	  length;
	const char *const str;
	const bool		  caseInsensitive;
} Token;

typedef struct TokenParseState {
	size_t			   index;
	const Token *const token;
} TokenParseState;

DECLARE_TOKEN(CRLF_TOKEN, CRLF);
DECLARE_TOKEN(DOUBLE_CRLF_TOKEN, DOUBLE_CRLF);

void reset_token_state(TokenParseState *state);

ParseStatus parse_token_or_reset(TokenParseState *, char);
ParseStatus parse_token(TokenParseState *, char);
ParseStatus parse_token_aligned_or_reset(TokenParseState *, char, size_t);
ParseStatus set_state_on_token_int(int *, TokenParseState *, int, char);

static inline bool token_complete(TokenParseState *state, char ch) {
	return parse_status_complete(parse_token_or_reset(state, ch));
}

#endif /* INC_PARSER_TOKEN_H_ */
