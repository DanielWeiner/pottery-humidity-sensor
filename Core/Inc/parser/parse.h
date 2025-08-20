#ifndef INC_PARSER_PARSE_H_
#define INC_PARSER_PARSE_H_

#include <stdbool.h>
#include <stddef.h>

typedef enum ParseStatus { PARSE_STATUS_INCOMPLETE, PARSE_STATUS_COMPLETE, PARSE_STATUS_INVALID } ParseStatus;

static inline bool parse_status_complete(ParseStatus status) {
	return status == PARSE_STATUS_COMPLETE;
}

static inline bool parse_status_invalid(ParseStatus status) {
	return status == PARSE_STATUS_INVALID;
}

static inline bool parse_status_incomplete(ParseStatus status) {
	return status == PARSE_STATUS_INCOMPLETE;
}

#endif /* INC_PARSER_PARSE_H_ */
