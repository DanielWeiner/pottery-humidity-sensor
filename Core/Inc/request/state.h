#ifndef INC_REQUEST_STATE_H_
#define INC_REQUEST_STATE_H_

#include <stdint.h>

#include "epoch.h"
#include "parser/http_response.h"
#include "parser/ipd.h"
#include "parser/token.h"
#include "sensor.h"
#include "state_machine.h"

typedef enum RequestState {
	REQUEST_STATE_DISABLED,
	REQUEST_STATE_ENABLED,
	REQUEST_STATE_SEND_REQUEST,
	REQUEST_STATE_CONNECTING,
	REQUEST_STATE_CONNECTED,
	REQUEST_STATE_SET_PAYLOAD_LENGTH,
	REQUEST_STATE_REQUESTING,
	REQUEST_STATE_REQUEST_ERROR,
	REQUEST_STATE_PAYLOAD_READY,
	REQUEST_STATE_SEND_PAYLOAD,
	REQUEST_STATE_RECEIVING_RESPONSE,
	REQUEST_STATE_RECEIVING_IPD,
	REQUEST_STATE_REQUEST_DONE,
	REQUEST_STATE_MAX = REQUEST_STATE_REQUEST_DONE
} RequestState;

typedef struct RequestStateMachine {
	StateMachine *const		 stateMachine;
	int *const				 wifiState;
	int *const				 lastSntpUpdate;
	char *const				 requestPayload;
	EpochTick *const		 epochTick;
	const size_t			 payloadBufferSize;
	const size_t			 measurementBufferCount;
	IPDParseState			 ipdState;
	HttpResponse			 response;
	SensorMeasurement *const sensorMeasurements;
	size_t *const			 measurementCount;		// Number of measurements pending
	size_t					 measurementIndex;		// index of the next measurement to send
	size_t					 nextMeasurementIndex;	// index of the measurement to send in the next batch
	size_t					 lastBatchSize;			// how many measurements were sent last
	size_t					 requestLength;			// length of the current request payload
	uint32_t				 lastHttpRequest;		// when the last batch was sent
	TokenParseState			 okToken;
	TokenParseState			 errorToken;
	TokenParseState			 caretToken;
	TokenParseState			 closedToken;
} RequestStateMachine;

#endif /* INC_REQUEST_STATE_H_ */
