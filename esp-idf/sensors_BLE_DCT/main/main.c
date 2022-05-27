//Task Creation at SERVICE START EVENT

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "rom/ets_sys.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_bt.h"

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
#define SAMPLE_DEVICE_NAME          "CoE_198"
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the gatt client write or prepare write, 
*  the data length must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX. 
*/
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 500
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

#define BUF_SIZE					(128)

//-- user variables --------------------------------------------------------------------------------------------------
uint8_t ble_data[BUF_SIZE];
char sensor_data[BUF_SIZE];

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

//Sensor Arrays
float pm_10_arr[32];
float pm_25_arr[32];
float temp_arr[32];
float hum_arr[32];
int arr_counter = 0;

//BLE
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
void printArray(float *arr, char *title)  // Array name Declared as a pointer
{
    printf("%s", title);
    for (int i = 0; i < 16; i++)
        printf("%f ", arr[i]);

    printf("\n");
}

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

    // vTaskDelete(NULL);
}

void send_notification(){


	while(1){
        init_DCT();

		//-- SDS011 Read -------------------------
		printf("=== Reading SDS ===\n" );
		//Wake Up SDS
		SDS_setwake();

		vTaskDelay(3000 / portTICK_RATE_MS);

		do{
			SDS_ret = readSDS();
			SDS_errorhandler(SDS_ret);

			pm_10 = get_pm_10();
			pm_25 = get_pm_25();
		}while((SDS_ret!=0) || (pm_10<pm_25));

		printf("PM10: %.2f\n", pm_10);
		printf("PM2.5: %.2f\n", pm_25);

		//Set SDS to sleep
		SDS_setsleep();

		//-- DHT22 Read ---------------------------
		printf("=== Reading DHT ===\n" );

		do{
			DHT_ret = readDHT();
			DHT_errorhandler(DHT_ret);

			if(DHT_ret!=0){
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
		}while(DHT_ret != 0);

		temp = getTemperature();
		hum = getHumidity();

		printf("Tmp %.1f\n", temp);
		printf("Hum %.1f\n", hum);

        //Store measured values to array
        pm_10_arr[arr_counter] = pm_10;
        pm_25_arr[arr_counter] = pm_25;
        temp_arr[arr_counter] = temp;
        hum_arr[arr_counter] = hum;

		arr_counter++;

        if (arr_counter == 16){
            init_DCT();

            DCT_mod(pm_10_arr);
            printArray(pm_10_arr, "PM10 Compressed: ");

            DCT_mod(pm_25_arr);
            printArray(pm_25_arr, "PM25 Compressed: ");

            DCT_mod(temp_arr);
            printArray(temp_arr, "TEMP Compressed: ");
            
            DCT_mod(hum_arr);
            printArray(hum_arr, "HUM Compressed: ");

            arr_counter = 0;
            // deinit_DCT();            

            for (int i = 0; i < 16; i++){
                sprintf(pm_10_str, "{\"pm10\":\"%.6f\",", pm_10_arr[i]);
                sprintf(pm_25_str, "\"pm2_5\":\"%.6f\",", pm_25_arr[i]);
                sprintf(temp_str, "\"temp\":\"%.6f\",", temp_arr[i]);
                sprintf(hum_str, "\"hum\":\"%.6f\"}", hum_arr[i]);

                strcpy(sensor_data, pm_10_str);
		        strcat(sensor_data, pm_25_str);
		        strcat(sensor_data, temp_str);
		        strcat(sensor_data, hum_str);

                memcpy(ble_data, sensor_data, BUF_SIZE);
                ESP_LOGI(TAG, "Generated String: %s", sensor_data);

                //check if client notification status	
                if(client_notify_stat == 1){
                    //send sensor data notification to client
                    esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data, false);
                    ESP_LOGI(GATTS_TABLE_TAG, "Notificaiton Sent!");
                }else if(client_notify_stat == 2){
                    //send sensor data indication to client
                    esp_ble_gatts_send_indicate(heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if, curr_conn_id, heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data, true);
                    ESP_LOGI(GATTS_TABLE_TAG, "Indicate Sent!");
                }else{
                    esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_VAL_A], BUF_SIZE, ble_data);
                    ESP_LOGI(GATTS_TABLE_TAG, "Table Updated!");
                }


                strcpy(save_string, sensor_data);

                save_sensor_data();
                // xTaskCreate(save_sensor_data, "SAVE_DATA", 3072, NULL, 5, NULL);
                vTaskDelay(2 * 1000 / portTICK_RATE_MS);

            }    

            // iDCT_mod(pm_10_arr);
            // printArray(pm_10_arr, "PM10 Decompressed: ");   

            // iDCT_mod(pm_25_arr);
            // printArray(pm_25_arr, "PM25 Decompressed: ");

            // iDCT_mod(temp_arr);
            // printArray(temp_arr, "TEMP Decompressed: ");

            // iDCT_mod(hum_arr);
            // printArray(hum_arr, "HUM Decompressed: ");

            deinit_DCT();
        }

    	//delay 3 seconds
		vTaskDelay(3000 / portTICK_RATE_MS);
	}
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
            /* send response when param->write.need_rsp is true*/
            if (param->write.need_rsp){
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
      	    break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
			xTaskCreate(send_notification, "BLE_NOTIF", 16000, NULL, 5, NULL);
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
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
			//disable client notification
            client_notify_stat = 0;
			esp_ble_gatts_set_attr_value(heart_rate_handle_table[IDX_CHAR_CFG_A], sizeof(disable_notif_cmd), &disable_notif_cmd);
			//re-start advertising
            esp_ble_gap_start_advertising(&adv_params);
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
