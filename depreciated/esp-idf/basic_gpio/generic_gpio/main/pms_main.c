#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
 
#include "PMS.h"
#include "PMS.c"

void PMS_task(void *pvParameter)
{
    initPMS();
    ESP_LOGI(PMS_TAG, "Starting PMS Task\n\n");

    while (1)
    {
        ESP_LOGI(PMS_TAG, "=== Reading PMS ===\n");
        //int PMS_ret = readPMS();

		PMS_pm_10 = get_pm_10();
		PMS_pm_25 = get_pm_25();
		ESP_LOGI(PMS_TAG, "PM10: %.2f", PMS_pm_10);
		ESP_LOGI(PMS_TAG, "PM2.5: %.2f", PMS_pm_25);
		PMS_setSleep();
        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(10000 / portTICK_RATE_MS);
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t PMS_ret = nvs_flash_init();
    if (PMS_ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        PMS_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(PMS_ret);

    esp_log_level_set("*", ESP_LOG_INFO);

    xTaskCreate(&PMS_task, "PMS_task", 2048, NULL, 5, NULL);
}