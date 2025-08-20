#ifndef INC_SENSOR_H_
#define INC_SENSOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEASUREMENT_BUFFER_SIZE_BYTES 16384
#define MEASUREMENT_SIZE sizeof(SensorMeasurement)
// sizeof(SensorMeasurement) is 8, so 2048 measurements can be stored in 16k of RAM
// since measurements are taken once every 0.25 seconds, that means 512 seconds of data can be stored, or ~8.5 minutes
#define MEASUREMENT_BUFFER_SIZE (MEASUREMENT_BUFFER_SIZE_BYTES / MEASUREMENT_SIZE)

typedef struct SensorMeasurement {
	uint16_t temperature;
	uint16_t humidity;
	uint32_t tickMs;  // current tick in ms. Will be added to the epoch timestamp collected upon startup
} SensorMeasurement;

typedef enum SensorState { SENSOR_STATE_IDLE, SENSOR_STATE_READING, SENSOR_STATE_DISPLAYING } SensorState;
typedef enum { FAHRENHEIT, CELSIUS } TemperatureUnit;

bool get_readings(uint16_t *, uint16_t *);
void readings_to_float(TemperatureUnit, uint16_t, uint16_t, float *, float *);
void add_measurement(SensorMeasurement *, size_t *, size_t *, uint16_t, uint16_t, uint32_t);

#endif /* INC_SENSOR_H_ */
