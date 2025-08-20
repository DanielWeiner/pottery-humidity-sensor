#include "digits.h"

bool add_digit_if_valid_int8_t(char ch, int8_t *value, int8_t sign) {
	if (!is_valid_digit(ch)) return false;
	int16_t newValue = *value * 10 + (ch - '0') * sign;			   // use a wider type to check for over/underflow
	if (newValue < INT8_MIN || newValue > INT8_MAX) return false;  // Integer over/underflow
	*value = (int8_t)newValue;									   // update the value as an int8
	return true;
}

bool add_digit_if_valid_size_t(char ch, size_t *value) {
	if (!is_valid_digit(ch)) return false;
	size_t newValue = *value * 10 + dec_to_int(ch);
	if (newValue < *value) return false;  // Integer overflow
	*value = newValue;
	return true;
}

bool add_digit_if_valid_uint32_t(char ch, uint32_t *value) {
	if (!is_valid_digit(ch)) return false;
	uint32_t newValue = *value * 10 + dec_to_int(ch);
	if (newValue < *value) return false;  // Integer overflow
	*value = newValue;
	return true;
}

bool add_digit_if_valid_uint16_t(char ch, uint16_t *value) {
	if (!is_valid_digit(ch)) return false;
	uint16_t newValue = *value * 10 + dec_to_int(ch);
	if (newValue < *value) return false;  // Integer overflow
	*value = newValue;
	return true;
}

bool add_digit_if_valid_uint8_t(char ch, uint8_t *value) {
	if (!is_valid_digit(ch)) return false;
	uint8_t newValue = *value * 10 + dec_to_int(ch);
	if (newValue < *value) return false;  // Integer overflow
	*value = newValue;
	return true;
}

bool add_hex_digit_if_valid_uint8_t(char ch, uint8_t *value) {
	if (!is_valid_hex(ch)) return false;
	uint8_t newValue = *value * 16 + hex_to_int(ch);
	if (newValue < *value) return false;  // Integer overflow
	*value = newValue;
	return true;
}
