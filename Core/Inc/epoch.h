#ifndef INC_EPOCH_H_
#define INC_EPOCH_H_

#include <stdint.h>

#define S_TO_MS 1000

typedef struct EpochTick {
	uint32_t epoch;			 // time at boot since epoch in seconds
	uint32_t previousEpoch;	 // last reported epoch seconds, used to detect the second boundary
	uint32_t tick;			 // STM32 tick in ms that was recorded when the epoch was last updated
} EpochTick;

#endif /* INC_EPOCH_H_ */
