#ifndef INC_DIGITS_H_
#define INC_DIGITS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INVALID_HEX 16
#define INVALID_DEC 10

static inline bool is_valid_digit(char ch) {
	return ch >= '0' && ch <= '9';
}

static inline bool is_valid_hex(char ch) {
	return is_valid_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static inline uint8_t dec_to_int(char ch) {
	return is_valid_digit(ch) ? ch - '0' : INVALID_DEC;
}

static inline uint8_t hex_to_int(char ch) {
	if (is_valid_digit(ch)) return ch - '0';
	if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
	if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';

	return INVALID_HEX;
}

bool add_digit_if_valid_size_t(char, size_t *);
bool add_digit_if_valid_uint16_t(char, uint16_t *);
bool add_digit_if_valid_int8_t(char, int8_t *, int8_t);
bool add_digit_if_valid_uint8_t(char, uint8_t *);
bool add_digit_if_valid_uint32_t(char, uint32_t *);
bool add_hex_digit_if_valid_uint8_t(char, uint8_t *);

#endif /* INC_DIGITS_H_ */
