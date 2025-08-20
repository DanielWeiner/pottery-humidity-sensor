#ifndef INC_PARSER_ACCESS_POINT_H_
#define INC_PARSER_ACCESS_POINT_H_

#include <stdbool.h>
#include <stdint.h>

#include "parser/int.h"
#include "parser/parse.h"
#include "parser/token.h"

#define ACCESS_POINT()     \
	{.ecn = 0,             \
	 .channel = 0,         \
	 .pairwise_cipher = 0, \
	 .group_cipher = 0,    \
	 .bgn = 0,             \
	 .wps = 0,             \
	 .rssi = 0,            \
	 .freq_offset = 0,     \
	 .freqcal_val = 0,     \
	 .ssid = {0},          \
	 .mac = {0},           \
	 .pass = {0},          \
	 ._parseState = ACCESS_POINT_PARSE_STATE()}

#define ACCESS_POINT_PARSE_STATE()                  \
	{.phase = PARSE_ACCESS_POINT_START,             \
	 .uint8State = {0},                             \
	 .int8State = {0},                              \
	 .ssidState = {0},                              \
	 .macState = {0},                               \
	 .cwlapToken = TOKEN_PARSE_STATE(CWLAP_PREFIX), \
	 .crlfToken = TOKEN_PARSE_STATE(CRLF_TOKEN)}

typedef enum AccessPointParsePhase {
	PARSE_ACCESS_POINT_START,
	PARSE_ACCESS_POINT_ECN,
	PARSE_ACCESS_POINT_SSID,
	PARSE_ACCESS_POINT_RSSI,
	PARSE_ACCESS_POINT_MAC,
	PARSE_ACCESS_POINT_CHANNEL,
	PARSE_ACCESS_POINT_FREQ_OFFSET,
	PARSE_ACCESS_POINT_FREQCAL_VAL,
	PARSE_ACCESS_POINT_PAIRWISE_CIPHER,
	PARSE_ACCESS_POINT_GROUP_CIPHER,
	PARSE_ACCESS_POINT_BGN,
	PARSE_ACCESS_POINT_WPS,
	PARSE_ACCESS_POINT_END
} AccessPointParsePhase;

typedef struct SSIDParseState {
	char   lastChar;
	size_t index;
} SSIDParseState;

typedef struct MACParseState {
	uint8_t currentHexValue;
	size_t	index;
} MACParseState;

typedef struct AccessPointParseState {
	AccessPointParsePhase phase;
	TokenParseState		  cwlapToken;
	TokenParseState		  crlfToken;
	Uint8ParseState		  uint8State;
	Int8ParseState		  int8State;
	SSIDParseState		  ssidState;
	MACParseState		  macState;
} AccessPointParseState;

typedef struct AccessPoint {
	uint8_t				  ecn;				// encryption method
	uint8_t				  channel;			// channel
	uint8_t				  pairwise_cipher;	// pairwise cipher type
	uint8_t				  group_cipher;		// group cipher type
	uint8_t				  bgn;				// bit 0: 802.11b, bit 1: 802.11g, bit 2: 802.11n
	uint8_t				  wps;				// 0 = WPS disabled, 1 = WPS enabled
	int8_t				  rssi;				// signal strength
	int8_t				  freq_offset;		// frequency offset (reserved)
	int8_t				  freqcal_val;		// frequency calibration value (reserved)
	uint8_t				  mac[6];			// mac address
	char				  ssid[33];			// SSID
	char				  pass[64];			// password
	AccessPointParseState _parseState;		// state for parsing access point data
} AccessPoint;

DECLARE_TOKEN(CWLAP_PREFIX, "+CWLAP:(");

void reset_ssid_parse_state(SSIDParseState *);
void reset_mac_parse_state(MACParseState *);
void reset_access_point_parse_state(AccessPointParseState *);
void reset_access_point(AccessPoint *);

ParseStatus parse_access_point(AccessPoint *, char);
ParseStatus parse_access_point_or_reset(AccessPoint *, char);
ParseStatus parse_mac_string(MACParseState *, uint8_t *, char, char);
ParseStatus parse_ssid_string(SSIDParseState *, char *, char, char);
ParseStatus parse_ssid_string_or_reset(SSIDParseState *, char *, char, char);

static inline ParseStatus choose_access_point_phase(ParseStatus status, AccessPointParsePhase *phase,
													AccessPointParsePhase nextPhase) {
	if (parse_status_complete(status)) {
		*phase = nextPhase;
		return PARSE_STATUS_INCOMPLETE;
	}
	return status;
}

#endif /* INC_PARSER_ACCESS_POINT_H_ */
