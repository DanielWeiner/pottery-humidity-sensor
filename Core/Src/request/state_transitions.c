#include "request/state_transitions.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "wifi/state.h"

static int	  log_request_error(int, uint32_t, void *);
static int	  log_request_batch(int, uint32_t, void *);
static int	  prepare_for_request(int, uint32_t, void *);
static size_t set_request_contents(RequestStateMachine *, size_t);
static size_t calculate_content_length(size_t);

static size_t add_measurement_to_body(char *, size_t, SensorMeasurement *, EpochTick *);
static size_t build_measurements_body(RequestStateMachine *, size_t, size_t);

static RequestStateMachine *stateMachine;

static const MachineStateTransition stateTransitions[] = {
	{REQUEST_STATE_REQUEST_ERROR, REQUEST_STATE_ENABLED, log_request_error},
	{REQUEST_STATE_REQUEST_DONE, REQUEST_STATE_ENABLED, log_request_batch, &stateMachine},
	{REQUEST_STATE_ENABLED, REQUEST_STATE_ENABLED, prepare_for_request, &stateMachine},
	{REQUEST_STATE_CONNECTED, REQUEST_STATE_SET_PAYLOAD_LENGTH, NULL},
	{REQUEST_STATE_PAYLOAD_READY, REQUEST_STATE_SEND_PAYLOAD, NULL}};

static int log_request_error(int state, uint32_t, void *) {
	LOG("HTTP request failed, retrying" CRLF);
	return state;
}

static int log_request_batch(int state, uint32_t now, void *userData) {
	RequestStateMachine *stateMachine = *(RequestStateMachine **)userData;

	if (stateMachine->nextMeasurementIndex != stateMachine->measurementIndex) {
		stateMachine->measurementIndex = stateMachine->nextMeasurementIndex;
		*stateMachine->measurementCount -= stateMachine->lastBatchSize;

		LOG("Sent %d measurements" CRLF, stateMachine->lastBatchSize);

		stateMachine->lastBatchSize = 0;
	}
	return state;
}

static size_t set_request_contents(RequestStateMachine *stateMachine, size_t numMeasurements) {
	if (stateMachine->payloadBufferSize < BODY_SIZE(NUM_BATCH_ENTRIES)) {
		LOG("Buffer size too small for request body" CRLF);
		return 0;
	}
	// clang-format off
	const char requestFormat[] =
		"POST /readings-raw HTTP/1.1" CRLF
		"Host: " ENDPOINT_HOST CRLF
		"Content-Type: text/plain" CRLF
		"User-Agent: PotteryHumiditySensor/0.1 (ESP8266; STM32)" CRLF
		"Content-Length: %u" CRLF
		"Connection: close" DOUBLE_CRLF;
	// clang-format on

	// Calculate and set the value of the Content-Length header
	size_t contentLength = calculate_content_length(numMeasurements);
	size_t headersSize =
		snprintf(stateMachine->requestPayload, stateMachine->payloadBufferSize, requestFormat, contentLength);
	if (headersSize >= stateMachine->payloadBufferSize) {
		LOG("Headers too large for buffer" CRLF);
		return 0;
	}

	// Build the actual body
	size_t bodySize = build_measurements_body(stateMachine, headersSize, numMeasurements);

	if (bodySize == 0) {
		LOG("Error building request body" CRLF);
		return 0;
	}

	return headersSize + bodySize;
}

static size_t calculate_content_length(size_t numMeasurements) {
	return numMeasurements * MEASUREMENT_STRING_SIZE;
}

static size_t add_measurement_to_body(char *buffer, size_t bufferSize, SensorMeasurement *measurement,
									  EpochTick *epochTick) {
	if (!buffer || !measurement || !epochTick) return 0;
	if (bufferSize < MEASUREMENT_STRING_SIZE + 1) {
		LOG("Buffer size too small for measurement" CRLF);
		return 0;
	}

	snprintf(buffer, bufferSize, "%04X%04X%08" PRIX32 "%04X", measurement->temperature, measurement->humidity,
			 epochTick->epoch + measurement->tickMs / S_TO_MS, (uint16_t)(measurement->tickMs % S_TO_MS));

	return MEASUREMENT_STRING_SIZE;	 // Return the size of the measurement string
}

static size_t build_measurements_body(RequestStateMachine *stateMachine, size_t headersSize, size_t numMeasurements) {
	if (!stateMachine || headersSize == 0 || numMeasurements == 0) return 0;

	size_t charsWritten = headersSize;
	for (size_t i = 0; i < numMeasurements; i++) {
		size_t remainingBuffer = stateMachine->payloadBufferSize - charsWritten;
		if (remainingBuffer <= 1) {
			LOG("Buffer too small for measurements" CRLF);
			return 0;
		}

		size_t measurementSize = add_measurement_to_body(
			stateMachine->requestPayload + charsWritten, remainingBuffer,
			&stateMachine->sensorMeasurements[stateMachine->nextMeasurementIndex++], stateMachine->epochTick);

		stateMachine->nextMeasurementIndex %= stateMachine->measurementBufferCount;

		if (measurementSize == 0 || measurementSize >= remainingBuffer) {
			LOG("Error adding measurement to body" CRLF);
			return 0;
		}

		charsWritten += measurementSize;
	}

	return charsWritten - headersSize;	// Return the size of the body only
}

static int prepare_for_request(int state, uint32_t now, void *userData) {
	RequestStateMachine *stateMachine = *(RequestStateMachine **)userData;
	if (now - *stateMachine->lastSntpUpdate >= SNTP_RESYNC_INTERVAL_MS) {
		LOG("Resyncing SNTP time" CRLF);
		*stateMachine->wifiState = WIFI_STATE_UPDATE_SNTP_TIME;
		*stateMachine->lastSntpUpdate = 0;

		return REQUEST_STATE_DISABLED;	// let the wifi state machine handle the clock sync
	}

	// wait for the next request timeout
	if (stateMachine->lastHttpRequest && now - stateMachine->lastHttpRequest < HTTP_REQUEST_DELAY) {
		return state;
	}

	// timeout done, prepare the request
	stateMachine->lastHttpRequest = now;
	uint8_t batchSize =
		*stateMachine->measurementCount > NUM_BATCH_ENTRIES ? NUM_BATCH_ENTRIES : *stateMachine->measurementCount;

	stateMachine->requestLength = set_request_contents(stateMachine, batchSize);
	stateMachine->lastBatchSize = batchSize;

	LOG("Sending %d measurements (max %d) in %d bytes" CRLF, batchSize, NUM_BATCH_ENTRIES, stateMachine->requestLength);

	return REQUEST_STATE_SEND_REQUEST;
}

void register_request_state_transitions(RequestStateMachine *requestStateMachine) {
	stateMachine = requestStateMachine;

	for (size_t i = 0; i < sizeof(stateTransitions) / sizeof(stateTransitions[0]); i++) {
		register_machine_state_transition_config(requestStateMachine->stateMachine, &stateTransitions[i]);
	}
}
