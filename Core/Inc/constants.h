#ifndef INC_CONSTANTS_H_
#define INC_CONSTANTS_H_

#if __has_include("constants_private.h")
#include "constants_private.h"
#endif

#ifndef WIFI_CREDENTIALS_COUNT
#define WIFI_CREDENTIALS_COUNT 0
#endif

#define S_TO_MS 1000

#define RX_BUFFER_SIZE 2048

#define SNTP_SERVER_1 "0.us.pool.ntp.org"
#define SNTP_SERVER_2 "1.us.pool.ntp.org"
#define SNTP_SERVER_3 "2.us.pool.ntp.org"

#define CRLF "\r\n"
#define DOUBLE_CRLF "\r\n\r\n"
#define AT_COMMAND_GMR "AT+GMR"
#define AT_COMMAND_CWMODE "AT+CWMODE=1"
#define AT_COMMAND_CWLAP "AT+CWLAP"
#define AT_COMMAND_CWJAP "AT+CWJAP=\"%s\",\"%s\""
#define AT_COMMAND_CIPMUX "AT+CIPMUX=0"
#define AT_COMMAND_CIPSTART "AT+CIPSTART=\"SSL\",\"%s\",443"
#define AT_COMMAND_CIPSEND "AT+CIPSEND=%d"
#define AT_COMMAND_RST "AT+RST"
#define AT_COMMAND_CIPSNTPCFG "AT+CIPSNTPCFG=1,0,\"" SNTP_SERVER_1 "\",\"" SNTP_SERVER_2 "\",\"" SNTP_SERVER_3 "\""
#define AT_COMMAND_CIPSNTPTIME "AT+CIPSNTPTIME?"
#define AT_COMMAND_SYSTIMESTAMP "AT+SYSTIMESTAMP?"

#define SENSOR_THROTTLE_MS 250
#define HTTP_REQUEST_DELAY 5000
#define STARTUP_DELAY_MS 500			 // some delay to allow the ESP8266 to boot
#define SNTP_UPDATE_THROTTLE_MS 4000	 // per pool.ntp.org, minimum 4 seconds between updates
#define WIFI_WATCHDOG_TIMEOUT_MS 180000	 // 3 minute timeout on Wifi startup
#define SHT31_ADDRESS (0x44 << 1)

#define SNTP_SERVER_1 "0.us.pool.ntp.org"
#define SNTP_SERVER_2 "1.us.pool.ntp.org"
#define SNTP_SERVER_3 "2.us.pool.ntp.org"

#endif /* INC_CONSTANTS_H_ */
