#ifndef INC_RETRY_H_
#define INC_RETRY_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct RetryState {
	uint32_t baseRetryDelay;  // in milliseconds
	uint32_t retryDelay;	  // in milliseconds
	uint32_t lastRetry;		  // HAL_GetTick() value of the last retry
	int		*currentState;
	int		 errorState;  // the state to switch to on error
	uint8_t	 retries;
	uint8_t	 maxRetries;
} RetryState;

#define reset_retries_on_state(retryState, targetState, now) \
	reset_retries_on_state_int((retryState), (int)(targetState), (now))
#define RETRY_STATE(_currentState, _errorState, _maxRetries, _baseRetryDelay) \
	{.currentState = (int *)(_currentState),                                  \
	 .errorState = (int)(_errorState),                                        \
	 .maxRetries = (_maxRetries),                                             \
	 .baseRetryDelay = (_baseRetryDelay),                                     \
	 .retryDelay = (_baseRetryDelay),                                         \
	 .lastRetry = 0,                                                          \
	 .retries = 0}

void on_retry(RetryState *retryState);
void on_retry_failure(RetryState *retryState);
bool retry_wait(RetryState *retryState, uint32_t);
void reset_retries(RetryState *retryState, uint32_t);
void reset_retries_on_state_int(RetryState *retryState, int targetState, uint32_t now);

#endif /* INC_RETRY_H_ */
