#include "parser/ipd.h"

#include "digits.h"

void reset_ipd_parse_state(IPDParseState *state) {
	state->phase = PARSE_IPD_PREFIX;
	state->ipdBytesTmp = 0;
	state->ipdBytes = 0;  // Reset the number of bytes remaining in the current IPD packet
	reset_token_state(&state->ipdToken);
}

static ParseStatus parse_ipd_prefix(IPDParseState *state, char ch) {
	ParseStatus tokenStatus = parse_token_or_reset(&state->ipdToken, ch);
	if (parse_status_complete(tokenStatus)) {
		state->ipdBytes = 0;				   // Reset bytes for new IPD data
		state->ipdBytesTmp = 0;				   // Reset temporary bytes for new IPD data
		state->phase = PARSE_IPD_PREFIX_DONE;  // Signal that the prefix has been parsed
		return PARSE_STATUS_INCOMPLETE;		   // Continue to parsing digits
	}

	return tokenStatus;
}

static ParseStatus parse_ipd_digits(IPDParseState *state, char ch) {
	if (ch == ':') {
		if (state->phase == PARSE_IPD_PREFIX_DONE) {
			// ':' cannot come immediately after prefix
			return PARSE_STATUS_INVALID;
		}
		if (state->ipdBytesTmp == 0) return PARSE_STATUS_COMPLETE;	// No bytes to process, return complete status

		state->ipdBytes = state->ipdBytesTmp;  // Set the number of bytes from the temporary count
		state->phase = PARSE_IPD_DIGITS_DONE;  // Signal that digits have been parsed

		return PARSE_STATUS_INCOMPLETE;	 // Continue to parse the packet
	}

	// Ensure we are in the digit parsing phase, it might be PARSE_IPD_PREFIX_DONE
	state->phase = PARSE_IPD_DIGITS;

	if (add_digit_if_valid_size_t(ch, &state->ipdBytesTmp)) {
		return PARSE_STATUS_INCOMPLETE;	 // Valid digit, proceed
	}
	state->ipdBytesTmp = 0;		  // invalid digit, reset
	return PARSE_STATUS_INVALID;  // signal invalid input
}

static ParseStatus parse_ipd_packet(IPDParseState *state, char ch) {
	// Ensure we are in the packet parsing phase, it might be PARSE_IPD_DIGITS_DONE
	state->phase = PARSE_IPD_PACKET;

	if (state->ipdBytes == 0 || --state->ipdBytes == 0) {
		state->phase = PARSE_IPD_PACKET_DONE;  // Signal that the packet has been fully parsed
		return PARSE_STATUS_COMPLETE;		   // No more bytes to process
	}

	return PARSE_STATUS_INCOMPLETE;	 // Still processing IPD bytes
}

ParseStatus parse_ipd(IPDParseState *state, char ch) {
	ParseStatus status;
	switch (state->phase) {
		case PARSE_IPD_PACKET_DONE:
			reset_ipd_parse_state(state);  // Reset state after processing
										   // fall through to parse another prefix
		case PARSE_IPD_PREFIX:
			status = parse_ipd_prefix(state, ch);
			break;
		case PARSE_IPD_PREFIX_DONE:
		case PARSE_IPD_DIGITS:
			status = parse_ipd_digits(state, ch);
			break;
		case PARSE_IPD_DIGITS_DONE:
		case PARSE_IPD_PACKET:
			status = parse_ipd_packet(state, ch);
			break;
		default:
			status = PARSE_STATUS_INVALID;	// Invalid phase, should not happen
	}

	if (parse_status_invalid(status)) {
		reset_ipd_parse_state(state);
		return parse_ipd_prefix(state, ch);	 // Retry parsing the prefix
	}

	return status;	// Return the current parse status
}
