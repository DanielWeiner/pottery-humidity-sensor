#include "retry.h"

__attribute__((weak)) void on_retry(RetryState *retryState) {}
__attribute__((weak)) void on_retry_failure(RetryState *retryState) {}

void reset_retries(RetryState *retryState, uint32_t now) {
	retryState->retries = 0;
	retryState->retryDelay = retryState->baseRetryDelay;
	retryState->lastRetry = now;
}

void reset_retries_on_state_int(RetryState *retryState, int targetState, uint32_t now) {
	if (*retryState->currentState == targetState) {
		reset_retries(retryState, now);
	}
}

bool retry_wait(RetryState *retryState, uint32_t now) {
	if (retryState->retries > retryState->maxRetries) {
		*retryState->currentState = retryState->errorState;
		reset_retries(retryState, now);
		on_retry_failure(retryState);
		return false;
	}

	if (now - retryState->lastRetry < retryState->retryDelay) return false;
	on_retry(retryState);

	retryState->lastRetry = now;
	retryState->retries++;
	retryState->retryDelay *= 2;  // Exponential backoff

	return true;
}
