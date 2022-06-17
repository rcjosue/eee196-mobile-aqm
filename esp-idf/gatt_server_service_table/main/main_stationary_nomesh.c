//MQTT Connect at ROOT_GOT_IP

//SNTP Start at ROOT_GOT_IP

//Mesh RX Task (esp_mesh_p2p_rx_main) Creation at Parent Connected

//Root Broadcast Time Task (root_broadcast_time) Creation when Child Connected (if Root)
//Mesh Update System Time Task (mesh_update_system_time) Creation at Mesh RX Task (if message is from root)
//BLE update system time task (ble_update_system_time) creation at write event if write to ff04

//Read & Upload Sensor Data Task (read_upload_sensor_data) Creation at BLE and Mesh Update System Time

//Save Sensor Data Task (save_sensor_data) Creation at Upload Sensor Data Task & Mesh Receive (Save after getting sensor values)

//Send stored data task (send_stored_data) creation at ble notification registration

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_bt.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "gatts_table_creat_demo.h"
#include "esp_gatt_common_api.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "DHT22.h"
#include "SDS011.h"

/*******************************************************
 *                Constants
 *******************************************************/
//MESH
#define RX_SIZE          (180)
#define TX_SIZE          (180)
#define BUF_SIZE		 (180)

#define MESH_ROUTER_SSID "Onezone"
#define MESH_ROUTER_PASSWD "abcd5678"
#define MESH_AP_PASSWD "COE198SENSOR"
#define MESH_AP_CONNECTIONS 5		//range:1-10
#define MESH_ROUTE_TABLE_SIZE 50	//range:1-300

//BLE
#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SAMPLE_DEVICE_NAME          "CoE_198_S"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the gatt client write or prepare write, 
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX. 
*/
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

//MQTT
#define BROKER_URI       	"ws://3.0.58.83/ws"
#define BROKER_USER			"5BxShf65Y0qYcUjm9Lhd"

//LOC
#define LONGITUDE			"\"longitude\":\"121.06866957\","
#define LATITUDE 			"\"latitude\":\"14.64951646\"}}-"

/*******************************************************
 *                Variable Definitions
 *******************************************************/
char sensor_data[BUF_SIZE];
bool is_readsensor_task_running = false;

esp_err_t err;

//MESH
static const char *TAG = "STATIONARY NODE";
static const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
static uint8_t root_tx_buf[TX_SIZE] = {0};
static uint8_t node_tx_buf[TX_SIZE] = {0};
char node_rx_buf[RX_SIZE] = {0};
static uint8_t rx_buf[RX_SIZE] = {0};
static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;

mesh_rx_pending_t pending_msg;

mesh_addr_t broadcast_group_id = {
		.addr = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff
		}
};

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

//MQTT
char root_rx_mqtt_data[RX_SIZE] = {0};
esp_mqtt_client_handle_t mqtt_client;

//SDS
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

//TIME
time_t root_now = 0;
time_t now = 0;

struct tm timeinfo = { 0 };
struct timeval cur_timeval = { 0 };
struct timezone cur_timezone = { 0 };

char timestamp_str[BUF_SIZE];
char root_epoch_buf[BUF_SIZE];
char epoch_buf[BUF_SIZE];
char node_rx_time_data[RX_SIZE] = {0};

int ble_epoch_time_update = 0;
int mesh_epoch_time_update = 0;

//Location
char longitude_str[BUF_SIZE] = LONGITUDE;
char latitude_str[BUF_SIZE] = LATITUDE;

//BLE
uint16_t curr_conn_id;
uint16_t client_notify_stat;
uint8_t disable_notif_cmd = 0;

bool is_ble_connected = false;

uint8_t ble_notify_sensor_data[BUF_SIZE];
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
static const uint8_t char_prop_read                = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify         = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t heart_measurement_ccc[2]      = {0x00, 0x00};
static const uint8_t char_value[4]                 = {0x00, 0x00, 0x00, 0x00};

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

 
/*******************************************************
 *                Function Definitions
 *******************************************************/
//TIME
static void obtain_time()
{
	time(&now);
	sprintf(epoch_buf, "%ld", now);
}

/*******************************************************
 *                Task Definitions
 *******************************************************/
//MEMORY
void save_sensor_data(){
	ESP_LOGI(TAG, "String to Save: %s", save_string);

	//Save String to Memory
	ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle");

	//Open NVS Memory
	err = nvs_open("storage", NVS_READWRITE, &save_data_handle);

	if (err!=ESP_OK){
	    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
	}else{
		ESP_LOGI(TAG, "Done");

		//Get Write Counter from NVS
		ESP_LOGI(TAG, "Getting counter value from NVS");

		nvs_val_size = BUF_SIZE;
		err = nvs_get_str(save_data_handle, "counter", write_counter_str, &nvs_val_size);

		switch (err) {
			case ESP_OK:
				ESP_LOGI(TAG, "Done");
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
		ESP_LOGI(TAG, "Committing updates in NVS block memory: %s", write_counter_str);
		err = nvs_set_str(save_data_handle, write_counter_str, save_string);
		err = nvs_commit(save_data_handle);

		if(err!=ESP_OK){
			ESP_LOGI(TAG, "Sensor Data Save to NVS Failed");
		}else{
			ESP_LOGI(TAG, "Sensor Data Save to NVS Success");
		}

		//Update Write Counter and Save to NVS
		ESP_LOGI(TAG, "Updating nvs block memory counter");
		write_counter++;
		sprintf(write_counter_str, "%d", write_counter); //convert write_counter to string

		ESP_LOGI(TAG, "Next NVS block memory: %s", write_counter_str);
		err = nvs_set_str(save_data_handle, "counter", write_counter_str);
		err = nvs_commit(save_data_handle);

		if(err!=ESP_OK){
			ESP_LOGI(TAG, "Updated counter save to NVS failed");
		}else{
			ESP_LOGI(TAG, "Updated counter save to NVS success");
		}
		
		//Close NVS Handle
		nvs_close(save_data_handle);
	}

    vTaskDelete(NULL);
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
		}

		nvs_close(load_data_handle);
	}

    vTaskDelete(NULL);
}

//Get & Upload Sensor Data (BLE & Mesh)
void read_upload_sensor_data(){
    esp_err_t err;
    mesh_data_t node_tx_data;
    node_tx_data.data = node_tx_buf;
    node_tx_data.size = sizeof(node_tx_buf);
    node_tx_data.proto = MESH_PROTO_BIN;

	is_readsensor_task_running = true;

	while(!is_mesh_connected || (is_mesh_connected && !esp_mesh_is_root())){
		//-- SDS011 Read -------------------------
		ESP_LOGI(TAG, "=== Reading SDS ===" );
		//Wake Up SDS
		SDS_setwake();

		vTaskDelay(3000 / portTICK_RATE_MS);

		do{
			if(is_mesh_connected && esp_mesh_is_root()){
				break;
			}

			SDS_ret = readSDS();
			SDS_errorhandler(SDS_ret);

			pm_10 = get_pm_10();
			pm_25 = get_pm_25();
		}while((SDS_ret!=0) || (pm_10<pm_25));

		ESP_LOGI(TAG, "PM10: %.2f", pm_10);
		ESP_LOGI(TAG, "PM2.5: %.2f", pm_25);

		//Set SDS to sleep
		SDS_setsleep();

		//-- DHT22 Read ---------------------------
		ESP_LOGI(TAG, "=== Reading DHT ===" );

		do{
			DHT_ret = readDHT();
			DHT_errorhandler(DHT_ret);

			if(DHT_ret!=0){
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
		}while(DHT_ret != 0);

		temp = getTemperature();
		hum = getHumidity();

		ESP_LOGI(TAG, "Tmp %.1f", temp);
		ESP_LOGI(TAG, "Hum %.1f", hum);

		//Get Time ------------------------------------------------------------------------------------
		obtain_time();

		//Construct Complete MQTT String
		sprintf(timestamp_str, "{\"ts\":%s000,", epoch_buf);
		sprintf(pm_10_str, "\"values\":{\"node\":\"1\",\"pm10\":\"%.2f\",", pm_10);
		sprintf(pm_25_str, "\"pm2_5\":\"%.2f\",", pm_25);
		sprintf(temp_str, "\"temperature\":\"%.2f\",", temp);
		sprintf(hum_str, "\"humidity\":\"%.2f\",", hum); 

		strcpy(sensor_data, timestamp_str);
		strcat(sensor_data, pm_10_str);
		strcat(sensor_data, pm_25_str);
		strcat(sensor_data, temp_str);
		strcat(sensor_data, hum_str);
		strcat(sensor_data, longitude_str);
		strcat(sensor_data, latitude_str);

    	ESP_LOGI(TAG, "Generated String: %s", sensor_data);

		if(is_ble_connected){
			ESP_LOGI(TAG, "BLE is connected!");

			memcpy(ble_notify_sensor_data, sensor_data, BUF_SIZE);

			//check if client notification status	
			if(client_notify_stat == 1){
				//send sensor data notification to client
				esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_notify_sensor_data, false);
				ESP_LOGI(TAG, "Notification Sent!");
			}else if(client_notify_stat == 2){
				//send sensor data indication to client
				esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_notify_sensor_data, true);
				ESP_LOGI(TAG, "Indicate Sent!");
			}else{
				esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_notify_sensor_data);
				ESP_LOGI(TAG, "Table Updated!");
			}


		//Save sensor data
		strcpy(save_string, sensor_data);
		xTaskCreate(save_sensor_data, "SAVE_DATA", 3072, NULL, 5, NULL);

		//delay 15 seconds
		vTaskDelay(15 * 1000 / portTICK_RATE_MS);
	}

	is_readsensor_task_running = false;

    vTaskDelete(NULL);
}

//TIME

void ble_update_system_time(void *arg)
{
	sscanf(ble_epoch_str, "%d", &ble_epoch_time_update);

	//Set Updated Timeval to Updated Epoch
	cur_timeval.tv_sec = ble_epoch_time_update;
	settimeofday(&cur_timeval, &cur_timezone);

	if(!is_readsensor_task_running){
		xTaskCreate(read_upload_sensor_data, "READ_SENSOR", 3072, NULL, 5, NULL);
	}

	vTaskDelete(NULL);
}


void root_broadcast_time(void *arg)
{
    esp_err_t err;
    mesh_data_t root_tx_data;
    root_tx_data.data = root_tx_buf;
    root_tx_data.size = sizeof(root_tx_buf);
    root_tx_data.proto = MESH_PROTO_BIN;

	vTaskDelay(3*1000 / portTICK_PERIOD_MS);

	//Get Current Time
	do{
		time(&root_now);
		sprintf(root_epoch_buf, "%ld", root_now);

		if(root_now<1500000000){
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}while(root_now<1500000000);

	//Copy Time String to root_tx_buf
	memcpy(root_tx_buf, root_epoch_buf, TX_SIZE);

	//Broadcast Time to All Nodes
    err = esp_mesh_send(&broadcast_group_id, &root_tx_data, MESH_DATA_P2P, NULL, 0);

    if (err) {
        ESP_LOGE(TAG, "Time Broadcast Failed!");
    }else{
		ESP_LOGI(TAG, "Time Broadcast Success!");
	}
	vTaskDelete(NULL);
}

/*******************************************************
 *                Event Handlers
 *******************************************************/
//BLE
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
                ESP_LOGE(TAG, "advertising start failed");
            }else{
                ESP_LOGI(TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(TAG, "Stop adv successfully");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
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
                ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            //config adv data
            esp_err_t err = esp_ble_gap_config_adv_data(&adv_data);
            if (err){
                ESP_LOGE(TAG, "config adv data failed, error code = %x", err);
            }
            adv_config_done |= ADV_CONFIG_FLAG;

            //config scan response data
            err = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (err){
                ESP_LOGE(TAG, "config scan response data failed, error code = %x", err);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
       	    break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_READ_EVT");
       	    break;
        case ESP_GATTS_WRITE_EVT:
            // the data length of gattc write must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);

            if (heart_rate_handle_table[IDX_CHAR_CFG_A] == param->write.handle && param->write.len == 2){
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001){
                    ESP_LOGI(TAG, "notify enable");
                	client_notify_stat = 1;
					xTaskCreate(send_stored_data, "SEND_STORED", 3072, NULL, 5, NULL);
                }else if (descr_value == 0x0002){
                    ESP_LOGI(TAG, "indicate enable");
                	client_notify_stat = 2;
                }
                else if (descr_value == 0x0000){
                    ESP_LOGI(TAG, "notify/indicate disable ");
                	client_notify_stat = 0;
                }else{
                    ESP_LOGE(TAG, "unknown descr value");
                    esp_log_buffer_hex(TAG, param->write.value, param->write.len);
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
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
			is_ble_connected = false;
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
			curr_conn_id = param->connect.conn_id; //////////////////////////////////////////////////////////////////
            esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
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
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
			//disable client notification
            client_notify_stat = 0;
			esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_CFG_A], sizeof(disable_notif_cmd), &disable_notif_cmd);
			//re-start advertising
            esp_ble_gap_start_advertising(&adv_params);
			is_ble_connected = false;
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != HRS_IDX_NB){
                ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_IDX_NB);
            }
            else {
                ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
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
            ESP_LOGE(TAG, "reg app failed, app_id %04x, status %d",
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

/*******************************************************
 *                Initializations
 *******************************************************/
//MQTT
void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = BROKER_URI,
        .event_handle = mqtt_event_handler,
        .port = 8083,
		.keepalive = 60
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
}

//NTP
static void ntp_init(void)
{
	// Set SNTP configurations
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
}

//BLE
void ble_server_init(){
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
    if (err) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    err = esp_bluedroid_init();
    if (err) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    err = esp_bluedroid_enable();
    if (err) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(err));
        return;
    }

    err = esp_ble_gatts_register_callback(gatts_event_handler);
    if (err){
        ESP_LOGE(TAG, "gatts register error, error code = %x", err);
        return;
    }

    err = esp_ble_gap_register_callback(gap_event_handler);
    if (err){
        ESP_LOGE(TAG, "gap register error, error code = %x", err);
        return;
    }

    err = esp_ble_gatts_app_register(ESP_APP_ID);
    if (err){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", err);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}
/*******************************************************
 *                Main
 *******************************************************/
void app_main(void)
{
	//Initializations---------------------------------------------------------
    //NVS
    ESP_LOGI(TAG, "Initializing Non-Volatile Storage");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
	vTaskDelay( 1000 / portTICK_RATE_MS );

	//MQTT
    ESP_LOGI(TAG, "Initializing MQTT");
	mqtt_init();
	
	//NTP
    ESP_LOGI(TAG, "Initializing NTP");
	ntp_init();

	//DHT22
    ESP_LOGI(TAG, "Initializing DHT22");
	setDHTgpio(18);

	//UART & SDS
    ESP_LOGI(TAG, "Initializing UART & SDS011");
	initUART();
	initSDS();

	//BLE
    ESP_LOGI(TAG, "Initializing BLE");
	ble_server_init();
}