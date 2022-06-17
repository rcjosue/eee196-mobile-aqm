//Task Creation at SERVICE START EVENT

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_bt.h"
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "gatts_table_creat_demo.h"
#include "esp_gatt_common_api.h"

#include "DHT22.h"
#include "SDS011.h"
#include "DCT_mod.h"

#define GATTS_TABLE_TAG "GATTS_TABLE_DEMO"

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SAMPLE_DEVICE_NAME          "maqiBLE_3"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the gatt client write or prepare write, 
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX. 
*/
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

#define BUF_SIZE					(180)
#define DCT_N					    64

//LOC
#define LONGI			"\"long\":\"121.04664227522113\","
#define LATI			"\"lat\":\"14.553687875023439\"}}_"

//-- user variables --------------------------------------------------------------------------------------------------
uint8_t ble_data[180];
char sensor_data[180];

//For Testing
char ref_data[180];

char pm_10_stref[BUF_SIZE];
char pm_25_stref[BUF_SIZE];
char temp_stref[BUF_SIZE];
char hum_stref[BUF_SIZE];
clock_t start, end;
long double cpu_time_used;
float rmse, nrmse, psnr, mean_ko;
float maxVal, minVal;

bool is_readsensor_task_running = false;

static const char *TAG = "INTEGRATED";
esp_err_t ret;
esp_err_t err; //temporary lang

//GATT service table variables
uint16_t curr_conn_id;
uint16_t client_notify_stat;
uint8_t disable_notif_cmd = 0;

//Memory
nvs_handle save_data_handle;
nvs_handle load_data_handle;
size_t nvs_val_size = BUF_SIZE;

int write_counter = 0; // value will default to 0, if not set yet in NVS
char write_counter_str[BUF_SIZE] = "0";
char save_string[BUF_SIZE];

int read_counter = 0;
char read_counter_str[BUF_SIZE] = "0";

int max_read_counter = 0;
char max_read_counter_str[BUF_SIZE] = "0";

char read_counter_nvs_val[BUF_SIZE] = "0";

//SDS
int i = 0;
int SDS_ret = 0;
float pm_10 = 0;
float pm_25 = 0;

char pm_10_str[BUF_SIZE];
char pm_25_str[BUF_SIZE];

//DHT
int DHT_ret = 0;
float temp = 0;
float hum = 0;

char temp_str[BUF_SIZE];
char hum_str[BUF_SIZE];

//Data Arrays
char tm_arr[64][100] = {"1654654170","1654654230","1654654290","1654654350","1654654410","1654654470","1654654530","1654654590","1654654650","1654654710","1654654770","1654654830","1654654890","1654654950","1654655010","1654655070","1654655130","1654655190","1654655250","1654655310","1654655370","1654655430","1654655490","1654655550","1654655610","1654655670","1654655730","1654655790","1654655850","1654655910","1654655970","1654656030","1654656090","1654656150","1654656210","1654656270","1654656330","1654656390","1654656450","1654656510","1654656570","1654656630","1654656690","1654656750","1654656810","1654656870","1654656930","1654656990","1654657050","1654657110","1654657170","1654657230","1654657290","1654657350","1654657410","1654657470","1654657530","1654657590","1654657650","1654657710","1654657770","1654657830","1654657890","1654657950"};
float hum_arr[128] = {15.55000019,16.400000570000003,15.94999981,17.14999962,16.80000019,16.80000019,18.25,16.60000038,17.849999429999997,16.400000094999996,15.85000038,16.19999981,16.85000038,17.14999962,16.349999905,17.10000038,16.249999525,18.35000038,17.650000570000003,16.30000019,16.75,15.75,16.60000038,15.80000019,17.349999905000004,16.75,17.80000019,16.35000038,17.349999429999997,17.349999429999997,18.89999962,15.950000285000002,16.05000019,17.299999715,16.400000095,16.10000038,15.549999714999998,16.14999962,16.650000570000003,17.0,17.0,17.30000019,15.5,16.349999429999997,16.95000076,16.70000076,18.85000038,17.30000019,18.14999962,18.44999981,18.0,20.14999962,17.89999962,18.69999981,18.94999981,18.400000570000003,19.30000019,17.599999429999997,19.14999962,18.05000019,19.0,18.39999962,18.75,18.30000019};
float pm_10_arr[128]= {13.0,12.30000019,12.94999981,12.80000019,12.599999905,13.099999905,13.650000095,13.19999981,13.25,13.099999905,12.94999981,13.349999905,13.349999905,13.55000019,13.75,13.30000019,13.099999905,13.69999981,13.900000095,13.5,13.5,13.55000019,13.25,12.849999905,13.25,13.849999905,13.599999905,13.44999981,13.599999905,13.400000095,13.5,12.5,13.5,13.099999905,12.849999905,12.700000285000002,13.099999905,13.5,14.0,13.700000285000002,13.650000095,13.80000019,13.5,13.55000019,13.5,14.39999962,13.80000019,14.450000285000002,15.400000095,16.35000038,16.0,16.14999962,15.400000095,15.799999714999998,15.900000094999998,15.900000570000001,16.44999981,15.75,16.35000038,16.05000019,16.94999981,16.39999962,15.94999981,16.30000019};
float pm_25_arr[128] = {31.89999962,32.049999235,32.09999847,32.09999847,32.29999924,31.999999045000003,31.45000076,31.10000038,31.25,31.39999962,31.5,31.44999981,31.44999981,31.5,31.44999981,31.5,31.650000570000003,31.849999429999997,31.94999981,32.049999235,32.09999847,32.149999615,32.049999235,32.0,31.89999962,31.650000570000003,31.70000076,31.79999924,31.60000038,31.55000019,31.60000038,31.650000570000003,31.650000570000003,31.70000076,31.75,31.79999924,31.75,31.89999962,31.89999962,31.89999962,31.89999962,31.75,31.79999924,31.79999924,31.75,31.79999924,31.75,31.75,31.70000076,31.70000076,31.70000076,31.60000038,31.5,31.650000570000003,31.70000076,31.70000076,31.70000076,31.79999924,31.79999924,31.89999962,31.89999962,32.0,32.049999235,32.149999615};
float temp_arr[128] = {71.299999235,71.05000305,71.349998475,74.59999847,72.69999695,71.89999771000001,73.0,76.0,76.14999771000001,76.400001525,76.25,76.45000076,74.700000765,74.40000153,76.150001525,77.55000305,73.900001525,72.40000153,73.79999924,71.30000305,70.89999771000001,75.5,74.849998475,74.900001525,74.150001525,74.40000153,75.04999924,76.10000228999999,76.799999235,77.69999695,72.5,71.69999695,78.0,76.14999771000001,75.900001525,76.099998475,75.35000228999999,73.10000228999999,73.20000076,74.80000305,76.40000153,76.599998475,76.40000153,75.0,77.049999235,79.849998475,81.150001525,79.90000153,75.349998475,80.14999771000001,81.400001525,77.549999235,72.700000765,75.70000076,74.950000765,74.79999924,73.5,72.549999235,74.5,73.10000228999999,71.650001525,71.950000765,72.90000153,72.5};

float hum_ref[64] = {15.55000019,16.400000570000003,15.94999981,17.14999962,16.80000019,16.80000019,18.25,16.60000038,17.849999429999997,16.400000094999996,15.85000038,16.19999981,16.85000038,17.14999962,16.349999905,17.10000038,16.249999525,18.35000038,17.650000570000003,16.30000019,16.75,15.75,16.60000038,15.80000019,17.349999905000004,16.75,17.80000019,16.35000038,17.349999429999997,17.349999429999997,18.89999962,15.950000285000002,16.05000019,17.299999715,16.400000095,16.10000038,15.549999714999998,16.14999962,16.650000570000003,17.0,17.0,17.30000019,15.5,16.349999429999997,16.95000076,16.70000076,18.85000038,17.30000019,18.14999962,18.44999981,18.0,20.14999962,17.89999962,18.69999981,18.94999981,18.400000570000003,19.30000019,17.599999429999997,19.14999962,18.05000019,19.0,18.39999962,18.75,18.30000019};
float pm_10_ref[64]= {13.0,12.30000019,12.94999981,12.80000019,12.599999905,13.099999905,13.650000095,13.19999981,13.25,13.099999905,12.94999981,13.349999905,13.349999905,13.55000019,13.75,13.30000019,13.099999905,13.69999981,13.900000095,13.5,13.5,13.55000019,13.25,12.849999905,13.25,13.849999905,13.599999905,13.44999981,13.599999905,13.400000095,13.5,12.5,13.5,13.099999905,12.849999905,12.700000285000002,13.099999905,13.5,14.0,13.700000285000002,13.650000095,13.80000019,13.5,13.55000019,13.5,14.39999962,13.80000019,14.450000285000002,15.400000095,16.35000038,16.0,16.14999962,15.400000095,15.799999714999998,15.900000094999998,15.900000570000001,16.44999981,15.75,16.35000038,16.05000019,16.94999981,16.39999962,15.94999981,16.30000019};
float pm_25_ref[64] = {31.89999962,32.049999235,32.09999847,32.09999847,32.29999924,31.999999045000003,31.45000076,31.10000038,31.25,31.39999962,31.5,31.44999981,31.44999981,31.5,31.44999981,31.5,31.650000570000003,31.849999429999997,31.94999981,32.049999235,32.09999847,32.149999615,32.049999235,32.0,31.89999962,31.650000570000003,31.70000076,31.79999924,31.60000038,31.55000019,31.60000038,31.650000570000003,31.650000570000003,31.70000076,31.75,31.79999924,31.75,31.89999962,31.89999962,31.89999962,31.89999962,31.75,31.79999924,31.79999924,31.75,31.79999924,31.75,31.75,31.70000076,31.70000076,31.70000076,31.60000038,31.5,31.650000570000003,31.70000076,31.70000076,31.70000076,31.79999924,31.79999924,31.89999962,31.89999962,32.0,32.049999235,32.149999615};
float temp_ref[64] = {71.299999235,71.05000305,71.349998475,74.59999847,72.69999695,71.89999771000001,73.0,76.0,76.14999771000001,76.400001525,76.25,76.45000076,74.700000765,74.40000153,76.150001525,77.55000305,73.900001525,72.40000153,73.79999924,71.30000305,70.89999771000001,75.5,74.849998475,74.900001525,74.150001525,74.40000153,75.04999924,76.10000228999999,76.799999235,77.69999695,72.5,71.69999695,78.0,76.14999771000001,75.900001525,76.099998475,75.35000228999999,73.10000228999999,73.20000076,74.80000305,76.40000153,76.599998475,76.40000153,75.0,77.049999235,79.849998475,81.150001525,79.90000153,75.349998475,80.14999771000001,81.400001525,77.549999235,72.700000765,75.70000076,74.950000765,74.79999924,73.5,72.549999235,74.5,73.10000228999999,71.650001525,71.950000765,72.90000153,72.5};
int arr_counter = 0;

//TIME
time_t now = 0;

struct tm timeinfo = { 0 };
struct timeval cur_timeval = { 0 };
struct timezone cur_timezone = { 0 };

char tm_str[BUF_SIZE];
char epoch_buf[BUF_SIZE];

int ble_epoch_time_update = 0;

//Location
char longi_str[BUF_SIZE] = LONGI;
char lati_str[BUF_SIZE] = LATI;

//BLE
bool is_ble_connected = false;

uint8_t ble_notify_saved_data[BUF_SIZE];

char ble_epoch_str[BUF_SIZE];

static uint8_t adv_config_done       = 0;

uint16_t heart_rate_handle_table[HRS_IDX_NB];

static uint8_t service_uuid[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

/* The length of adv data must be less than 31 bytes */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //&test_manufacturer[0],
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 16,
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
					esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/* Service */
static const uint16_t GATTS_SERVICE_UUID_TEST      = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_TEST_A       = 0xFF01;
static const uint16_t GATTS_CHAR_UUID_TEST_B       = 0xFF02;
static const uint16_t GATTS_CHAR_UUID_TEST_C       = 0xFF03;
static const uint16_t GATTS_CHAR_UUID_TEST_D       = 0xFF04;

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
// static const uint8_t char_prop_read                =  ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify         = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
// static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t heart_measurement_ccc[2]      = {0x00, 0x00};
static const uint8_t char_value[4]                 = {0x00, 0x00, 0x00, 0x00};

//-- functions -------------------------------------------------------------------------------------------------------
//TESTING
float rmsValue (float arr1[], float arr2[]){
    float square = 0.0;
    float mean = 0.0, root = 0.0;
    mean_ko = 0.0;
    maxVal = minVal = arr1[0];

    //Calculate Square and Sum of Orig and MinMaxVal
    for (int i = 0; i < 16; i++){
        square += pow((double)(arr1[i]-arr2[i]),2);

        mean_ko += arr1[i];

        if (arr1[i] < minVal) { minVal = arr1[i]; } 
        if (arr1[i] > maxVal) { maxVal = arr1[i]; }
    }

    //Calculate Mean
    mean = (square / (float)(16));

    //Mean of Orig
    mean_ko /= (float)(16);

    //Calculate Root
    root = sqrt(mean);

    return root;
}

//TIME
static void obtain_time(int i)
{
	time(&now);
	sprintf(epoch_buf, "%ld", now);
    sprintf(tm_arr[i], "%ld", now);
}

void printArray(float *arr, char *title)  // Array name Declared as a pointer
{
    printf("%s", title);
    for (int i = 0; i < DCT_N; i++)
        printf("%f ", arr[i]);

    printf("\n");
}

void save_sensor_data(){
	//ESP_LOGI(TAG, "String to Save: %s", save_string);

	//Save String to Memory
	//ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle");

	//Open NVS Memory
	err = nvs_open("storage", NVS_READWRITE, &save_data_handle);

	if (err!=ESP_OK){
	    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
	}else{
		//ESP_LOGI(TAG, "Done");

		//Get Write Counter from NVS
		//ESP_LOGI(TAG, "Getting counter value from NVS");

		nvs_val_size = BUF_SIZE;
		err = nvs_get_str(save_data_handle, "counter", write_counter_str, &nvs_val_size);

		switch (err) {
			case ESP_OK:
				//ESP_LOGI(TAG, "Done");
				ESP_LOGI(TAG, "Got memory block number = %s", write_counter_str);
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGE(TAG, "The value is not initialized yet!");
				break;
			default :
				ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
		}

		sscanf(write_counter_str, "%d", &write_counter);

		if(write_counter >= 128){
			ESP_LOGI(TAG, "Max memory block is reached, resetting write counter to 0");
			write_counter = 0;	//Reset Write Counter if block memory is 100
			sprintf(write_counter_str, "%d", write_counter); //convert write_counter to string
		}

		//Erase Data in Block and Write Complete Sensor Data
		//ESP_LOGI(TAG, "Committing updates in NVS block memory: %s", write_counter_str);
		err = nvs_set_str(save_data_handle, write_counter_str, save_string);
		err = nvs_commit(save_data_handle);

		if(err!=ESP_OK){
			ESP_LOGI(TAG, "Sensor Data Save to NVS Failed");
		}else{
			ESP_LOGI(TAG, "Sensor Data Save to NVS Success");
		}

		//Update Write Counter and Save to NVS
		//ESP_LOGI(TAG, "Updating nvs block memory counter");
		write_counter++;
		sprintf(write_counter_str, "%d", write_counter); //convert write_counter to string

		//ESP_LOGI(TAG, "Next NVS block memory: %s", write_counter_str);
		err = nvs_set_str(save_data_handle, "counter", write_counter_str);
		err = nvs_commit(save_data_handle);

		if(err!=ESP_OK){
			ESP_LOGI(TAG, "Updated counter save to NVS failed");
		}else{
			//ESP_LOGI(TAG, "Updated counter save to NVS success");
		}
		
		//Close NVS Handle
		nvs_close(save_data_handle);
	}

    // vTaskDelete(NULL);
}

void send_stored_data()
{
	ESP_LOGI(TAG, "Detected initial connection to mobile node, dumping all saved data...");

	//Open NVS Memory
	err = nvs_open("storage", NVS_READWRITE, &load_data_handle);

	if (err!=ESP_OK){
		ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
	}else{
		ESP_LOGI(TAG, "Done");

		//Get Read Counter from NVS
		ESP_LOGI(TAG, "Getting max read counter value from NVS");

		nvs_val_size = BUF_SIZE;
		err = nvs_get_str(load_data_handle, "counter", max_read_counter_str, &nvs_val_size);

		switch (err) {
			case ESP_OK:
				ESP_LOGI(TAG, "Done");
				ESP_LOGI(TAG, "Got maximum read memory block number = %s", max_read_counter_str);
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGE(TAG, "The value is not initialized yet!");
				break;
			default :
				ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
		}

		sscanf(max_read_counter_str, "%d", &max_read_counter);

		//Restart Write Count to Zero
		ESP_LOGI(TAG, "Restarting write counter to 0");
		err = nvs_set_str(load_data_handle, "counter", "0");
		err = nvs_commit(load_data_handle);

		if(err!=ESP_OK){
			ESP_LOGI(TAG, "Save Failed");
		}else{
			ESP_LOGI(TAG, "Save Success");
		}

		//Start Getting all stored data from memory and upload via mesh
        start = clock();
        for(read_counter=0; read_counter<max_read_counter; read_counter++){
			sprintf(read_counter_str, "%d", read_counter); //convert read_counter to string

	    	ESP_LOGI(TAG, "Reading string from NVS key: %s", read_counter_str);

			nvs_val_size = BUF_SIZE;
			err = nvs_get_str(load_data_handle, read_counter_str, read_counter_nvs_val, &nvs_val_size);

			switch (err) {
				case ESP_OK:
					ESP_LOGI(TAG, "Done");
					ESP_LOGI(TAG, "Read value = %s", read_counter_nvs_val);

					//Send Stored MQTT String to Mobile via Notification
					memcpy(ble_notify_saved_data, read_counter_nvs_val, BUF_SIZE);
					esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_notify_saved_data, false);

					ESP_LOGI(TAG, "Notification Sent!");

					break;
				case ESP_ERR_NVS_NOT_FOUND:
					ESP_LOGE(TAG, "The value is not initialized yet!");
					break;
				default :
					ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
			}
            // vTaskDelay(1000 / portTICK_RATE_MS);
		}
        
        end = clock();
        cpu_time_used = ((long double) (end - start)) / CLOCKS_PER_SEC;
        printf("~~~~~~~~TIME TO DUMP: %Lf\n", cpu_time_used);

		nvs_close(load_data_handle);
	}

    vTaskDelete(NULL);
}

void send_notification(){

    is_readsensor_task_running = true;

	while(1){
        
        // init_DCT();
		// //-- SDS011 Read -------------------------
		// printf("=== Reading SDS ===\n" );
		// //Wake Up SDS
		// SDS_setwake();

		// vTaskDelay(3000 / portTICK_RATE_MS);

		// do{
		// 	SDS_ret = readSDS();
		// 	SDS_errorhandler(SDS_ret);

		// 	pm_10 = get_pm_10();
		// 	pm_25 = get_pm_25();
		// }while((SDS_ret!=0) || (pm_10<pm_25));

		// printf("PM10: %.2f\n", pm_10);
		// printf("PM2.5: %.2f\n", pm_25);

		// //Set SDS to sleep
		// SDS_setsleep();

		// //-- DHT22 Read ---------------------------
		// printf("=== Reading DHT ===\n" );

		// do{
		// 	DHT_ret = readDHT();
		// 	DHT_errorhandler(DHT_ret);

		// 	if(DHT_ret!=0){
		// 		vTaskDelay(1000 / portTICK_RATE_MS);
		// 	}
		// }while(DHT_ret != 0);

		// temp = getTemperature();
		// hum = getHumidity();

		// printf("Tmp %.1f\n", temp);
		// printf("Hum %.1f\n", hum);

        // obtain_time(arr_counter);
        // printf("Time: %s\n", epoch_buf);
        // //Store measured values to array
        // tm_arr[arr_counter] =  *epoch_buf;
        // pm_10_ref[arr_counter] = pm_10_arr[arr_counter] = pm_10;
        // pm_25_ref[arr_counter] = pm_25_arr[arr_counter] = pm_25;
        // temp_ref[arr_counter] = temp_arr[arr_counter] = temp;
        // hum_ref[arr_counter] = hum_arr[arr_counter] = hum;
		
        //arr_counter++;

        if (1){
            // start = clock();
            init_DCT();
            
            DCT_mod(pm_10_arr);
            printArray(pm_10_arr, "PM10 Compressed: ");

            DCT_mod(pm_25_arr);
            printArray(pm_25_arr, "PM25 Compressed: ");

            DCT_mod(temp_arr);
            printArray(temp_arr, "TEMP Compressed: ");
            
            DCT_mod(hum_arr);
            printArray(hum_arr, "HUM Compressed: ");

            // deinit_DCT(); 
            // end = clock();
            // cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            // printf("~~~~~~~~TIME TO COMP: %f\n", cpu_time_used);

            arr_counter = 0;           

            for (int i = 0; i < DCT_N; i++){
                start = clock();
                sprintf(tm_str, "{\"ts\":%s000,", tm_arr[i]);

                if (fabs(pm_10_arr[i]) < 0.00001){
                    sprintf(pm_10_str, "\"values\":{\"pm10\":\"\",");
                } else { sprintf(pm_10_str, "\"values\":{\"pm10\":\"%g\",", pm_10_arr[i]); }
                if (fabs(pm_25_arr[i]) < 0.00001){
                    sprintf(pm_25_str, "\"pm25\":\"\",");
                } else { sprintf(pm_25_str, "\"pm25\":\"%g\",", pm_25_arr[i]); }
                if (fabs(temp_arr[i]) < 0.00001){
                    sprintf(temp_str, "\"temp\":\"\",");
                } else { sprintf(temp_str, "\"temp\":\"%g\",", temp_arr[i]); }
                if (fabs(hum_arr[i]) < 0.00001){
                    sprintf(hum_str, "\"hum\":\"\",");
                } else { sprintf(hum_str, "\"hum\":\"%g\",", hum_arr[i]); }
                
                strcpy(sensor_data, tm_str);
                strcat(sensor_data, pm_10_str);
		        strcat(sensor_data, pm_25_str);
		        strcat(sensor_data, temp_str);
		        strcat(sensor_data, hum_str);
                strcat(sensor_data, longi_str);
		        strcat(sensor_data, lati_str);

                //Testing
                //printArray(hum_ref, "HUM Raw: ");
                sprintf(pm_10_stref, "\"values\":{\"pm10\":\"%.2f\",", pm_10_ref[i]);
                sprintf(pm_25_stref, "\"pm25\":\"%.2f\",", pm_25_ref[i]);
                sprintf(temp_stref, "\"temp\":\"%.2f\",", temp_ref[i]);
                sprintf(hum_stref, "\"hum\":\"%.2f\",", hum_ref[i]);
                strcpy(ref_data, tm_str);
                strcat(ref_data, pm_10_stref);
		        strcat(ref_data, pm_25_stref);
		        strcat(ref_data, temp_stref);
		        strcat(ref_data, hum_stref);
                strcat(ref_data, longi_str);
		        strcat(ref_data, lati_str);

                //printf("%d,%d,%d\n", strlen(sensor_data), strlen(ref_data), strlen(ref_data)-strlen(sensor_data));
                ////ESP_LOGI(TAG, "Generated String: %s", sensor_data);
                ////ESP_LOGI(TAG, "Uncompres String: %s", ref_data);

               
                // if(is_ble_connected){
                //     ////ESP_LOGI(TAG, "BLE is connected!");
                //     memcpy(ble_data, sensor_data, 180);
                //     // memcpy(ble_data, ref_data, BUF_SIZE);
                   
                //     //check if client notification status	
                //     if(client_notify_stat == 1){
                //         //send sensor data notification to client
                //         esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data, false);
                //         //ESP_LOGI(GATTS_TABLE_TAG, "Notificaiton Sent!");
                //     }else if(client_notify_stat == 2){
                //         //send sensor data indication to client
                //         esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data, true);
                //         //ESP_LOGI(GATTS_TABLE_TAG, "Indicate Sent!");
                //     }else{
                //         esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data);
                //         ////ESP_LOGI(GATTS_TABLE_TAG, "Table Updated!");
                //     }
                // }
                //end = clock();
                //cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
                //printf("~~~~~~~~TIME TO SEND COMP: %Lf\n", cpu_time_used);

                strcpy(save_string, sensor_data);
                save_sensor_data();
                // xTaskCreate(save_sensor_data, "SAVE_DATA", 3072, NULL, 5, NULL);
                //Originally 2 secs
                //vTaskDelay(3 * 1000 / portTICK_RATE_MS);

            }    

            // init_DCT();
            iDCT_mod(pm_10_arr);
            printArray(pm_10_arr, "PM10 Decomp: ");
            // printArray(pm_10_ref, "PM10 Origin: ");

            iDCT_mod(pm_25_arr);
            printArray(pm_25_arr, "PM25 Decomp: ");
            // printArray(pm_25_ref, "PM25 Origin: ");

            iDCT_mod(temp_arr);
            printArray(temp_arr, "TEMP Decomp: ");
            // printArray(temp_ref, "TEMP Origin: ");

            iDCT_mod(hum_arr);
            printArray(hum_arr, "HUM Decomp: ");
            // printArray(hum_ref, "HUM Origin: ");

            deinit_DCT();

            //TESTING
            /*
             rmse = rmsValue(pm_10_ref, pm_10_arr);
             nrmse = rmse/mean_ko;
             psnr = 20*log10((maxVal - minVal) / rmse);
             printf("PM10 ---> RMSE: %f | NRMSE: %f  PSNR: %f\n", rmse, nrmse, psnr);

             rmse = rmsValue(pm_25_ref, pm_25_arr);
             nrmse = rmse/mean_ko;
             psnr = 20*log10((maxVal - minVal) / rmse);
             printf("PM25 ---> RMSE: %f | NRMSE: %f  PSNR: %f\n", rmse, nrmse, psnr);

             rmse = rmsValue(temp_ref, temp_arr);
             nrmse = rmse/mean_ko;
             psnr = 20*log10((maxVal - minVal) / rmse);
             printf("TEMP ---> RMSE: %f | NRMSE: %f  PSNR: %f\n", rmse, nrmse, psnr);

             rmse = rmsValue(hum_ref, hum_arr);
             nrmse = rmse/mean_ko;
             psnr = 20*log10((maxVal - minVal) / rmse);
             printf("HUM ---> RMSE: %f | NRMSE: %f  PSNR: %f\n", rmse, nrmse, psnr);

             vTaskDelay(15000 / portTICK_RATE_MS);
             */
        }

    	//delay 3 seconds
		vTaskDelay(3000 / portTICK_RATE_MS);
        vTaskDelete(NULL);
	}

    is_readsensor_task_running = false;
}

//TIME

void ble_update_system_time(void *arg)
{
	sscanf(ble_epoch_str, "%d", &ble_epoch_time_update);

	//Set Updated Timeval to Updated Epoch
	cur_timeval.tv_sec = ble_epoch_time_update;
	settimeofday(&cur_timeval, &cur_timezone);

	if(!is_readsensor_task_running){
		xTaskCreate(send_notification, "READ_SENSOR", 3072, NULL, 5, NULL);
	}

	vTaskDelete(NULL);
}

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
{
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}},

    /* PM, Temp. & Humidity Characteristic Declaration */
    [IDX_CHAR_A]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_A] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_A, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Client Characteristic Configuration Descriptor */
    [IDX_CHAR_CFG_A]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

    /* Time Characteristic Declaration */
    [IDX_CHAR_B]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_B]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_B, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Longitude Characteristic Declaration */
    [IDX_CHAR_C]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_C]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_C, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    /* Latitude Characteristic Declaration */
    [IDX_CHAR_D]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    /* Characteristic Value */
    [IDX_CHAR_VAL_D]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_D, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "advertising start failed");
            }else{
                ESP_LOGI(GATTS_TABLE_TAG, "advertising start successfully");
                start = clock();
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            end = clock();
            cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
            printf("~~~~~~~~TIME TO CONNECT: %Lf\n", cpu_time_used);
            break;
        default:
            break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:{
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            //config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;

            //config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_READ_EVT");
       	    break;
        case ESP_GATTS_WRITE_EVT:
            // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(GATTS_TABLE_TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
            esp_log_buffer_hex(GATTS_TABLE_TAG, param->write.value, param->write.len);
            if (heart_rate_handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2){
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001){
                    ESP_LOGI(GATTS_TABLE_TAG, "notify enable");
                	client_notify_stat = 1;
                    xTaskCreate(send_stored_data, "SEND_STORED", 3072, NULL, 5, NULL);
                }else if (descr_value == 0x0002){
                    ESP_LOGI(GATTS_TABLE_TAG, "indicate enable");
                	client_notify_stat = 2;
                }
                else if (descr_value == 0x0000){
                    ESP_LOGI(GATTS_TABLE_TAG, "notify/indicate disable ");
                	client_notify_stat = 0;
                }else{
                    ESP_LOGE(GATTS_TABLE_TAG, "unknown descr value");
                    esp_log_buffer_hex(GATTS_TABLE_TAG, param->write.value, param->write.len);
                }
            }
            
            if (heart_rate_handle_table[IDX_CHAR_VAL_D] == param->write.handle){
				memcpy(ble_epoch_str, param->write.value, param->write.len);
				xTaskCreate(ble_update_system_time, "BLE_TIME", 3072, NULL, 5, NULL);
			}
            
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
      	    break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            //// ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
			is_ble_connected = false;
            // xTaskCreate(send_notification, "BLE_NOTIF", 16000, NULL, 5, NULL);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
			curr_conn_id = param->connect.conn_id; //////////////////////////////////////////////////////////////////
            esp_log_buffer_hex(GATTS_TABLE_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            is_ble_connected = true;
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
			//disable client notification
            client_notify_stat = 0;
			esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_CFG_A], sizeof(disable_notif_cmd), &disable_notif_cmd);
			//re-start advertising
            esp_ble_gap_start_advertising(&adv_params);
            is_ble_connected = false;
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
                esp_ble_gatts_start_service(heart_rate_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
            is_ble_connected = false;
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATTS_TABLE_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[idx].gatts_if) {
                if (heart_rate_profile_tab[idx].gatts_cb) {
                    heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void ble_server_init(){
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(ESP_APP_ID);
    if (ret){
        ESP_LOGE(GATTS_TABLE_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(GATTS_TABLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}

void app_main()
{
	//Initializations --------------------------------------------------------------------
    //NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
	vTaskDelay( 1000 / portTICK_RATE_MS );

	//DHT22
	setDHTgpio(18);

	//SDS
	initUART();
	initSDS();

	//BLE
	ble_server_init();

    // init_DCT();
}
