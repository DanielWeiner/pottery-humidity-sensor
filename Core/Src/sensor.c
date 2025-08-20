#include "sensor.h"

#include <stm32l4xx_hal.h>

#include "constants.h"
#include "log.h"
#include "sht31_crc.h"

extern I2C_HandleTypeDef hi2c1;

static inline bool validate_sht31_crc(uint8_t dataHigh, uint8_t dataLow, uint8_t crc) {
	// Validate the CRC for the SHT31 sensor data
	return crc == crc_finalize(crc_update(crc_init(), (uint8_t[]){dataHigh, dataLow}, 2));
}

bool get_readings(uint16_t *temperature, uint16_t *humidity) {
	// datasheet:
	// https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
	uint8_t commandData[2] = {0x2C, 0x06};	// clock stretching, high repeatability
	uint8_t sensorData[6] = {0};			// bytes 0-1: temperature, byte 2: checksum. bytes
											// 3-4: humidity, byte 5: checksum

	HAL_I2C_Master_Transmit(&hi2c1, SHT31_ADDRESS, commandData, sizeof commandData, HAL_MAX_DELAY);
	HAL_Delay(1);
	HAL_I2C_Master_Receive(&hi2c1, SHT31_ADDRESS, sensorData, sizeof sensorData, HAL_MAX_DELAY);

	// Checksum algorithm is provided by the datasheet and was generated to C by pycrc
	if (!validate_sht31_crc(sensorData[0], sensorData[1], sensorData[2])) {
		LOG("Temperature CRC error" CRLF);
		if (temperature) *temperature = 0;
		if (humidity) *humidity = 0;
		return false;
	}

	if (!validate_sht31_crc(sensorData[3], sensorData[4], sensorData[5])) {
		LOG("Humidity CRC error" CRLF);
		if (temperature) *temperature = 0;
		if (humidity) *humidity = 0;
		return false;
	}

	// Extract the temperature and humidity from the i2c data
	uint16_t tempData = (sensorData[0] << 8) | sensorData[1];
	uint16_t humidityData = (sensorData[3] << 8) | sensorData[4];

	if (temperature) *temperature = tempData;
	if (humidity) *humidity = humidityData;

	return true;
}

void readings_to_float(TemperatureUnit unit, uint16_t temperature, uint16_t humidity, float *tempOut,
					   float *humidityOut) {
	// These conversion formulas are in the SHT3x-D datasheet
	if (tempOut) {
		if (unit == FAHRENHEIT) {
			*tempOut = -49.f + 315.f * (float)temperature / 65535.f;
		} else {
			*tempOut = -45.f + 175.f * (float)temperature / 65535.f;
		}
	}
	if (humidityOut) *humidityOut = 100.0f * (float)humidity / 65535.f;	 // % Relative Humidity
}

void add_measurement(SensorMeasurement *measurements, size_t *measurementCount, size_t *measurementIndex,
					 uint16_t temperature, uint16_t humidity, uint32_t tickMs) {
	if (!measurements || !measurementCount || !measurementIndex) return;

	measurements[*measurementIndex].temperature = temperature;
	measurements[*measurementIndex].humidity = humidity;
	measurements[*measurementIndex].tickMs = tickMs;

	(*measurementIndex)++;
	(*measurementIndex) %= MEASUREMENT_BUFFER_SIZE;

	if (*measurementCount < MEASUREMENT_BUFFER_SIZE) {
		(*measurementCount)++;
	}
}
