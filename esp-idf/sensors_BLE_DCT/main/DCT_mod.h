
#define BUF_SIZE (180)

void init_DCT();
void DCT_mod(float refVal[BUF_SIZE]);
char get_compressed_data();
char get_decompressed_data();