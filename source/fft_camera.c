#include "fft_camera.h"
#include "app_sai.h"
#include "arm_math.h"
#include "consolas.h"

STRUCT_PACKED
typedef struct _rgb {
        uint8_t red: 2;
        uint8_t green: 3;
        uint8_t blue: 2;
} rgb_t;STRUCT_UNPACKED


typedef uint8_t pixel_t;

typedef struct {
    pixel_t  *pixel;
    uint16_t width;
    uint16_t height;
    uint32_t num_pixel;
} draw_data_t;

void data_gen_sine(float* sig, int sig_len);
void data_gen_linear(float* sig, int sig_len);
int data_draw_rand(draw_data_t * data);
int data_draw_frame(draw_data_t* data, int r, int g, int b);
int data_draw_series(draw_data_t* data, float* sig, int sig_len, int r, int g, int b);
int data_draw_text(draw_data_t* data, const char* text, int x, int y, int r, int g, int b);

extern float32_t do_fft(uint32_t sampleRate, uint32_t bitWidth, uint8_t *buffer, float32_t *fftData, float32_t *fftResult);

static float maxFreq;
static float ffData[2 * BUFFER_SIZE];
static float ffResult[BUFFER_SIZE];


// AT_NONCACHEABLE_SECTION_ALIGN(static uint16_t s_cameraFrameBuffer[CAMERA_FRAME_BUFFER_COUNT][CAMERA_VERTICAL_POINTS * CAMERA_HORIZONTAL_POINTS + 32u], FRAME_BUFFER_ALIGN);
static pixel_t s_cameraFrameBuffer[CAMERA_FRAME_BUFFER_COUNT][CAMERA_FRAME_BYTES + 32u];

static uint8_t s_cameraBufferFull[CAMERA_FRAME_BUFFER_COUNT];

void FFT_CAMERA_SubmitEmptyBuffer(void* instance, uint32_t buffer)
{
    for (int i = 0; i < CAMERA_FRAME_BUFFER_COUNT; i++) {
    	usb_echo("%x, %x\r\n", buffer, s_cameraFrameBuffer[i]);

        if (buffer == s_cameraFrameBuffer[i] + 32)
            s_cameraBufferFull[i] = 0;
    }
}

int FFT_CAMERA_GetFullBuffer(void* instance, uint32_t* buffer)
{
    for (int i = 0; i < CAMERA_FRAME_BUFFER_COUNT; i++) {
        if (s_cameraBufferFull[i]) {
            *buffer = s_cameraFrameBuffer[i] + 32; //?
            return 0;
        }
    }
    return -1;
}

int FFT_CAMERA_DoCapture(void* instance, uint8_t* signal, uint32_t signal_len)
{
    static draw_data_t data;
    data.width = CAMERA_HORIZONTAL_POINTS;
    data.height = CAMERA_VERTICAL_POINTS;
    data.num_pixel = data.width * data.height;

    int i = 0;
    pixel_t* frame = s_cameraFrameBuffer[i] + 32;
    data.pixel = frame;


    if (signal) {
        /* Compute the frequency of the data using FFT */
        maxFreq = do_fft(DEMO_AUDIO_SAMPLE_RATE, DEMO_AUDIO_BIT_WIDTH, signal, ffData, ffResult);
        data_draw_series(&data, ffResult, sizeof(ffResult)/sizeof(ffResult[0])/2, 0, 255, 0);

    }


    s_cameraBufferFull[i] = 1; // mark buffer complete
    return 0;
}



void find_max_min(const float* y, int len, float* max, float* min)
{
    *max = y[0];
    *min = y[0];
    for (int i = 0; i < len; i++) {
        if ((y[i]) > *max)
            *max = (y[i]);
        if ((y[i]) < *min)
            *min = (y[i]);
    }
}

void data_gen_sine(float* sig, int sig_len)
{
    for (int i = 0; i < sig_len; i++) {
        sig[i] = (float)(10) * (float)sin(2 * 3.14 * 10 * i);
    }
}

void data_gen_linear(float* sig, int sig_len)
{
    for (int i = 0; i < sig_len; i++) {
        sig[i] = 2 * i + 10;
    }
}

#define RGB_SET(rgb, r, g, b)       do { \
                                        rgb.red = r & 0x1f; \
                                        rgb.green = g & 0x3F; \
                                        rgb.blue = b & 0x1F; \
                                    } while (0);

void set_pixel(uint8_t* pixels, uint32_t pixel, uint8_t val)
{
    int px_byte = pixel / 8;
    int px_bit = pixel  % 8;
    pixels[px_byte] &= ~(1 << px_bit);
    if (val)
        pixels[px_byte] |= (1 << px_bit);
}


void set_rgb(draw_data_t * data, int pixel, int r, int g, int b, int smear)
{
    int num_pixel = data->width * data->height;
    if (pixel >= num_pixel) {
        printf("invalid pixel\n");
        return;
    }
    int val = r || g || b;
    set_pixel(data->pixel, pixel, val);
    if (smear > 0) {
        for (int i = 0; i < smear; i++) {
            if (pixel - i > 0) {     // left
                set_pixel(data->pixel, pixel - i, val);
            }
            if (pixel + i < num_pixel) {    // right
                set_pixel(data->pixel, pixel + i, val);
            }
            if (pixel - i * data->width > 0) {     // top
                set_pixel(data->pixel, pixel - i * data->width, val);
            }
            if (pixel + i * data->width < num_pixel) {    // bottom
                set_pixel(data->pixel, pixel + i * data->width, val);
            }
        }
    }
}

int data_draw_rand(draw_data_t * data)
{
    for (int i=0; i < data->num_pixel; i++)
    {
        set_rgb(data, i, rand() % 2, 0, 0, 0);
    }
    return 0;
}

int data_draw_frame(draw_data_t* data, int r, int g, int b)
{
    for (int i = 0; i < data->width; i++) {
        set_rgb(data, 0 * data->width + i, r, g, b, 0);
        set_rgb(data, (data->height-1) * data->width + i, r, g, b, 0);
    }

    for (int i = 0; i < data->height; i++) {
        set_rgb(data, i * data->width + 0, r, g, b, 0);
        set_rgb(data, i * data->width + data->width-1, r, g, b, 0);
    }
    return 0;
}

int data_draw_text2(draw_data_t* data, const char* text, int x, int y, int r, int g, int b)
{
    const char* ch = text;
    while ( *ch ) {
        FONT_CHAR_INFO char_info = consolas_16ptDescriptors[*ch - consolas_16ptFontInfo.startChar];
        const uint8_t* bitmap = &consolas_16ptBitmaps[char_info.offset];
        for (int h = 0; h < consolas_16ptFontInfo.heightPages * 8; h++) // next pixel line
        {
            for (int nBits = 0; nBits < char_info.widthBits; nBits++)
            {
                int pixel = (data->height - y) * data->width + x + data->width * h + nBits;
                if (*bitmap >> (nBits % 8) & 0x01)
                    set_rgb(data, pixel, r, g, b, 0);
                if (nBits % 7 == 0)
                    bitmap++;
            }
        }
        ch++;
    }
    return 0;
}

int data_draw_text(draw_data_t* data, const char* text, int x, int y, int r, int g, int b)
{
    const char* ch = text;
    int char_start = 0;
    while ( *ch ) {
        FONT_CHAR_INFO char_info = consolas_16ptDescriptors[*ch - consolas_16ptFontInfo.startChar];
        const uint8_t* bitmap = &consolas_16ptBitmaps[char_info.offset];
        for (int h = 0; h < 21; h++) // next pixel line
        {
            int nBytes = char_info.widthBits / 8;
            if (char_info.widthBits % 8)
                nBytes++;
            for (int byte = 0; byte < nBytes; byte++)
            {
                for (int bit = 7; bit >= 0; bit--)
                {
                    int pixel = (y) * data->width + x + data->width * h + byte * 8 + (7-bit) + char_start;
                    if ((*bitmap >> bit) & 0x01) {
                        set_rgb(data, pixel, r, g, b, 0);
                        //printf("X");
                    }
                    else {
                        //printf(" ");
                    }
                }
                bitmap++;
            }
            //printf("\n");
        }
        char_start += char_info.widthBits;
        char_start += consolas_16ptFontInfo.spacePixels;
        ch++;
    }
    return 0;
}

void data_clear(draw_data_t* data)
{
    memset(data->pixel, 0, CAMERA_FRAME_BYTES + 32u);
}

int data_draw_series(draw_data_t* data, float* sig, int sig_len, int r, int g, int b)
{
    const int smear = 0;

    float y_max, y_min;

    find_max_min(sig, sig_len, &y_max, &y_min);

    for (int i = 0; i < sig_len; i++) {
        // scale x axis from [0, len] to [0, PLOT_WIDTH]
        int xx = (i * (data->width-1)) / sig_len;
        // scale y axis from [ymin, ymax] to [0, PLOT_HEIGHT]
        int yy = ((sig[i] - y_min) * (data->height-1)) / (y_max - y_min);
        set_rgb(data, (data->height - yy) * data->width + xx, r, g, b, smear);
    }
    return 0;
}

