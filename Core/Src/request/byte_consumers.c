#include "request/byte_consumers.h"

#include "log.h"
#include "parser/token.h"

typedef struct IpdResponse {
	IPDParseState *ipdState;
	HttpResponse  *response;
} IpdResponse;

static int process_incoming_ipd(int, char, uint32_t, void *);
static int process_incoming_ipd_packet(int, char, uint32_t, void *);

static TokenParseState *okToken;
static TokenParseState *errorToken;
static TokenParseState *caretToken;
static TokenParseState *closedToken;
static IpdResponse		ipdResponse;

void http_on_header_byte(HttpResponse *response, char ch) {
	if (response->headersDone) {
		LOG("Status code: %d" CRLF, response->status);
		LOG("Content length: %u" CRLF, response->contentLength);
	}
}

void http_on_body_byte(HttpResponse *response, char ch) {
	if (response->bodyDone) {
		LOG("Response done" CRLF);
	}
}

static const MachineStateByteConsumer byteConsumers[] = {
	BYTE_CONSUMER(REQUEST_STATE_CONNECTING, MAP_TOKENS(MAP_TOKEN(&okToken, REQUEST_STATE_CONNECTED),
													   MAP_TOKEN(&errorToken, REQUEST_STATE_REQUEST_ERROR))),

	BYTE_CONSUMER(REQUEST_STATE_REQUESTING, MAP_TOKENS(MAP_TOKEN(&caretToken, REQUEST_STATE_PAYLOAD_READY),
													   MAP_TOKEN(&errorToken, REQUEST_STATE_REQUEST_ERROR))),

	BYTE_CONSUMER(REQUEST_STATE_RECEIVING_RESPONSE,
				  MAP_TOKENS(MAP_TOKEN(&closedToken, REQUEST_STATE_REQUEST_DONE),
							 MAP_TOKEN(&errorToken, REQUEST_STATE_REQUEST_ERROR)),
				  ON_BYTE_DATA(process_incoming_ipd, &ipdResponse)),

	BYTE_CONSUMER(REQUEST_STATE_RECEIVING_IPD, ON_BYTE_DATA(process_incoming_ipd_packet, &ipdResponse)),
};

static int process_incoming_ipd(int state, char ch, uint32_t now, void *userData) {
	IpdResponse *ipdResponse = (IpdResponse *)userData;
	parse_ipd(ipdResponse->ipdState, ch);
	if (is_ipd_size_done(ipdResponse->ipdState)) {
		return REQUEST_STATE_RECEIVING_IPD;
	}

	return state;
}

static int process_incoming_ipd_packet(int state, char ch, uint32_t now, void *userData) {
	IpdResponse *ipdResponse = (IpdResponse *)userData;
	parse_ipd(ipdResponse->ipdState, ch);
	if (is_ipd_packet(ipdResponse->ipdState)) {
		parse_http_response_or_reset(ipdResponse->response, ch);
	}
	if (is_ipd_packet_done(ipdResponse->ipdState)) {
		return REQUEST_STATE_REQUEST_DONE;
	}
	return state;
}

void register_request_state_byte_consumers(RequestStateMachine *requestStateMachine) {
	ipdResponse = (IpdResponse){
		.ipdState = &requestStateMachine->ipdState,
		.response = &requestStateMachine->response,
	};
	okToken = &requestStateMachine->okToken;
	errorToken = &requestStateMachine->errorToken;
	caretToken = &requestStateMachine->caretToken;
	closedToken = &requestStateMachine->closedToken;

	for (size_t i = 0; i < sizeof(byteConsumers) / sizeof(byteConsumers[0]); i++) {
		register_machine_state_byte_consumer_config(requestStateMachine->stateMachine, &byteConsumers[i]);
	}
}
