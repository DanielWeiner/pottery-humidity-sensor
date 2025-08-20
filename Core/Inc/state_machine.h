#ifndef INC_STATE_MACHINE_H_
#define INC_STATE_MACHINE_H_

#include <stddef.h>

#include "parser/token.h"
#include "retry.h"

#define MAP_TOKENS(...)                                            \
	.tokenTransitions = (MachineStateTokenMapping[]){__VA_ARGS__}, \
	.tokenCount = sizeof((MachineStateTokenMapping[]){__VA_ARGS__}) / sizeof(MachineStateTokenMapping)
#define MAP_TOKEN(tok, state) {(TokenParseState * *const)(tok), (state)}
#define BYTE_CONSUMER(state, ...) \
	{.processingState = (state),  \
	 .tokenTransitions = NULL,    \
	 .tokenCount = 0,             \
	 .onByte = NULL,              \
	 .userData = NULL,            \
	 __VA_ARGS__}
#define ON_BYTE_DATA(fn, data) .onByte = (fn), .userData = (data)
#define ON_BYTE(fn) .onByte = (fn), .userData = NULL

typedef struct MachineStateRetry {
	RetryState *const retryState;
	const int		  actionState;	 // action state to retry
	const int		  errorState;	 // state to trigger a retry
	const int		  successState;	 // state to reset retries upon entry
} MachineStateRetry;

typedef struct MachineStateTransition {
	const int prevState;  // state to transition from
	const int nextState;  // state to transition to
	const int (*onState)(int state, uint32_t now, void *userData);
	void *const userData;  // user data for the onState callback
} MachineStateTransition;

typedef struct MachineStateTokenMapping {
	TokenParseState **const token;		// the token to match. Kept as a double pointer to allow static initialization
	const int				nextState;	// the state to transition to on match
} MachineStateTokenMapping;

typedef struct MachineStateByteConsumer {
	const int						processingState;
	const MachineStateTokenMapping *tokenTransitions;
	const size_t					tokenCount;
	const int (*onByte)(int state, char ch, uint32_t now, void *);
	void *const userData;
} MachineStateByteConsumer;

typedef struct StateMachine {
	int *const						 currentState;
	const size_t					 stateCount;
	const MachineStateByteConsumer **byteConsumers;
	const MachineStateTransition   **stateTransitions;
	const MachineStateRetry		   **retries;
} StateMachine;

void register_machine_state_byte_consumer_config(StateMachine *, const MachineStateByteConsumer *);
void register_machine_state_transition_config(StateMachine *, const MachineStateTransition *);
void register_machine_state_retry_config(StateMachine *, const MachineStateRetry *);

void process_machine_state_retry_wait(StateMachine *, uint32_t);
void process_machine_state_retry_reset(StateMachine *, uint32_t);
void process_machine_state_transition(StateMachine *, uint32_t);
void process_machine_state_byte(StateMachine *, char, uint32_t);

void process_machine_state(StateMachine *, char, uint32_t);

#endif /* INC_STATE_MACHINE_H_ */
