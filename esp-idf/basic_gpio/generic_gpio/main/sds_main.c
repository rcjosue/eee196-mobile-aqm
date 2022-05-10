#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
 
#include "SDS011.h"
#include "SDS011.c"

void SDS_task(void *pvParameter)
{
    initSDS();
    ESP_LOGI(SDS_TAG, "Starting PMS Task\n\n");

    while (1)
    {
        ESP_LOGI(SDS_TAG, "=== Reading PMS ===\n");
		int SDS_ret = readSDS();
		SDS_errorhandler(SDS_ret);

		float pm_10 = get_pm_10();
		float pm_25 = get_pm_25();

		ESP_LOGI(SDS_TAG, "PM10: %.2f", pm_10);
		ESP_LOGI(SDS_TAG, "PM2.5: %.2f", pm_25);
		SDS_setsleep();
        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t SDS_ret = nvs_flash_init();
    if (SDS_ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        SDS_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(SDS_ret);

    esp_log_level_set("*", ESP_LOG_INFO);

    xTaskCreate(&SDS_task, "PMS_task", 2048, NULL, 5, NULL);
}