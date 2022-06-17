/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"

#include "SDS011.h"

/**
 * This is an example which echos any data it receives on UART1 back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: UART1
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below
 */

//== SDS Definitions ========================================================================
#define ECHO_TEST_TXD  (17)
#define ECHO_TEST_RXD  (16)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE 				(128)
#define BAUD_RATE       		(9600)
#define PACKET_READ_TICS        (800 / portTICK_RATE_MS)
#define ECHO_UART_PORT          (UART_NUM_2)

#define SDS_OK 0
#define SDS_CHECKSUM_ERROR -1
#define SDS_READ_ERROR -2

#define MAXSDSDATA 10
#define MAXSDSCMD 19

//== SDS Global Variables ========================================================================
uart_config_t uart_config = {
	.baud_rate = BAUD_RATE,
	.data_bits = UART_DATA_8_BITS,
	.parity    = UART_PARITY_DISABLE,
	.stop_bits = UART_STOP_BITS_1,
	.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};

static const char* SDS_TAG = "SDS";

int SDS_i = 0;

uint8_t SDS_data[MAXSDSDATA];
int SDS_rxbytes = 0;

float SDS_pm_25 = 0;
float SDS_pm_10 = 0;

const char* SDS_wakecmd = "\xaa\xb4\x06\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x06\xab";
const char* SDS_sleepcmd = "\xaa\xb4\x06\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x05\xab";
const char* SDS_querymodecmd = "\xaa\xb4\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x02\xab";
const char* SDS_activemodecmd = "\xaa\xb4\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x01\xab";
const char* SDS_querydatacmd = "\xaa\xb4\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x02\xab";

uint8_t SDS_checksum;

//== SDS Functions ========================================================================
void SDS_errorhandler(int SDS_response)
{
	switch(SDS_response) {

		case SDS_CHECKSUM_ERROR:
			ESP_LOGE(SDS_TAG, "CheckSum Error\n");
			break;

		case SDS_READ_ERROR:
			ESP_LOGE(SDS_TAG, "Read Sensor Data Error\n");

		case SDS_OK:
			break;

		default :
			ESP_LOGE(SDS_TAG, "Unknown Error\n");
	}
}

float get_pm_10()
{
	return SDS_pm_10;
}

float get_pm_25()
{
	return SDS_pm_25;
}

void SDS_setwake()
{
	uart_write_bytes(ECHO_UART_PORT, SDS_wakecmd, MAXSDSCMD);
	SDS_rxbytes = uart_read_bytes(ECHO_UART_PORT, SDS_data, MAXSDSDATA, PACKET_READ_TICS);

	if (SDS_rxbytes!=10){
		ESP_LOGE(SDS_TAG, "Set Wake Command Error");
	}
}

void SDS_setsleep()
{
	uart_write_bytes(ECHO_UART_PORT, SDS_sleepcmd, MAXSDSCMD);
	SDS_rxbytes = uart_read_bytes(ECHO_UART_PORT, SDS_data, MAXSDSDATA, PACKET_READ_TICS);

	if (SDS_rxbytes!=10){
		ESP_LOGE(SDS_TAG, "Set Sleep Command Error");
	}
}

void SDS_setquery()
{
	uart_write_bytes(ECHO_UART_PORT, SDS_querymodecmd, MAXSDSCMD);
	SDS_rxbytes = uart_read_bytes(ECHO_UART_PORT, SDS_data, MAXSDSDATA, PACKET_READ_TICS);

	if (SDS_rxbytes!=10){
		ESP_LOGE(SDS_TAG, "Set Query Command Error");
	}
}

void SDS_setactive()
{
	uart_write_bytes(ECHO_UART_PORT, SDS_activemodecmd, MAXSDSCMD);
	SDS_rxbytes = uart_read_bytes(ECHO_UART_PORT, SDS_data, MAXSDSDATA, PACKET_READ_TICS);

	if (SDS_rxbytes!=10){
		ESP_LOGE(SDS_TAG, "Set Active Command Error");
	}
}

void SDS_querydata()
{
	uart_write_bytes(ECHO_UART_PORT, SDS_querydatacmd, MAXSDSCMD);
	SDS_rxbytes = uart_read_bytes(ECHO_UART_PORT, SDS_data, MAXSDSDATA, PACKET_READ_TICS);

	if (SDS_rxbytes!=10){
		ESP_LOGE(SDS_TAG, "Query Data Command Error");
	}
}

void initUART()
{
	uart_param_config(ECHO_UART_PORT, &uart_config);
	uart_set_pin(ECHO_UART_PORT, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
	uart_driver_install(ECHO_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
}

void initSDS()
{
	SDS_setwake();
	SDS_setquery();
}

int readSDS()
{
	//Flush Buffer
	uart_flush_input(ECHO_UART_PORT);

	//Send query data command
	SDS_querydata();

	if (SDS_rxbytes==10){
		// ESP_LOGI(SDS_TAG, "Received data from SDS011!");
		// ESP_LOG_BUFFER_HEXDUMP(SDS_TAG, SDS_data, SDS_rxbytes, ESP_LOG_INFO);	

		//Compute SDS_checksum
		SDS_checksum = SDS_data[2] + SDS_data[3] + SDS_data[4] + SDS_data[5] + SDS_data[6] + SDS_data[7];
		
		if (SDS_data[8] != SDS_checksum){
			return SDS_CHECKSUM_ERROR;
		}else{
			SDS_pm_10 = (SDS_data[5]<<8) + (SDS_data[4]/10);
			SDS_pm_25 = (SDS_data[3]<<8) + (SDS_data[2]/10);
			return SDS_OK;
		}
		
		//printf("%f\n",SDS_pm_10);
		//printf("%f\n",SDS_pm_25);

	}else{
		return SDS_READ_ERROR;
	}
}