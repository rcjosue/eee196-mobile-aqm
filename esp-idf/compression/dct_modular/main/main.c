#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "esp_log.h"


#include "DHT.h"
#include "DCT_mod.h"
// #include "../src/DHT_c.c"

/* Time */
// time_t now = 0;
// static void obtain_time()
// {
// 	time(&now);
// 	sprintf(epoch_buf, "%ld", now);
// }


#define BUF_SIZE (180)
float temp = 0;
float hum = 0;
float refVal[BUF_SIZE];
static const char *TAG = "dsps_dct";


void DHT_task(void *pvParameter)
{
    setDHTgpio(18);
    ESP_LOGI(TAG, "Starting DHT Task\n\n");

    for(int i=0; i<16; i++)
    {
        ESP_LOGI(TAG, "=== Reading DHT ===\n");
        int ret = readDHT();

        errorHandler(ret);

        hum = getHumidity();
        temp = getTemperature();

        refVal[i] = temp;

        ESP_LOGI(TAG, "Hum: %.1f Tmp: %.1f\n", hum, temp);

        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(2000 / portTICK_RATE_MS);
    }

    // vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;

    //Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_INFO);

    // xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 5, NULL); 
    DHT_task(NULL);

    init_DCT();
    DCT_mod(refVal);
}
