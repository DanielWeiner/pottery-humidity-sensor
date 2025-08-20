#ifndef INC_PARSER_IPD_H_
#define INC_PARSER_IPD_H_

#include <stdbool.h>
#include <stdint.h>

#include "parser/parse.h"
#include "token.h"

#define DECLARE_IPD_TOKEN() DECLARE_TOKEN(IPD, "+IPD,");
#define IPD_PARSE_STATE() \
	{.phase = PARSE_IPD_PREFIX, .ipdToken = TOKEN_PARSE_STATE(IPD), .ipdBytesTmp = 0, .ipdBytes = 0}

typedef enum IPDParseStatePhase {
	PARSE_IPD_PREFIX,
	PARSE_IPD_PREFIX_DONE,
	PARSE_IPD_DIGITS,
	PARSE_IPD_DIGITS_DONE,
	PARSE_IPD_PACKET,
	PARSE_IPD_PACKET_DONE
} IPDParseStatePhase;

typedef struct IPDParseState {
	IPDParseStatePhase phase;
	TokenParseState	   ipdToken;
	size_t			   ipdBytesTmp;
	size_t			   ipdBytes;  // Number of bytes remaining in the current IPD packet
} IPDParseState;

void		reset_ipd_parse_state(IPDParseState *state);
ParseStatus parse_ipd(IPDParseState *, char);

static inline bool is_ipd_packet(IPDParseState *state) {
	return state->phase == PARSE_IPD_PACKET || state->phase == PARSE_IPD_PACKET_DONE;
}
static inline bool is_ipd_packet_done(IPDParseState *state) {
	return state->phase == PARSE_IPD_PACKET_DONE;
}

static inline bool is_ipd_size_done(IPDParseState *state) {
	return state->phase == PARSE_IPD_DIGITS_DONE;
}

#endif /* INC_PARSER_IPD_H_ */
