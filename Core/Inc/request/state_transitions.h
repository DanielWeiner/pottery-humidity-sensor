#ifndef INC_REQUEST_STATE_TRANSITIONS_H
#define INC_REQUEST_STATE_TRANSITIONS_H

#include "request/state.h"

#define SNTP_RESYNC_INTERVAL_MS 120000	// 2 minutes to resync clock, to mitigate drift
#define HTTP_REQUEST_DELAY 5000

#define ENDPOINT_HOST "process-sensor-readings-405985529031.us-central1.run.app"
#define UINT16_HEX_LEN 4
#define UINT32_HEX_LEN 8
#define MEASUREMENT_STRING_SIZE (UINT16_HEX_LEN + UINT16_HEX_LEN + UINT32_HEX_LEN + UINT16_HEX_LEN)
#define BODY_SIZE(numEntries) (MEASUREMENT_STRING_SIZE * (numEntries))
// clang-format off
#define MAX_HEADER_SIZE                                               \
	sizeof(                                                           \
		"POST /readings-raw HTTP/1.1" CRLF                            \
		"Host: " ENDPOINT_HOST CRLF                                   \
		"Content-Type: text/plain" CRLF                               \
		"User-Agent: PotteryHumiditySensor/0.1 (ESP8266; STM32)" CRLF \
		"Content-Length: 2048" CRLF                                   \
		"Connection: close" DOUBLE_CRLF                               \
	)
// clang-format on
// maximum number of entries in a single batch.
// comes out to 91 records at a total request length of 2040 bytes
#define NUM_BATCH_ENTRIES ((RX_BUFFER_SIZE - MAX_HEADER_SIZE) / MEASUREMENT_STRING_SIZE)
#define PAYLOAD_BUFFER_SIZE (BODY_SIZE(NUM_BATCH_ENTRIES) + MAX_HEADER_SIZE)

void register_request_state_transitions(RequestStateMachine *requestStateMachine);

#endif /* INC_REQUEST_STATE_TRANSITIONS_H */
