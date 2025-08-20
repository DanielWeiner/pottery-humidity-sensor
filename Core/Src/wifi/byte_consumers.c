#include "wifi/byte_consumers.h"

#include <stdbool.h>
#include <string.h>

#include "constants.h"
#include "digits.h"
#include "epoch.h"
#include "log.h"
#include "parser/access_point.h"
#include "parser/parse.h"
#include "parser/token.h"
#include "wifi/state.h"

static bool log_access_point_if_parsed(AccessPoint *, char);
static int	log_AT_response_byte(int, char, uint32_t, void *);
static int	log_access_point_and_check(int, char, uint32_t, void *);
static int	log_access_point(int, char, uint32_t, void *);
static int	add_digit_to_epoch(int, char, uint32_t, void *);

static TokenParseState				 *okToken;
static TokenParseState				 *errorToken;
static TokenParseState				 *gmrCommandToken;
static TokenParseState				 *doubleCrlfToken;
static TokenParseState				 *okTokenNoCRLFPrefix;
static TokenParseState				 *errorTokenNoCRLFPrefix;
static TokenParseState				 *sntpInvalidTime;
static TokenParseState				 *sysTimestampToken;
static TokenParseState				 *wifiDisconnectToken;
static AccessPoint					 *accessPoint;
static AccessPoint					  discardAccessPoint = ACCESS_POINT();
static EpochTick					 *epochTick;
static const MachineStateByteConsumer byteConsumers[] = {
	BYTE_CONSUMER(WIFI_STATE_RESETTING, MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_RESETTING_SUCCESS),
												   MAP_TOKEN(&errorToken, WIFI_STATE_RESETTING_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_GETTING_FIRMWARE_INFO,
				  MAP_TOKENS(MAP_TOKEN(&gmrCommandToken, WIFI_STATE_PRINTING_FIRMWARE_INFO),
							 MAP_TOKEN(&errorToken, WIFI_STATE_FIRMWARE_INFO_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_PRINTING_FIRMWARE_INFO,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_FIRMWARE_INFO_SUCCESS),
							 MAP_TOKEN(&errorToken, WIFI_STATE_FIRMWARE_INFO_ERROR),
							 MAP_TOKEN(&doubleCrlfToken, WIFI_STATE_WAITING_FOR_FIRMWARE_OK)),
				  ON_BYTE(log_AT_response_byte)),

	BYTE_CONSUMER(WIFI_STATE_WAITING_FOR_FIRMWARE_OK,
				  MAP_TOKENS(MAP_TOKEN(&okTokenNoCRLFPrefix, WIFI_STATE_FIRMWARE_INFO_SUCCESS),
							 MAP_TOKEN(&errorTokenNoCRLFPrefix, WIFI_STATE_FIRMWARE_INFO_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_ENABLING, MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_ENABLING_SUCCESS),
												  MAP_TOKEN(&errorToken, WIFI_STATE_ENABLING_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_SCANNING,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_SCANNING_NETWORK_MISS),
							 MAP_TOKEN(&errorToken, WIFI_STATE_SCANNING_ERROR)),
				  ON_BYTE_DATA(log_access_point_and_check, &accessPoint)),

	BYTE_CONSUMER(
		WIFI_STATE_SCANNING_NETWORK_FOUND,
		MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_SCANNING_DONE), MAP_TOKEN(&errorToken, WIFI_STATE_SCANNING_ERROR)),
		ON_BYTE_DATA(log_access_point, &discardAccessPoint)),

	BYTE_CONSUMER(
		WIFI_STATE_CONNECTING,
		MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_CONNECTED), MAP_TOKEN(&errorToken, WIFI_STATE_CONNECTING_ERROR)), ),

	BYTE_CONSUMER(WIFI_STATE_SETTING_CONNECTION_MODE,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_CONNECTION_MODE_SET),
							 MAP_TOKEN(&errorToken, WIFI_STATE_CONNECTION_MODE_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_CONFIGURING_SNTP, MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_SNTP_CONFIGURATION_DONE),
														  MAP_TOKEN(&errorToken, WIFI_STATE_SNTP_CONFIGURATION_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_UPDATING_SNTP_TIME,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_SNTP_UPDATE_DONE),
							 MAP_TOKEN(&errorToken, WIFI_STATE_SNTP_UPDATE_ERROR),
							 MAP_TOKEN(&sntpInvalidTime, WIFI_STATE_UPDATING_SNTP_TIME_NOT_READY))),

	BYTE_CONSUMER(WIFI_STATE_UPDATING_SNTP_TIME_NOT_READY,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_SNTP_QUERY_RETRY),
							 MAP_TOKEN(&errorToken, WIFI_STATE_SNTP_UPDATE_ERROR))),

	BYTE_CONSUMER(WIFI_STATE_GETTING_TIME,
				  MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_TIME_ERROR), MAP_TOKEN(&errorToken, WIFI_STATE_TIME_ERROR),
							 MAP_TOKEN(&sysTimestampToken, WIFI_STATE_GOT_TIMESTAMP_TOKEN))),

	BYTE_CONSUMER(
		WIFI_STATE_TIME_PARSING,
		MAP_TOKENS(MAP_TOKEN(&okToken, WIFI_STATE_TIME_RECEIVED), MAP_TOKEN(&errorToken, WIFI_STATE_TIME_ERROR)),
		ON_BYTE_DATA(add_digit_to_epoch, &epochTick)),

	BYTE_CONSUMER(WIFI_STATE_WIFI_READY, MAP_TOKENS(MAP_TOKEN(&wifiDisconnectToken, WIFI_STATE_RESET)))

};

static bool log_access_point_if_parsed(AccessPoint *ap, char ch) {
	if (parse_status_complete(parse_access_point_or_reset(ap, ch))) {
		if (ap->ssid[0]) {
			LOG("\"%s\" (%02x:%02x:%02x:%02x:%02x:%02x)" CRLF, ap->ssid, ap->mac[0], ap->mac[1], ap->mac[2], ap->mac[3],
				ap->mac[4], ap->mac[5]);
		}
		return true;
	}
	return false;
}

static int log_AT_response_byte(int state, char ch, uint32_t, void *) {
	LOG("%c", ch);
	return state;
}

static int log_access_point_and_check(int state, char ch, uint32_t now, void *userData) {
	AccessPoint *ap = *(AccessPoint **)userData;

	if (log_access_point_if_parsed(ap, ch)) {
		for (size_t i = 0; i < WIFI_CREDENTIALS_COUNT; i++) {
			if (strlen(ap->ssid) != strlen(WIFI_CREDENTIALS[i].ssid)) {
				continue;  // SSID lengths do not match, skip
			}

			if (strncmp(ap->ssid, WIFI_CREDENTIALS[i].ssid, sizeof(ap->ssid)) == 0) {
				strncpy(ap->pass, WIFI_CREDENTIALS[i].pass, sizeof(ap->pass));
				LOG("Found matching access point: %s" CRLF, ap->ssid);
				return WIFI_STATE_SCANNING_NETWORK_FOUND;
			}
		}
	}
	return state;
}

static int log_access_point(int state, char ch, uint32_t now, void *userData) {
	AccessPoint *ap = (AccessPoint *)userData;
	log_access_point_if_parsed(ap, ch);
	return state;
}

static int add_digit_to_epoch(int state, char ch, uint32_t now, void *userData) {
	EpochTick *epochTick = *(EpochTick **)userData;
	if (state == WIFI_STATE_TIME_PARSING) {
		add_digit_if_valid_uint32_t(ch, &epochTick->epoch);
	}
	return state;
}

void register_wifi_state_byte_consumers(WifiStateMachine *wifiStateMachine) {
	okToken = &wifiStateMachine->okToken;
	errorToken = &wifiStateMachine->errorToken;
	gmrCommandToken = &wifiStateMachine->gmrCommandToken;
	doubleCrlfToken = &wifiStateMachine->doubleCrlfToken;
	okTokenNoCRLFPrefix = &wifiStateMachine->okTokenNoCRLFPrefix;
	errorTokenNoCRLFPrefix = &wifiStateMachine->errorTokenNoCRLFPrefix;
	sntpInvalidTime = &wifiStateMachine->sntpInvalidTime;
	accessPoint = &wifiStateMachine->accessPoint;
	sysTimestampToken = &wifiStateMachine->sysTimestampToken;
	wifiDisconnectToken = &wifiStateMachine->wifiDisconnectToken;
	epochTick = wifiStateMachine->epochTick;

	for (size_t i = 0; i < sizeof(byteConsumers) / sizeof(MachineStateByteConsumer); i++) {
		register_machine_state_byte_consumer_config(wifiStateMachine->stateMachine, &byteConsumers[i]);
	}
}
