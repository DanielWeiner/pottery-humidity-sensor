#include "wifi/state_transitions.h"

#include "constants.h"
#include "log.h"
#include "state_machine.h"

static int log_wifi_enabled(int, uint32_t, void *);
static int log_network_found(int, uint32_t, void *);
static int log_network_connected(int, uint32_t, void *);
static int transition_to_sntp_if_epoch_unset(int, uint32_t, void *);
static int set_last_sntp_update(int, uint32_t, void *);
static int set_epoch_tick(int, uint32_t, void *);
static int set_unix_time_offset(int, uint32_t, void *);

static EpochTick   *epochTick = NULL;
static uint32_t	   *lastSntpUpdate = NULL;
static AccessPoint *accessPoint = NULL;

static const MachineStateTransition stateTransitions[] = {
	{WIFI_STATE_RESETTING_SUCCESS, WIFI_STATE_MODULE_WAIT_FOR_READY, NULL, NULL},
	{WIFI_STATE_FIRMWARE_INFO_SUCCESS, WIFI_STATE_ENABLE_WIFI, NULL, NULL},
	{WIFI_STATE_ENABLING_SUCCESS, WIFI_STATE_SCAN_NETWORKS, log_wifi_enabled, NULL},
	{WIFI_STATE_SCANNING_DONE, WIFI_STATE_CONNECT_NETWORK, log_network_found, &accessPoint},
	{WIFI_STATE_SCANNING_NETWORK_MISS, WIFI_STATE_SCAN_NETWORKS, NULL, NULL},
	{WIFI_STATE_CONNECTED, WIFI_STATE_SET_CONNECTION_MODE, log_network_connected, &accessPoint},
	{WIFI_STATE_CONNECTION_MODE_SET, WIFI_STATE_WIFI_READY, transition_to_sntp_if_epoch_unset, &epochTick},
	{WIFI_STATE_SNTP_CONFIGURATION_DONE, WIFI_STATE_UPDATE_SNTP_TIME, set_last_sntp_update, &lastSntpUpdate},
	{WIFI_STATE_SNTP_UPDATE_DONE, WIFI_STATE_GET_TIME, NULL, NULL},
	{WIFI_STATE_SNTP_QUERY_RETRY, WIFI_STATE_UPDATE_SNTP_TIME, NULL, NULL},
	{WIFI_STATE_GOT_TIMESTAMP_TOKEN, WIFI_STATE_TIME_PARSING, set_epoch_tick, &epochTick},
	{WIFI_STATE_TIME_RECEIVED, WIFI_STATE_WIFI_READY, set_unix_time_offset, &epochTick},
};

static int log_wifi_enabled(int state, uint32_t, void *) {
	if (state == WIFI_STATE_ENABLING_SUCCESS) LOG("Wifi enabled" CRLF);
	return state;
}

static int log_network_found(int state, uint32_t, void *userData) {
	AccessPoint *ap = *(AccessPoint **)userData;
	if (state == WIFI_STATE_SCANNING_NETWORK_FOUND) LOG("Network found: %s" CRLF, ap->ssid);
	return state;
}

static int log_network_connected(int state, uint32_t, void *userData) {
	AccessPoint *ap = *(AccessPoint **)userData;
	if (state == WIFI_STATE_CONNECTED) LOG("Connected to network: %s" CRLF, ap->ssid);
	return state;
}

static int transition_to_sntp_if_epoch_unset(int state, uint32_t, void *userData) {
	EpochTick *epochTick = *(EpochTick **)userData;	 // Pointer to the UNIX time offset
	if (epochTick->epoch == 0) return WIFI_STATE_CONFIGURE_SNTP;
	return state;
}

static int set_last_sntp_update(int state, uint32_t now, void *userData) {
	uint32_t *lastSntpUpdate = *(uint32_t **)userData;	// Pointer to the last SNTP update time
	if (state == WIFI_STATE_SNTP_CONFIGURATION_DONE) {
		*lastSntpUpdate = now;
	}
	return state;
}

static int set_epoch_tick(int state, uint32_t now, void *userData) {
	EpochTick *epochTick = *(EpochTick **)userData;	 // Pointer to the UNIX time offset
	if (state == WIFI_STATE_GOT_TIMESTAMP_TOKEN) {
		epochTick->tick = now;	// Store the current tick in ms
		epochTick->epoch = 0;	// Reset the epoch to 0, it will be set when the SNTP time is received
	}
	return state;
}

static int set_unix_time_offset(int state, uint32_t now, void *userData) {
	EpochTick *epochTick = *(EpochTick **)userData;	 // Pointer to the UNIX time offset
	if (state == WIFI_STATE_TIME_RECEIVED) {
		// if we're not at a second boundary, keep polling until we are.
		// This should keep the millisecond offset relative to the epoch consistent across syncs.
		if (epochTick->epoch != epochTick->previousEpoch + 1) {
			epochTick->previousEpoch = epochTick->epoch;
			epochTick->tick = now;
			epochTick->epoch = 0;  // Reset the epoch to 0, it will be set when the SNTP time is received
			return WIFI_STATE_GET_TIME;
		}
		epochTick->epoch -= epochTick->tick / S_TO_MS;	// Store the current time in seconds from the epoch
		LOG("Current UNIX time offset: %lu, ms: %lu" CRLF, epochTick->epoch, epochTick->tick % S_TO_MS);
	}
	return state;
}

void register_wifi_state_transitions(WifiStateMachine *wifiStateMachine) {
	epochTick = wifiStateMachine->epochTick;
	lastSntpUpdate = &wifiStateMachine->lastSntpUpdate;
	accessPoint = &wifiStateMachine->accessPoint;

	for (size_t i = 0; i < sizeof(stateTransitions) / sizeof(MachineStateTransition); i++) {
		register_machine_state_transition_config(wifiStateMachine->stateMachine, &stateTransitions[i]);
	}
}
