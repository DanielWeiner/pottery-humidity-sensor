#include "parser/http_response.h"

#include "digits.h"

void reset_http_processing_state(HttpResponseParseState *processingState) {
	processingState->phase = PROCESSING_START_LINE;
	processingState->lineIndex = 0;
	processingState->statusCodeDone = false;
	processingState->bodyBytesRemaining = 0;
	processingState->tempStatusCode = 0;

	reset_token_state(&processingState->crlfToken);
	reset_token_state(&processingState->doubleCrlfToken);
	reset_token_state(&processingState->httpV1_1StartLineToken);

	reset_content_length_state(&processingState->contentLengthState);
}

void reset_response(HttpResponse *response) {
	response->bodyDone = false;
	response->contentLength = 0;
	response->enableStreaming = false;
	response->headersDone = false;
	response->status = 0;

	reset_http_processing_state(&response->_parseState);
}

void reset_content_length_state(ContentLengthParseState *state) {
	state->hasDigits = false;
	state->digitsDone = false;
	state->contentLength = 0;
	state->headerMatched = false;

	reset_token_state(&state->contentLengthToken);
	reset_token_state(&state->crlfToken);
}

ParseStatus parse_content_length_or_reset(ContentLengthParseState *state, char ch, size_t *dest, size_t lineIndex) {
	if (lineIndex == 0) reset_content_length_state(state);
	ParseStatus status = parse_content_length(state, ch, lineIndex);
	switch (status) {
		case PARSE_STATUS_COMPLETE:
			*dest = state->contentLength;
		case PARSE_STATUS_INVALID:
			reset_content_length_state(state);
		default:
			return status;
	}
}

ParseStatus parse_content_length(ContentLengthParseState *state, char ch, size_t lineIndex) {
	if (!state->headerMatched) {
		ParseStatus headerState = parse_token_aligned_or_reset(&state->contentLengthToken, ch, lineIndex);
		if (parse_status_complete(headerState)) {
			state->headerMatched = true;
			return PARSE_STATUS_INCOMPLETE;
		}
		return headerState;
	}

	if (!state->hasDigits) {
		if (ch == ' ') return PARSE_STATUS_INCOMPLETE;	// any number of spaces after "Content-Length:" is valid
		if (!add_digit_if_valid_size_t(ch, &state->contentLength))
			return PARSE_STATUS_INVALID;  // but only valid digits otherwise
		state->hasDigits = true;
		return PARSE_STATUS_INCOMPLETE;
	}

	if (!state->digitsDone && add_digit_if_valid_size_t(ch, &state->contentLength)) return PARSE_STATUS_INCOMPLETE;

	ParseStatus crlfState = parse_token_or_reset(&state->crlfToken, ch);
	if (parse_status_incomplete(crlfState)) state->digitsDone = true;
	return crlfState;
}

ParseStatus parse_http_response_or_reset(HttpResponse *response, char ch) {
	ParseStatus status = parse_http_response(response, ch);

	if (parse_status_invalid(status)) {
		reset_response(response);
		status = parse_http_response(response, ch);	 // if parsing failed, try parsing the start of a response
	}

	if (parse_status_complete(status)) {
		response->_parseState.phase = PROCESSING_RESPONSE_RESET;  // reset the state for the next response
	} else if (parse_status_invalid(status)) {
		reset_response(response);  // reset the whole thing if invalid
	}

	return status;
}

__attribute__((weak)) void http_on_header_byte(HttpResponse *, char) {}
__attribute__((weak)) void http_on_body_byte(HttpResponse *, char) {}

ParseStatus parse_http_response(HttpResponse *response, char ch) {
	HttpResponseParseState *state = &response->_parseState;

	switch (state->phase) {
		case PROCESSING_RESPONSE_RESET:
			reset_response(response);
			// fall through to start processing the response
		case PROCESSING_START_LINE:

			ParseStatus startLineStatus = parse_token_or_reset(&state->httpV1_1StartLineToken, ch);
			http_on_header_byte(response, ch);

			if (parse_status_complete(startLineStatus)) state->phase = PROCESSING_STATUS_CODE;
			else if (parse_status_invalid(startLineStatus)) return PARSE_STATUS_INVALID;

			return PARSE_STATUS_INCOMPLETE;
		case PROCESSING_STATUS_CODE:
			http_on_header_byte(response, ch);

			if (!state->statusCodeDone && add_digit_if_valid_uint16_t(ch, &state->tempStatusCode))
				return PARSE_STATUS_INCOMPLETE;
			if (!state->tempStatusCode) return PARSE_STATUS_INVALID;
			state->statusCodeDone = true;
			if (!token_complete(&state->crlfToken, ch)) return PARSE_STATUS_INCOMPLETE;

			// Status line is done, move on to header
			state->phase = PROCESSING_HEADERS;
			response->status = state->tempStatusCode;

			return PARSE_STATUS_INCOMPLETE;
		case PROCESSING_HEADERS:
			parse_content_length_or_reset(&state->contentLengthState, ch, &response->contentLength, state->lineIndex++);
			if (token_complete(&state->crlfToken, ch)) state->lineIndex = 0;
			response->headersDone = token_complete(&state->doubleCrlfToken, ch);

			// call after setting headersDone so that the consumer knows it's done on the final byte
			http_on_header_byte(response, ch);

			if (!response->headersDone) return PARSE_STATUS_INCOMPLETE;

			// Double CRLF has been detected, move on to body
			state->bodyBytesRemaining = response->contentLength;
			if (!state->bodyBytesRemaining && !response->enableStreaming) {
				response->bodyDone = true;
				return PARSE_STATUS_COMPLETE;
			}
			state->phase = PROCESSING_BODY;

			return PARSE_STATUS_INCOMPLETE;
		case PROCESSING_BODY:
			// no checks on the content of the body, just keep decrementing bodyBytesRemaining until done
			if (state->bodyBytesRemaining) state->bodyBytesRemaining--;
			// the response isn't over if this is a streaming response without content-length
			if (!state->bodyBytesRemaining && !response->enableStreaming) response->bodyDone = true;

			// call after setting bodyDone so that the consumer knows it's done on the final byte
			http_on_body_byte(response, ch);

			return response->bodyDone ? PARSE_STATUS_COMPLETE : PARSE_STATUS_INCOMPLETE;
		default:
			return PARSE_STATUS_INVALID;
	}
}
