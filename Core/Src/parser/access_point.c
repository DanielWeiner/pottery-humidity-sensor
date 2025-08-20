#include "parser/access_point.h"

#include <string.h>

#include "digits.h"

#define SSID_MAX_LEN 32
#define SSID_INDEX_INITIAL SSID_MAX_LEN
#define SSID_INDEX_FINAL (SSID_MAX_LEN + 1)

#define MAC_LEN 6
#define MAC_STR_LEN (MAC_LEN * 3 - 1)
#define MAC_STR_INDEX_INITIAL (MAC_STR_LEN + 1)
#define MAC_STR_INDEX_FINAL (MAC_STR_INDEX_INITIAL + 1)

void reset_ssid_parse_state(SSIDParseState *state) {
	state->index = SSID_INDEX_INITIAL;
}

void reset_mac_parse_state(MACParseState *state) {
	state->index = MAC_STR_INDEX_INITIAL;
	state->currentHexValue = 0;
}

void reset_access_point_parse_state(AccessPointParseState *state) {
	state->phase = PARSE_ACCESS_POINT_START;
	reset_token_state(&state->cwlapToken);
	reset_token_state(&state->crlfToken);
	reset_uint8_parse_state(&state->uint8State);
	reset_int8_parse_state(&state->int8State);
	reset_ssid_parse_state(&state->ssidState);
	reset_mac_parse_state(&state->macState);
}

void reset_access_point(AccessPoint *accessPoint) {
	accessPoint->ecn = 0;
	accessPoint->channel = 0;
	accessPoint->pairwise_cipher = 0;
	accessPoint->group_cipher = 0;
	accessPoint->bgn = 0;
	accessPoint->wps = 0;
	accessPoint->rssi = 0;
	accessPoint->freq_offset = 0;
	accessPoint->freqcal_val = 0;
	accessPoint->ssid[0] = '\0';
	memset(accessPoint->mac, 0, MAC_LEN);
}

ParseStatus parse_mac_string_or_reset(MACParseState *state, uint8_t *mac, char ch, char endChar) {
	ParseStatus status = parse_mac_string(state, mac, ch, endChar);
	switch (status) {
		case PARSE_STATUS_INVALID:
			memset(mac, 0, MAC_LEN);
		case PARSE_STATUS_COMPLETE:
			reset_mac_parse_state(state);
		default:
			return status;
	}
}

ParseStatus parse_mac_string(MACParseState *state, uint8_t *mac, char ch, char endChar) {
	// MAC_STR_INDEX_INITIAL is an OOB sentinel value indicating the lack of an open quote
	if (state->index == MAC_STR_INDEX_INITIAL) {
		if (ch != '"') return PARSE_STATUS_INVALID;
		state->index = 0;
		return PARSE_STATUS_INCOMPLETE;
	}

	if (state->index == MAC_STR_LEN) {
		if (ch != '"') return PARSE_STATUS_INVALID;
		mac[MAC_LEN - 1] = state->currentHexValue;
		state->index = MAC_STR_INDEX_FINAL;
		return PARSE_STATUS_INCOMPLETE;
	}

	// MAC_INDEX_FINAL is an OOB sentinel value indicating the presence of an close quote
	if (state->index == MAC_STR_INDEX_FINAL) {
		if (ch != endChar) return PARSE_STATUS_INVALID;
		return PARSE_STATUS_COMPLETE;
	}

	size_t index = state->index++;
	if (index % 3 == 2) {
		if (ch != ':') return PARSE_STATUS_INVALID;
		mac[index / 3] = state->currentHexValue;
		state->currentHexValue = 0;
		return PARSE_STATUS_INCOMPLETE;
	}
	if (!add_hex_digit_if_valid_uint8_t(ch, &state->currentHexValue)) return PARSE_STATUS_INVALID;
	return PARSE_STATUS_INCOMPLETE;
}

ParseStatus parse_ssid_string_or_reset(SSIDParseState *state, char *dest, char ch, char endChar) {
	ParseStatus status = parse_ssid_string(state, dest, ch, endChar);
	switch (status) {
		case PARSE_STATUS_INVALID:
			dest[0] = '\0';
		case PARSE_STATUS_COMPLETE:
			reset_ssid_parse_state(state);
		default:
			return status;
	}
}

ParseStatus parse_ssid_string(SSIDParseState *state, char *dest, char ch, char endChar) {
	// SSID_INDEX_INITIAL is an OOB sentinel value indicating the lack of an open quote
	char lastChar = state->lastChar;
	state->lastChar = ch;

	if (state->index == SSID_INDEX_INITIAL) {
		if (ch != '"') return PARSE_STATUS_INVALID;
		state->index = 0;
		return PARSE_STATUS_INCOMPLETE;
	}

	// SSID_INDEX_FINAL is an OOB sentinel value indicating the presence of an close quote
	if (state->index == SSID_INDEX_FINAL) {
		if (ch != endChar) return PARSE_STATUS_INVALID;
		return PARSE_STATUS_COMPLETE;
	}

	if (lastChar == '\\') {
		if (state->index < SSID_MAX_LEN) {
			dest[state->index++] = ch;
			return PARSE_STATUS_INCOMPLETE;
		}

		return PARSE_STATUS_INVALID;
	}

	if (ch == '"') {
		dest[state->index] = '\0';
		state->index = SSID_INDEX_FINAL;
		return PARSE_STATUS_INCOMPLETE;
	}

	if (state->index == SSID_MAX_LEN) return PARSE_STATUS_INVALID;
	if (ch == '\\') return PARSE_STATUS_INCOMPLETE;

	dest[state->index++] = ch;
	return PARSE_STATUS_INCOMPLETE;
}

ParseStatus parse_access_point_or_reset(AccessPoint *accessPoint, char ch) {
	ParseStatus status = parse_access_point(accessPoint, ch);
	if (parse_status_invalid(status)) {
		reset_access_point(accessPoint);
		reset_access_point_parse_state(&accessPoint->_parseState);
		status = parse_access_point(accessPoint, ch);
	}

	switch (status) {
		case PARSE_STATUS_INVALID:
		case PARSE_STATUS_COMPLETE:
			reset_access_point_parse_state(&accessPoint->_parseState);
		default:
			return status;
	}
}

ParseStatus parse_access_point(AccessPoint *accessPoint, char ch) {
	AccessPointParseState *state = &accessPoint->_parseState;
	switch (state->phase) {
		case PARSE_ACCESS_POINT_START:
			return choose_access_point_phase(parse_token(&state->cwlapToken, ch), &state->phase,
											 PARSE_ACCESS_POINT_ECN);
		case PARSE_ACCESS_POINT_ECN:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->ecn, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_SSID);
		case PARSE_ACCESS_POINT_SSID:
			return choose_access_point_phase(parse_ssid_string_or_reset(&state->ssidState, accessPoint->ssid, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_RSSI);
		case PARSE_ACCESS_POINT_RSSI:
			return choose_access_point_phase(parse_int8_t(&state->int8State, &accessPoint->rssi, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_MAC);
		case PARSE_ACCESS_POINT_MAC:
			return choose_access_point_phase(parse_mac_string_or_reset(&state->macState, accessPoint->mac, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_CHANNEL);
		case PARSE_ACCESS_POINT_CHANNEL:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->channel, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_FREQ_OFFSET);
		case PARSE_ACCESS_POINT_FREQ_OFFSET:
			return choose_access_point_phase(parse_int8_t(&state->int8State, &accessPoint->freq_offset, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_FREQCAL_VAL);
		case PARSE_ACCESS_POINT_FREQCAL_VAL:
			return choose_access_point_phase(parse_int8_t(&state->int8State, &accessPoint->freqcal_val, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_PAIRWISE_CIPHER);
		case PARSE_ACCESS_POINT_PAIRWISE_CIPHER:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->pairwise_cipher, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_GROUP_CIPHER);
		case PARSE_ACCESS_POINT_GROUP_CIPHER:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->group_cipher, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_BGN);
		case PARSE_ACCESS_POINT_BGN:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->bgn, ch, ','),
											 &state->phase, PARSE_ACCESS_POINT_WPS);
		case PARSE_ACCESS_POINT_WPS:
			return choose_access_point_phase(parse_uint8_t(&state->uint8State, &accessPoint->wps, ch, ')'),
											 &state->phase, PARSE_ACCESS_POINT_END);
		case PARSE_ACCESS_POINT_END:
			return parse_token_or_reset(&state->crlfToken, ch);
		default:
			return PARSE_STATUS_INVALID;
	}
}
