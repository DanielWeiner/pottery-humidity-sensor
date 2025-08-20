#ifndef INC_PARSER_INT_H_
#define INC_PARSER_INT_H_

#include <stdbool.h>
#include <stdint.h>

#include "parser/parse.h"

typedef struct Int8ParseState {
	bool   hasDigits;
	int8_t sign;
	int8_t value;
} Int8ParseState;

typedef struct Uint8ParseState {
	uint8_t value;
	bool	hasDigits;
} Uint8ParseState;

void		reset_int8_parse_state(Int8ParseState *);
void		reset_uint8_parse_state(Uint8ParseState *);
ParseStatus parse_int8_t(Int8ParseState *, int8_t *, char, char);
ParseStatus parse_uint8_t(Uint8ParseState *, uint8_t *, char, char);

#endif /* INC_PARSER_INT_H_ */
