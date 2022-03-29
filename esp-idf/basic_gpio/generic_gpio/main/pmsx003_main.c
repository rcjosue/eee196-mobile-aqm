/*
 * test_pmsx003.c
 *
 *  Created on: Sep 21, 2017
 *      Author: kris
 *
 *  This file is part of OpenAirProject-ESP32.
 *
 *  OpenAirProject-ESP32 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAirProject-ESP32 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAirProject-ESP32.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "pms.h"
#include "pms.c"
#include "../include/oap_test.h"

static pm_data_t last_result;

static int got_result = 0;
static void pms_test_callback(pm_data_t* result) {
	got_result = 1;
	memcpy(&last_result, result, sizeof(pm_data_t));
}

static pmsx003_config_t config = {
	.indoor = 1,
	.enabled = 1,
	.sensor_idx = 7,
	.callback = pms_test_callback,
	.set_pin = CONFIG_OAP_PM_SENSOR_CONTROL_PIN,
	.uart_num = CONFIG_OAP_PM_UART_NUM,
	.uart_txd_pin = CONFIG_OAP_PM_UART_TXD_PIN,
	.uart_rxd_pin = CONFIG_OAP_PM_UART_RXD_PIN,
	.uart_rts_pin = CONFIG_OAP_PM_UART_RTS_PIN,
	.uart_cts_pin = CONFIG_OAP_PM_UART_CTS_PIN
};

esp_err_t pms_init_uart(pmsx003_config_t* config);
void pms_init_gpio(pmsx003_config_t* config);
esp_err_t pms_uart_read(pmsx003_config_t* config, uint8_t data[32]);

static int uart_installed = 0;

void tearDown(void)
{
	test_reset_hw();
}

void PMS_task(void *pvParameter)
{
    pms_init_gpio(pmsx003_config_t* config);
    ESP_LOGI(TAG, "Starting PMS Task\n\n");

    while (1)
    {
        ESP_LOGI(TAG, "=== Reading PM ===\n");
        int ret = readDHT();

        errorHandler(ret);

        ESP_LOGI(TAG, "Hum: %.1f Tmp: %.1f\n", getHumidity(), getTemperature());

        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_INFO);

	pms_init_gpio(&config);

    xTaskCreate(&PMS_task, "DHT_task", 2048, NULL, 5, NULL);
}

TEST_CASE("pmsx003 measurement","pmsx003") {
	pms_init_gpio(&config);
	if (!uart_installed) {
		TEST_ESP_OK(pms_init_uart(&config));
		uart_installed = 1;
	}
	got_result = 0;
	uint8_t data[32];

	test_timer_t t = {
		.started = 0,
		.wait_for = 10000 //it takes a while to spin it up
	};

	TEST_ESP_OK(pmsx003_enable(&config, 1));
	while (!got_result && !test_timeout(&t)) {
		pms_uart_read(&config, data);
	}
	TEST_ESP_OK(pmsx003_enable(&config, 0));
	TEST_ASSERT_TRUE_MESSAGE(got_result, "timeout while waiting for measurement");

	TEST_ASSERT_EQUAL_INT(config.sensor_idx, last_result.sensor_idx);
	TEST_ASSERT_TRUE_MESSAGE(last_result.pm1_0 > 0, "no pm1.0");
	TEST_ASSERT_TRUE_MESSAGE(last_result.pm2_5 > 0, "no pm2.5");
	TEST_ASSERT_TRUE_MESSAGE(last_result.pm10 > 0, "no pm10");
}

