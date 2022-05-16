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

#include "esp_dsp.h"
#include "dsp_platform.h"
#include "dsps_dct.h"
#include "dsps_fft2r.h"

#include "esp_log.h"
#include <math.h>

#include "DHT.h"
// #include "../src/DHT_c.c"

/* Time */
// time_t now = 0;
// static void obtain_time()
// {
// 	time(&now);
// 	sprintf(epoch_buf, "%ld", now);
// }

/* Data Structure to keep track of index for cepstral compression */
struct ind
{
    double value;
    int index;
};

int cmp(const void *a, const void *b)
{
    struct ind *a1 = (struct str *)a;
    struct ind *b1 = (struct str *)b;
    // return ( (*a1).value - (*b1).value );
    if ((*a1).value > (*b1).value)
        return -1;
    else if ((*a1).value < (*b1).value)
        return 1;
    else
        return 0;
}

double norm(const struct ind *arr, const int N)
{
    struct ind *arr_temp = arr;
    double ans = 0;
    for (int i = 0; i < N; i++){
        ans += ((*arr_temp).value) * ((*arr_temp).value);
        arr_temp++;
    }

    return sqrt(ans);

}

static const char *TAG = "dsps_dct";
#define BUF_SIZE (180)
float temp = 0;
float hum = 0;
float refVal[BUF_SIZE];

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

    ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", ret);
        return;
    }

    /*INITIALIZATION OF VARIABLES*/
    const int N = 16;
    float* in = calloc(1024*2, sizeof(float));
    float* out = calloc(1024*2, sizeof(float));
    struct ind* trans = calloc(1024*2, sizeof(float));

    // float refVal[18] = {7.1, 4.2,3.9,4.5,4.3,3.6,2,2.9,4.9,6.2,6.9,7.6,7,6.1,5.3,4.3,4,5.2};
    // float refVal[16] = {7.1, 4.2,3.9,4.5,4.3,3.6,2,2.9,4.9,6.2,6.9,7.6,7,6.1,5.3,4.3};

    /*INSERT MEASURED VALUES TO ARRAY*/
    for (int i = 0 ; i < N ; i++) {
        in[i] = refVal[i];
    }    
    
    ESP_LOGI(TAG, "Start Example.");

    // unsigned int start_ref = xthal_get_ccount();
    // dsps_dct_f32_ref(in, N, out);
    // unsigned int end_ref = xthal_get_ccount();

    /*DCT*/
    unsigned int start_main = xthal_get_ccount();
    ret = dsps_dct_f32(in, N);
    
    // multiply a(u) factor
    for (int i = 1; i < N; i++){
        in[i] = in[i]*(sqrt( (double) 2/N));
    }
    in[0] = in[0]*(sqrt( (double) 1/N ));
    unsigned int end_main = xthal_get_ccount();

    // ESP_LOGI(TAG, "cycles of ref - %i", end_ref - start_ref);
    // ESP_LOGI(TAG, "cycles of main - %i", end_main - start_main);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Operation error = %i", ret);
    }

    // //SANITY CHECK
    ESP_LOGI(TAG, "DCT ");
    for (int i = 0; i < N; i++){
        ESP_LOGI(TAG, "%f", in[i]);
    }
    // ESP_LOGI(TAG, "OUT contents: ");
    // for (int i = 0; i < N; i++){
    //     ESP_LOGI(TAG, "%f", out[i]);
    // }

    /*COMPRESSION*/ 
    // insert to a structure that keeps track of index
    for (int i = 0 ; i < N ; i++) {
        trans[i].value = fabs((double)in[i]);
        trans[i].index = i;
    }

    qsort(trans, N, sizeof(trans[0]), cmp);

    // energy calculation
    int coeffs = 0;
    while (norm(trans, coeffs)/norm(trans, N) < 0.9998){
        coeffs++;
    }

    // ESP_LOGI(TAG, "number of coeffs reqd: %i\n", coeffs);
    /*Cepstral Compression*/
    for (int i = coeffs; i < N; i++){
        // ESP_LOGI(TAG, "to make zero -> %f : \n", trans[i].value);
        in[trans[i].index] = 0;
    }

    //SANITY CHECK
    ESP_LOGI(TAG, "COMPRESSED ");
    for (int i = 0; i < N; i++){
        ESP_LOGI(TAG, "%f", in[i]);
    }

    /*RECOVERY OF ORIGINAL DATA*/
    // remove a(u) factor
    for (int i = 1; i < N; i++){
        in[i] = in[i]/(sqrt( (double) 2/N));
    }
    in[0] = in[0]/(sqrt( (double) 1/N ));

    dsps_dct_inv_f32(in, N);
    ESP_LOGI(TAG, "RECOVERED DATA");
    for (int i = 0; i < N; i++){
        ESP_LOGI(TAG, "%f", in[i] / N * 2);
    }

    ESP_LOGI(TAG, "End Example.");

    dsps_fft2r_deinit_fc32();
    free(in);
    free(out);
    free(trans);
 
    // //SANITY CHECK
    // ESP_LOGI(TAG, "DEFACTORED");
    // for (int i = 0; i < N; i++){
    //     ESP_LOGI(TAG, "defactored: %f", in[i]);
    // }
}

