/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Monitors the temperature and humidity in a ceramics workshop.
 * 					 By monitoring the humidity of drying pots, the user can be
 * 					 better informed about when to trim the pots or adjust the
 * 					 room's environment for optimal drying conditions.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Daniel Weiner.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <i2c_lcd.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chars.h"
#include "constants.h"
#include "digits.h"
#include "esp8266.h"
#include "lcd.h"
#include "log.h"
#include "parser/access_point.h"
#include "parser/http_response.h"
#include "parser/int.h"
#include "parser/ipd.h"
#include "parser/parse.h"
#include "parser/token.h"
#include "request/byte_consumers.h"
#include "request/state.h"
#include "request/state_transitions.h"
#include "retry.h"
#include "sensor.h"
#include "state_machine.h"
#include "wifi/byte_consumers.h"
#include "wifi/retries.h"
#include "wifi/state.h"
#include "wifi/state_transitions.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DECLARE_AT_TOKEN(name, tk) DECLARE_TOKEN(name, CRLF tk)
#define DECLARE_AT_TOKEN_CRLF(name, tk) DECLARE_AT_TOKEN(name, tk CRLF)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define send_esp8266_message_default(fmt) (esp8266_printf_default(&esp8266State, (fmt CRLF)))
#define sendf_esp8266_message_default(fmt, ...) (esp8266_printf_default(&esp8266State, (fmt CRLF), __VA_ARGS__))
#define send_esp8266_message(buf, len, fmt) (esp8266_printf(&esp8266State, buf, len, (fmt CRLF)))
#define sendf_esp8266_message(buf, len, fmt, ...) (esp8266_printf(&esp8266State, buf, len, (fmt CRLF), __VA_ARGS__))
#define send_esp8266_raw(buf, len) (esp8266_send(&esp8266State, buf, len))

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static char				 rxBuffer[RX_BUFFER_SIZE];	// RX ring buffer
static uint8_t			 rxByte = 0;
static volatile uint16_t rxBufferTail = 0;
static ESP8266State		*esp8266StatePtr = NULL;

DECLARE_AT_TOKEN_CRLF(OK, ESP8266_TOKEN_OK);
DECLARE_TOKEN(OK_NO_PREFIX, ESP8266_TOKEN_OK CRLF);
DECLARE_AT_TOKEN_CRLF(ERROR_TOKEN, ESP8266_TOKEN_ERROR);
DECLARE_TOKEN(ERROR_NO_PREFIX, ESP8266_TOKEN_ERROR CRLF);
DECLARE_TOKEN(READY, ESP8266_TOKEN_READY CRLF);
DECLARE_AT_TOKEN_CRLF(CLOSED, ESP8266_TOKEN_CLOSED);
DECLARE_AT_TOKEN(CARET, ">");
DECLARE_IPD_TOKEN();
DECLARE_TOKEN(AT_TOKEN_GMR, AT_COMMAND_GMR CRLF);
DECLARE_AT_TOKEN_CRLF(AT_TOKEN_WIFI_DISCONNECT, ESP8266_TOKEN_WIFI_DISCONNECT);
// default year, before SNTP updates the ESP8266's clock... unless it takes a year to
// do so (or it's actually 1970)
DECLARE_TOKEN(AT_TOKEN_INVALID_SNTP_TIME, "1970" CRLF);
DECLARE_AT_TOKEN(AT_TOKEN_SYSTIMESTAMP, "+SYSTIMESTAMP:");
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void		SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void display_readings(I2C_LCD_HandleTypeDef *, TemperatureUnit, float, float);
static char get_next_esp8266_byte();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	static uint16_t rxIndex = 0;
	if (huart->Instance == USART1) {
		rxBuffer[rxIndex++] = rxByte;
		rxIndex %= RX_BUFFER_SIZE;
		rxBufferTail = rxIndex;
		HAL_UART_Receive_IT(&huart1, &rxByte, 1);
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		esp8266_transmit_complete(esp8266StatePtr);
	}
}

static char get_next_esp8266_byte() {
	static uint16_t head = 0;
	uint16_t		tail = rxBufferTail;

	if (head == tail) return '\0';

	char ch = rxBuffer[head++];
	head %= RX_BUFFER_SIZE;
	return ch;
}

static void display_readings(I2C_LCD_HandleTypeDef *lcd, TemperatureUnit unit, float temperature, float humidity) {
	char unitChar = unit == FAHRENHEIT ? 'F' : 'C';
	printf_lcd(lcd, 0, "Temp: %.1f %c", temperature, unitChar);
	printf_lcd(lcd, 1, "Humidity: %.1f%%", humidity);
}

void on_retry(RetryState *state) {
	LOG("Retrying in %lu ms..." CRLF, state->retryDelay);
}

void on_retry_failure(RetryState *state) {
	LOG("Retry failure after %d attempts" CRLF, state->retries);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_I2C1_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	static I2C_LCD_HandleTypeDef lcd1;
	static char					 requestPayload[PAYLOAD_BUFFER_SIZE] = {0};
	static SensorMeasurement	 measurements[MEASUREMENT_BUFFER_SIZE] = {0};
	static size_t				 measurementCount = 0;
	static size_t				 measurementWriteIndex = 0;	 // index to write the next measurement to
	static ESP8266State			 esp8266State = {
				 .status = ESP8266_STATE_TRANSMITTING_DONE, .readyToken = TOKEN_PARSE_STATE(READY), .uart = &huart1};
	static uint32_t		 lastSensorReading = 0;
	static GPIO_PinState lastButtonState = GPIO_PIN_SET;
	static uint32_t		 lastWifiReady = 0;	 // Last time the WiFi state was updated

	static WifiState	wifiState = WIFI_STATE_RESET;
	static RequestState requestState = REQUEST_STATE_DISABLED;
	static EpochTick	epochTick = {0};

	static const MachineStateRetry		  *wifiRetries[WIFI_STATE_MAX + 1] = {NULL};
	static const MachineStateTransition	  *wifiStateTransitions[WIFI_STATE_MAX + 1] = {NULL};
	static const MachineStateByteConsumer *wifiByteConsumers[WIFI_STATE_MAX + 1] = {NULL};

	static const MachineStateRetry		  *requestRetries[REQUEST_STATE_MAX + 1] = {NULL};
	static const MachineStateTransition	  *requestStateTransitions[REQUEST_STATE_MAX + 1] = {NULL};
	static const MachineStateByteConsumer *requestByteConsumers[REQUEST_STATE_MAX + 1] = {NULL};

	static StateMachine wifiStateMachineBase = {
		.currentState = (int *)&wifiState,
		.retries = wifiRetries,
		.stateTransitions = wifiStateTransitions,
		.byteConsumers = wifiByteConsumers,
		.stateCount = WIFI_STATE_MAX + 1,
	};

	static WifiStateMachine wifiStateMachine = {
		.epochTick = &epochTick,
		.lastSntpUpdate = 0,
		.okToken = TOKEN_PARSE_STATE(OK),
		.okTokenNoCRLFPrefix = TOKEN_PARSE_STATE(OK_NO_PREFIX),
		.errorToken = TOKEN_PARSE_STATE(ERROR_TOKEN),
		.errorTokenNoCRLFPrefix = TOKEN_PARSE_STATE(ERROR_NO_PREFIX),
		.doubleCrlfToken = TOKEN_PARSE_STATE(DOUBLE_CRLF_TOKEN),
		.gmrCommandToken = TOKEN_PARSE_STATE(AT_TOKEN_GMR),
		.wifiDisconnectToken = TOKEN_PARSE_STATE(AT_TOKEN_WIFI_DISCONNECT),
		.sntpInvalidTime = TOKEN_PARSE_STATE(AT_TOKEN_INVALID_SNTP_TIME),
		.sysTimestampToken = TOKEN_PARSE_STATE(AT_TOKEN_SYSTIMESTAMP),
		.accessPoint = ACCESS_POINT(),
		.stateMachine = &wifiStateMachineBase,
	};

	static StateMachine requestStateMachineBase = {
		.currentState = (int *)&requestState,
		.retries = requestRetries,
		.stateTransitions = requestStateTransitions,
		.byteConsumers = requestByteConsumers,
		.stateCount = REQUEST_STATE_MAX + 1,
	};

	static RequestStateMachine requestStateMachine = {.stateMachine = &requestStateMachineBase,
													  .wifiState = (int *)&wifiState,
													  .lastSntpUpdate = (int *)&wifiStateMachine.lastSntpUpdate,
													  .requestPayload = requestPayload,
													  .epochTick = &epochTick,
													  .sensorMeasurements = measurements,
													  .measurementCount = &measurementCount,
													  .payloadBufferSize = PAYLOAD_BUFFER_SIZE,
													  .measurementBufferCount = MEASUREMENT_BUFFER_SIZE,
													  .ipdState = IPD_PARSE_STATE(),
													  .response = HTTP_RESPONSE(),
													  .measurementIndex = 0,
													  .nextMeasurementIndex = 0,
													  .lastBatchSize = 0,
													  .requestLength = 0,
													  .lastHttpRequest = 0,
													  .okToken = TOKEN_PARSE_STATE(OK),
													  .errorToken = TOKEN_PARSE_STATE(ERROR_TOKEN),
													  .caretToken = TOKEN_PARSE_STATE(CARET),
													  .closedToken = TOKEN_PARSE_STATE(CLOSED)};

	register_wifi_state_transitions(&wifiStateMachine);
	register_wifi_state_retries(&wifiStateMachine);
	register_wifi_state_byte_consumers(&wifiStateMachine);

	register_request_state_transitions(&requestStateMachine);
	register_request_state_byte_consumers(&requestStateMachine);

	esp8266StatePtr = &esp8266State;
	lcd1.hi2c = &hi2c1;
	lcd1.address = LCD_ADDRESS;
	lcd_init(&lcd1);

	HAL_UART_Receive_IT(&huart1, &rxByte, 1);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	LOG("-------------------------------------------------------------------" CRLF CRLF);
	while (1) {
		uint32_t now = HAL_GetTick();

		GPIO_PinState buttonState = HAL_GPIO_ReadPin(USER_TOGGLE_GPIO_Port, USER_TOGGLE_Pin);
		bool		  buttonPressed = buttonState == GPIO_PIN_RESET;

		// LCD backlight is turned on when the button is pressed, off when released
		if (lastButtonState != buttonState) {
			if (buttonPressed) {
				lcd_backlight_on(&lcd1);
			} else {
				lcd_backlight_off(&lcd1);
			}
		}

		lastButtonState = buttonState;

		if (lastSensorReading == 0 || now - lastSensorReading >= SENSOR_THROTTLE_MS) {
			lastSensorReading = now;

			// Turn on the status LED while making measurements if the button is
			// pressed
			if (buttonPressed) HAL_GPIO_WritePin(STATUS_LED_OUT_GPIO_Port, STATUS_LED_OUT_Pin, GPIO_PIN_SET);

			// Write to the LCD readout

			TemperatureUnit tempUnit = FAHRENHEIT;
			float			temperatureFormatted = 0.f, humidityFormatted = 0.f;
			uint16_t		temperature = 0, humidity = 0;
			if (get_readings(&temperature, &humidity)) {
				readings_to_float(tempUnit, temperature, humidity, &temperatureFormatted, &humidityFormatted);
				add_measurement(measurements, &measurementCount, &measurementWriteIndex, temperature, humidity, now);
				display_readings(&lcd1, tempUnit, temperatureFormatted, humidityFormatted);
			}

			// Turn off the status LED
			if (buttonPressed) HAL_GPIO_WritePin(STATUS_LED_OUT_GPIO_Port, STATUS_LED_OUT_Pin, GPIO_PIN_RESET);
		}

		// Get the next byte from the ESP8266 ring buffer, if any.
		char receivedByte = get_next_esp8266_byte();
		process_machine_state(&wifiStateMachineBase, receivedByte, now);
		process_machine_state(&requestStateMachineBase, receivedByte, now);

		switch (wifiState) {
			case WIFI_STATE_RESET:
				if (now < STARTUP_DELAY_MS) break;	// give the ESP8266 time to be ready after cold boot
				esp8266_set_status(&esp8266State, ESP8266_STATE_TRANSMITTING_DONE);
				if (send_esp8266_message_default(AT_COMMAND_RST)) {
					LOG("Resetting ESP8266..." CRLF);
					lastWifiReady = now;
					wifiState = WIFI_STATE_RESETTING;
				}
				break;
			case WIFI_STATE_MODULE_WAIT_FOR_READY:
				esp8266_wait_until_ready(&esp8266State, receivedByte);
				if (esp8266_has_status(&esp8266State, ESP8266_STATE_READY)) {
					LOG("ESP8266 ready" CRLF);
					wifiState = WIFI_STATE_GET_FIRMWARE_INFO;
				}
				break;
			case WIFI_STATE_GET_FIRMWARE_INFO:
				if (send_esp8266_message_default(AT_COMMAND_GMR)) {
					LOG("ESP8266 firmware info:" CRLF);
					wifiState = WIFI_STATE_GETTING_FIRMWARE_INFO;
				}
				break;
			case WIFI_STATE_ENABLE_WIFI:
				if (send_esp8266_message_default(AT_COMMAND_CWMODE)) {
					LOG("Enabling wifi..." CRLF);
					wifiState = WIFI_STATE_ENABLING;
				}
				break;
			case WIFI_STATE_SCAN_NETWORKS:
				if (send_esp8266_message_default(AT_COMMAND_CWLAP)) {
					LOG("Scanning networks..." CRLF);
					wifiState = WIFI_STATE_SCANNING;
				}
				break;
			case WIFI_STATE_CONNECT_NETWORK:
				AccessPoint *ap = &wifiStateMachine.accessPoint;

				if (sendf_esp8266_message_default(AT_COMMAND_CWJAP, ap->ssid, ap->pass)) {
					LOG("Connecting to %s" CRLF, ap->ssid);
					wifiState = WIFI_STATE_CONNECTING;
				}
				break;
			case WIFI_STATE_SET_CONNECTION_MODE:
				if (send_esp8266_message_default(AT_COMMAND_CIPMUX)) {
					wifiState = WIFI_STATE_SETTING_CONNECTION_MODE;
				}
				break;
			case WIFI_STATE_CONFIGURE_SNTP:
				if (send_esp8266_message_default(AT_COMMAND_CIPSNTPCFG)) {
					LOG("Getting remote time..." CRLF);
					wifiState = WIFI_STATE_CONFIGURING_SNTP;
				}
				break;
			case WIFI_STATE_UPDATE_SNTP_TIME:
				if (now - wifiStateMachine.lastSntpUpdate < SNTP_UPDATE_THROTTLE_MS) break;
				if (send_esp8266_message_default(AT_COMMAND_CIPSNTPTIME)) {
					wifiState = WIFI_STATE_UPDATING_SNTP_TIME;
					wifiStateMachine.lastSntpUpdate = now;
				}
				break;
			case WIFI_STATE_GET_TIME:
				if (send_esp8266_message_default(AT_COMMAND_SYSTIMESTAMP)) {
					wifiState = WIFI_STATE_GETTING_TIME;
				}
				break;
			case WIFI_STATE_WIFI_READY:
				lastWifiReady = now;  // keep the watchdog timer fresh during normal operation
				break;
			default:
				break;
		}

		if (wifiState != WIFI_STATE_WIFI_READY && now - lastWifiReady > WIFI_WATCHDOG_TIMEOUT_MS) {
			LOG("Wifi watchdog timeout" CRLF);
			wifiState = WIFI_STATE_RESET;  // reset the wifi state on watchdog timeout
			continue;
		}

		if (wifiState == WIFI_STATE_WIFI_READY && requestState == REQUEST_STATE_DISABLED) {
			LOG("Enabling HTTP requests" CRLF);
			requestState = REQUEST_STATE_ENABLED;
		}

		if (wifiState != WIFI_STATE_WIFI_READY && requestState != REQUEST_STATE_DISABLED) {
			LOG("Disabling HTTP requests" CRLF);
			requestState = REQUEST_STATE_DISABLED;
		}

		switch (requestState) {
			case REQUEST_STATE_SEND_REQUEST:
				if (sendf_esp8266_message_default(AT_COMMAND_CIPSTART, ENDPOINT_HOST)) {
					requestState = REQUEST_STATE_CONNECTING;
				}
				break;
			case REQUEST_STATE_SET_PAYLOAD_LENGTH:
				if (sendf_esp8266_message_default(AT_COMMAND_CIPSEND, requestStateMachine.requestLength)) {
					requestState = REQUEST_STATE_REQUESTING;
				}
				break;
			case REQUEST_STATE_SEND_PAYLOAD:
				if (send_esp8266_raw(requestPayload, requestStateMachine.requestLength)) {
					requestState = REQUEST_STATE_RECEIVING_RESPONSE;
				}
				break;
			default:
				break;
		}

		if (requestState != REQUEST_STATE_DISABLED &&
			now - requestStateMachine.lastHttpRequest > WIFI_WATCHDOG_TIMEOUT_MS) {
			LOG("HTTP request watchdog timeout" CRLF);
			requestStateMachine.lastHttpRequest = now;
			requestState = REQUEST_STATE_DISABLED;	// fail the request on watchdog timeout
			wifiState = WIFI_STATE_RESET;			// reset the wifi state on request watchdog timeout
		}

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure LSE Drive Capability
	 */
	HAL_PWR_EnableBkUpAccess();
	__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 16;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}

	/** Enable MSI Auto calibration
	 */
	HAL_RCCEx_EnableMSIPLLMode();
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {
	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x00B07CB4;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {
	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {
	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(STATUS_LED_OUT_GPIO_Port, STATUS_LED_OUT_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : STATUS_LED_OUT_Pin */
	GPIO_InitStruct.Pin = STATUS_LED_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(STATUS_LED_OUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : USER_TOGGLE_Pin */
	GPIO_InitStruct.Pin = USER_TOGGLE_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(USER_TOGGLE_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LD3_Pin */
	GPIO_InitStruct.Pin = LD3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
