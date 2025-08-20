#include "state_machine.h"

void register_machine_state_byte_consumer_config(StateMachine *config, const MachineStateByteConsumer *consumerConfig) {
	if (!config || !consumerConfig || consumerConfig->processingState >= config->stateCount) return;

	config->byteConsumers[consumerConfig->processingState] = consumerConfig;
}

void register_machine_state_transition_config(StateMachine *config, const MachineStateTransition *transition) {
	if (!config || !transition || transition->prevState >= config->stateCount) return;

	config->stateTransitions[transition->prevState] = transition;
}

void register_machine_state_retry_config(StateMachine *config, const MachineStateRetry *retryConfig) {
	if (!config || !retryConfig || retryConfig->errorState >= config->stateCount ||
		retryConfig->successState >= config->stateCount)
		return;

	config->retries[retryConfig->errorState] = retryConfig;
	config->retries[retryConfig->successState] = retryConfig;
}

void process_machine_state_retry_wait(StateMachine *config, uint32_t now) {
	if (!config || *config->currentState >= config->stateCount) return;

	// Check if the current state has a retry configuration
	const MachineStateRetry *retryConfig = config->retries[*config->currentState];
	if (!retryConfig) return;

	if (*config->currentState == retryConfig->errorState) {
		if (!retry_wait(retryConfig->retryState, now)) return;

		*config->currentState = retryConfig->actionState;
	}
}

void process_machine_state_retry_reset(StateMachine *config, uint32_t now) {
	if (!config || *config->currentState >= config->stateCount) return;

	const MachineStateRetry *retryConfig = config->retries[*config->currentState];
	if (!retryConfig) return;

	if (*config->currentState == retryConfig->successState) {
		reset_retries(retryConfig->retryState, now);
	}
}

void process_machine_state_transition(StateMachine *config, uint32_t now) {
	if (!config || *config->currentState >= config->stateCount) return;

	const MachineStateTransition *transitionConfig = config->stateTransitions[*config->currentState];
	if (!transitionConfig) return;

	if (*config->currentState != transitionConfig->prevState) return;

	int nextState = transitionConfig->nextState;
	if (transitionConfig->onState) {
		// onState may override the next state if it returns something other than the current state
		nextState = transitionConfig->onState(transitionConfig->prevState, now, transitionConfig->userData);
	}

	if (nextState == transitionConfig->prevState) {
		nextState = transitionConfig->nextState;  // if the state did not change, use the next state
	}

	*config->currentState = nextState;
}

void process_machine_state_byte(StateMachine *config, char ch, uint32_t now) {
	if (!config || !ch || *config->currentState >= config->stateCount) return;

	const MachineStateByteConsumer *byteConsumer = config->byteConsumers[*config->currentState];
	if (!byteConsumer) return;

	// Call the custom byte handler for the state if defined
	if (byteConsumer->onByte) {
		*config->currentState = byteConsumer->onByte(*config->currentState, ch, now, byteConsumer->userData);
	}

	// Process all token-based state transitions ("OK" -> SUCCESS, "ERROR" -> FAILURE, etc.)
	for (size_t i = 0; i < byteConsumer->tokenCount; i++) {
		set_state_on_token(config->currentState, *byteConsumer->tokenTransitions[i].token,
						   byteConsumer->tokenTransitions[i].nextState, ch);
	}
}

void process_machine_state(StateMachine *config, char ch, uint32_t now) {
	if (!config) return;

	process_machine_state_retry_wait(config, now);	 // Process the retry wait state
	process_machine_state_byte(config, ch, now);	 // Process the current byte
	process_machine_state_retry_reset(config, now);	 // Reset retries if the current state matches
	process_machine_state_transition(config, now);	 // Apply state transitions
	process_machine_state_retry_reset(config, now);	 // Reset retries if the transitioned state matches
}
