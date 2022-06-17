/* 

	DHT22 temperature sensor driver

*/

#ifndef DHT22_H_  
#define DHT22_H_

// == function prototypes =======================================

void 	setDHTgpio(int gpio);
void 	DHT_errorhandler(int DHT_response);
int 	readDHT();
float 	getHumidity();
float 	getTemperature();
int 	getSignalLevel( int usTimeOut, bool state );

#endif
