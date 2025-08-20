#ifndef INC_PARSER_HTTP_RESPONSE_H_
#define INC_PARSER_HTTP_RESPONSE_H_

#include <stdbool.h>
#include <stdint.h>

#include "parser/parse.h"
#include "parser/token.h"

DECLARE_TOKEN_CI(HTTP_HEADER_CONTENT_LENGTH, "Content-Length:");
DECLARE_TOKEN(HTTP_START_LINE_V1_1, "HTTP/1.1 ");

#define CONTENT_LENGTH_PARSE_STATE()                                      \
	{.hasDigits = false,                                                  \
	 .digitsDone = false,                                                 \
	 .headerMatched = false,                                              \
	 .contentLength = 0,                                                  \
	 .contentLengthToken = TOKEN_PARSE_STATE(HTTP_HEADER_CONTENT_LENGTH), \
	 .crlfToken = TOKEN_PARSE_STATE(CRLF_TOKEN)}

#define HTTP_PARSE_STATE()                                              \
	{.httpV1_1StartLineToken = TOKEN_PARSE_STATE(HTTP_START_LINE_V1_1), \
	 .crlfToken = TOKEN_PARSE_STATE(CRLF_TOKEN),                        \
	 .doubleCrlfToken = TOKEN_PARSE_STATE(DOUBLE_CRLF_TOKEN),           \
	 .statusCodeDone = false,                                           \
	 .phase = PROCESSING_START_LINE,                                    \
	 .lineIndex = 0,                                                    \
	 .tempStatusCode = 0,                                               \
	 .bodyBytesRemaining = 0,                                           \
	 .contentLengthState = CONTENT_LENGTH_PARSE_STATE()}

#define HTTP_RESPONSE()        \
	{.status = 0,              \
	 .contentLength = 0,       \
	 .bodyDone = false,        \
	 .headersDone = false,     \
	 .enableStreaming = false, \
	 ._parseState = HTTP_PARSE_STATE()}

typedef enum ResponseProcessingPhase {
	PROCESSING_START_LINE,
	PROCESSING_STATUS_CODE,
	PROCESSING_HEADERS,
	PROCESSING_BODY,
	PROCESSING_RESPONSE_RESET
} ResponseProcessingPhase;

typedef struct ContentLengthParseState {
	bool			hasDigits;
	bool			digitsDone;
	bool			headerMatched;
	size_t			contentLength;
	TokenParseState contentLengthToken;
	TokenParseState crlfToken;
} ContentLengthParseState;

typedef struct HttpResponseParseState {
	bool					statusCodeDone;
	ResponseProcessingPhase phase;
	uint16_t				lineIndex;
	uint16_t				tempStatusCode;
	size_t					bodyBytesRemaining;
	ContentLengthParseState contentLengthState;
	TokenParseState			doubleCrlfToken;
	TokenParseState			crlfToken;
	TokenParseState			httpV1_1StartLineToken;
} HttpResponseParseState;

typedef struct HttpResponse {
	uint16_t			   status;
	size_t				   contentLength;
	bool				   bodyDone;
	bool				   headersDone;
	bool				   enableStreaming;
	HttpResponseParseState _parseState;
} HttpResponse;

void reset_content_length_state(ContentLengthParseState *);
void reset_http_processing_state(HttpResponseParseState *);
void reset_response(HttpResponse *);

void http_on_header_byte(HttpResponse *, char);
void http_on_body_byte(HttpResponse *, char);

ParseStatus parse_http_response(HttpResponse *, char);
ParseStatus parse_http_response_or_reset(HttpResponse *, char);
ParseStatus parse_content_length(ContentLengthParseState *, char, size_t);

#endif /* INC_PARSER_HTTP_RESPONSE_H_ */
