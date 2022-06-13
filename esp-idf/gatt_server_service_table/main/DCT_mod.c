
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

struct ind trans[16];

void DCT_mod(float* in)
{
    /*INITIALIZATION OF VARIABLES*/
    // const int N = 16;
    // float* in = calloc(1024*2, sizeof(float));
    // float* out = calloc(1024*2, sizeof(float));
    // struct ind* trans = calloc(16, sizeof(float));

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

    /*Cepstral Compression*/
    for (int i = coeffs; i < N; i++){
        in[trans[i].index] = 0;
    }

    // free(trans);

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
    
}