
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"

#include "esp_dsp.h"
#include "dsp_platform.h"
#include "dsps_dct.h"
#include "dsps_fft2r.h"
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/event_groups.h"
#include "esp_event_loop.h"

static const char *TAG = "dsps_dct";
esp_err_t DCT_ret;

char temp[128];
char compressed_data[128];
char decompressed_data[128];

const int N = 16;

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

void init_DCT(){
    DCT_ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (DCT_ret  != ESP_OK) {
        ESP_LOGE(TAG, "Not possible to initialize FFT. Error = %i", DCT_ret);
        return;
    }
}

void deinit_DCT(){
    dsps_fft2r_deinit_fc32();
}

const char* get_compressed_data()
{
    return compressed_data;
}

const char* get_decompressed_data()
{
    return decompressed_data;
}

void print_Array(float *arr, char *title)  // Array name Declared as a pointer
{
    printf("%s", title);
    for (int i = 0; i < 16; i++)
        printf("%f ", arr[i]);

    printf("\n");
}

// struct ind* trans[16];

void DCT_mod(float* in)
{
    /*INITIALIZATION OF VARIABLES*/
    // const int N = 16;
    // float* in = calloc(1024*2, sizeof(float));
    // float* out = calloc(1024*2, sizeof(float));
    struct ind* trans = calloc(16, sizeof(float));

    /*INSERT MEASURED VALUES TO ARRAY*/
    // for (int i = 0 ; i < N ; i++) {
    //     in[i] = refVal[i];
    // }    
    
    // ESP_LOGI(TAG, "Start Example.");

    // SANITY CHECK
    // print_DATA(in, N, "IN ");
    // ESP_LOGI(TAG, "DCT ~~~~~~");
    // print_Array(in, "IN Contents: ");
    // for (int i = 0; i < N; i++){
    //     ESP_LOGI(TAG, "%f ", in[i]);
    // }

    /*DCT*/
    //unsigned int start_main = xthal_get_ccount();
    DCT_ret = dsps_dct_f32(in, N);
    
    // multiply a(u) factor
    for (int i = 1; i < N; i++){
        in[i] = in[i]*(sqrt( (double) 2/N));
    }
    in[0] = in[0]*(sqrt( (double) 1/N ));
    //unsigned int end_main = xthal_get_ccount();

    // ESP_LOGI(TAG, "cycles of ref - %i", end_ref - start_ref);
    // ESP_LOGI(TAG, "cycles of main - %i", end_main - start_main);

    if (DCT_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Operation error = %i", DCT_ret);
    }

    //SANITY CHECK
    // print_DATA(in, N, "DCT ");
    // ESP_LOGI(TAG, "DCT ");
    // for (int i = 0; i < N; i++){
    //     ESP_LOGI(TAG, "%f", in[i]);
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
    // ESP_LOGI(TAG, "COMP ~~~~~~");
    // print_Array(in, "IN Compressed: ");
    // strcpy(compressed_data, "COMPRESSED: [");
    // ESP_LOGI(TAG, "COMPRESSED ");
    // for (int i = 0; i < N; i++){
    //     // ESP_LOGI(TAG, "%f", in[i]);
    //     sprintf(temp, "%.2f, ", in[i]);
    //     strcat(compressed_data, temp);
    // }
    // strcat(compressed_data, "]");

    // ESP_LOGI(TAG, "End Example.");

    
    // free(in);
    // free(out);
    free(trans);

}

void iDCT_mod(float* in)
{
    /*RECOVERY OF ORIGINAL DATA*/
    // remove a(u) factor
    for (int i = 1; i < N; i++){
        in[i] = in[i]/(sqrt( (double) 2/N));
    }
    in[0] = in[0]/(sqrt( (double) 1/N ));

    dsps_dct_inv_f32(in, N);

    for (int i = 0; i < N; i++){
        in[i] = in[i] / N * 2;
    }
    //SANITY CHECK
    // ESP_LOGI(TAG, "iDCT ~~~~~~");
    // print_Array(in, "IN Decompressed: ");
    // strcpy(decompressed_data, "DECOMPRESSED: [");
    // ESP_LOGI(TAG, "RECOVERED DATA");
    // for (int i = 0; i < N; i++){
    //     // ESP_LOGI(TAG, "%f", in[i] / N * 2);
    //     sprintf(temp, "%.2f, ", in[i] / N * 2);
    //     strcat(decompressed_data, temp);
    // }
    // strcat(decompressed_data, "]");

    
}