/* 

	SDS011 PM sensor driver

*/

#ifndef SDS011_H_  
#define SDS011_H_

// == function prototypes =======================================

void 	SDS_errorhandler(int SDS_response);
float 	get_pm_10();
float 	get_pm_25();
void 	SDS_setwake();
void 	SDS_setsleep();
void 	SDS_setquery();
void 	SDS_setactive();
void 	SDS_querydata();
void 	initUART();
void 	initSDS();
int 	readSDS();

#endif
