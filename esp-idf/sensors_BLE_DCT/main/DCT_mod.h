
#define BUF_SIZE (180)

void init_DCT();
void deinit_DCT();
void DCT_mod(float* refVal);
void iDCT_mod(float* in);
char get_compressed_data();
char get_decompressed_data();