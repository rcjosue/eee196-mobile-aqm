/*------------------------------------------------------------------------------

	DHT22 DHT_temperature & DHT_humidity sensor AM2302 (DHT22) driver for ESP32

	Jun 2017:	Ricardo Timmermann, new for DHT22  	

	Code Based on Adafruit Industries and Sam Johnston and Coffe & Beer. Please help
	to improve this code. 
	
	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.

	PLEASE KEEP THIS CODE IN LESS THAN 0XFF LINES. EACH LINE MAY CONTAIN ONE BUG !!!

---------------------------------------------------------------------------------*/

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "DHT22.h"

// == DHT definitions =============================================

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

//DHT variables

static const char* DHT_TAG = "DHT";

int DHTgpio = 4;				// my default DHT pin = 4
float DHT_humidity = 0.;
float DHT_temperature = 0.;

// == set the DHT used pin=========================================

void setDHTgpio( int gpio )
{
	DHTgpio = gpio;
}

// == get temp & hum =============================================

float getHumidity() { return DHT_humidity; }
float getTemperature() { return DHT_temperature; }

// == error handler ===============================================

void DHT_errorhandler(int DHT_response)
{
	switch(DHT_response) {
	
		case DHT_TIMEOUT_ERROR :
			ESP_LOGE( DHT_TAG, "Sensor Timeout" );
			break;

		case DHT_CHECKSUM_ERROR:
			ESP_LOGE( DHT_TAG, "CheckSum Error" );
			break;

		case DHT_OK:
			break;

		default :
			ESP_LOGE( DHT_TAG, "Unknown Error\n" );
	}
}

/*-------------------------------------------------------------------------------
;
;	get next state 
;
;	I don't like this logic. It needs some interrupt blocking / priority
;	to ensure it runs in realtime.
;
;--------------------------------------------------------------------------------*/

int getSignalLevel( int usTimeOut, bool state )
{

	int DHT_uSec = 0;
	while( gpio_get_level(DHTgpio)==state ) {

		if( DHT_uSec > usTimeOut ) 
			return -1;
		
		++DHT_uSec;
		ets_delay_us(1);		// DHT_uSec delay
	}
	
	return DHT_uSec;
}

/*----------------------------------------------------------------------------
;
;	read DHT22 sensor

copy/paste from AM2302/DHT22 Docu:

DATA: Hum = 16 bits, Temp = 16 Bits, check-sum = 8 Bits

Example: MCU has received 40 bits data from AM2302 as
0000 0010 1000 1100 0000 0001 0101 1111 1110 1110
16 bits RH data + 16 bits T data + check sum

1) we convert 16 bits RH data from binary system to decimal system, 0000 0010 1000 1100 → 652
Binary system Decimal system: RH=652/10=65.2%RH

2) we convert 16 bits T data from binary system to decimal system, 0000 0001 0101 1111 → 351
Binary system Decimal system: T=351/10=35.1°C

When highest bit of DHT_temperature is 1, it means the DHT_temperature is below 0 degree Celsius. 
Example: 1000 0000 0110 0101, T= minus 10.1°C: 16 bits T data

3) Check Sum=0000 0010+1000 1100+0000 0001+0101 1111=1110 1110 Check-sum=the last 8 bits of Sum=11101110

Signal & Timings:

The interval of whole process must be beyond 2 seconds.

To request data from DHT:

1) Sent low pulse for > 1~10 ms (MILI SEC)
2) Sent high pulse for > 20~40 us (Micros).
3) When DHT detects the start signal, it will pull low the bus 80us as response signal, 
   then the DHT pulls up 80us for preparation to send data.
4) When DHT is sending data to MCU, every bit's transmission begin with low-voltage-level that last 50us, 
   the following high-voltage-level signal's length decide the bit is "1" or "0".
	0: 26~28 us
	1: 70 us

;----------------------------------------------------------------------------*/

#define MAXDHTDATA 5	// to complete 40 = 5*8 Bits

int readDHT()
{
int DHT_uSec = 0;

uint8_t DHT_data[MAXDHTDATA];
uint8_t byteInx = 0;
uint8_t bitInx = 7;

	for (int DHT_i = 0; DHT_i<MAXDHTDATA; DHT_i++) 
		DHT_data[DHT_i] = 0;

	// == Send start signal to DHT sensor ===========

	gpio_set_direction( DHTgpio, GPIO_MODE_OUTPUT );

	// pull down for 3 ms for a smooth and nice wake up 
	gpio_set_level( DHTgpio, 0 );
	ets_delay_us( 3000 );			

	// pull up for 25 us for a gentile asking for data
	gpio_set_level( DHTgpio, 1 );
	ets_delay_us( 25 );

	gpio_set_direction( DHTgpio, GPIO_MODE_INPUT );		// change to input mode
  
	// == DHT will keep the line low for 80 us and then high for 80us ====

	DHT_uSec = getSignalLevel( 85, 0 );
//	ESP_LOGI( DHT_TAG, "Response = %d", DHT_uSec );
	if( DHT_uSec<0 ) return DHT_TIMEOUT_ERROR; 

	// -- 80us up ------------------------

	DHT_uSec = getSignalLevel( 85, 1 );
//	ESP_LOGI( DHT_TAG, "Response = %d", DHT_uSec );
	if( DHT_uSec<0 ) return DHT_TIMEOUT_ERROR;

	// == No errors, read the 40 data bits ================
  
	for( int k = 0; k < 40; k++ ) {

		// -- starts new data transmission with >50us low signal

		DHT_uSec = getSignalLevel( 56, 0 );
		if( DHT_uSec<0 ) return DHT_TIMEOUT_ERROR;

		// -- check to see if after >70us rx data is a 0 or a 1

		DHT_uSec = getSignalLevel( 75, 1 );
		if( DHT_uSec<0 ) return DHT_TIMEOUT_ERROR;

		// add the current read to the output data
		// since all DHT_data array where set to 0 at the start, 
		// only look for "1" (>28us us)
	
		if (DHT_uSec > 40) {
			DHT_data[ byteInx ] |= (1 << bitInx);
			}
	
		// index to next byte

		if (bitInx == 0) { bitInx = 7; ++byteInx; }
		else bitInx--;
	}

	// == get DHT_humidity from Data[0] and Data[1] ==========================

	DHT_humidity = DHT_data[0];
	DHT_humidity *= 0x100;					// >> 8
	DHT_humidity += DHT_data[1];
	DHT_humidity /= 10;						// get the decimal

	// == get temp from Data[2] and Data[3]
	
	DHT_temperature = DHT_data[2] & 0x7F;	
	DHT_temperature *= 0x100;				// >> 8
	DHT_temperature += DHT_data[3];
	DHT_temperature /= 10;

	if( DHT_data[2] & 0x80 ) 			// negative temp, brrr it's freezing
		DHT_temperature *= -1;


	// == verify if checksum is ok ===========================================
	// Checksum is the sum of Data 8 bits masked out 0xFF
	
	if (DHT_data[4] == ((DHT_data[0] + DHT_data[1] + DHT_data[2] + DHT_data[3]) & 0xFF)) 
		return DHT_OK;

	else 
		return DHT_CHECKSUM_ERROR;
}

