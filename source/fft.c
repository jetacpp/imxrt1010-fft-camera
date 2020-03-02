#include "arm_math.h"
#include "arm_const_structs.h"
#include "app_sai.h"



void gen_sine(uint8_t * in, uint32_t in_len, int fs)
{
    uint32_t count      = DEMO_AUDIO_SAMPLE_RATE / fs;
    uint32_t i          = 0;
    uint32_t val        = 0;
    uint32_t totalNum = 0, index = 0;

    /* Gnerate the sine wave data */
    for (i = 0; i < count; i++)
    {
        val            = arm_sin_q15(0x8000 * i / count);
        in[4 * i]      = val & 0xFFU;
        in[4 * i + 1U] = (val >> 8U) & 0xFFU;
        in[4 * i + 2U] = val & 0xFFU;
        in[4 * i + 3U] = (val >> 8U) & 0xFFU;
    }

    /* Repeat the cycle */
    for (i = 1; i < in_len / (4 * count); i ++)
    {
        memcpy(in + (i * 4 * count), in, 4 * count);
    }
}


float32_t do_fft(
    uint32_t sampleRate, uint32_t bitWidth, uint8_t *buffer, float32_t *fftData, float32_t *fftResult)
{
    /* Counter variable for navigating buffers */
    uint32_t counter;

    /* Return value for wav frequency in hertz */
    float32_t wavFreqHz;

    /* CMSIS status & FFT instance */
    arm_status status;                /* ARM status variable */
    arm_cfft_radix2_instance_f32 fft; /* ARM FFT instance */

    /* Frequency analysis variables */
    float32_t maxValue;           /* max value for greatest FFT bin amplitude */
    uint32_t testIndex       = 0; /* value for storing the bin location with maxValue */
    uint32_t complexBuffSize = BUFFER_SIZE * 2;
    uint32_t fftSize         = BUFFER_SIZE; /* FFT bin size */
    uint32_t ifftFlag        = 0;           /* Flag for the selection of CFFT/CIFFT */
    uint32_t doBitReverse    = 1;           /* Flag for selection of normal order or bit reversed order */
    float32_t hzPerBin       = 2 * ((float32_t)sampleRate / (float32_t)fftSize); /* Calculate hz per FFT bin */

    uint8_t *temp8; /* Point to data for 8 bit samples */
    uint8_t temp8Data;

    uint16_t *temp16; /* Point to data for 16 bit samples */
    int16_t temp16Data;

    uint32_t *temp32; /* Point to data for 32 bit samples */
    int32_t temp32Data;

    /* Set status as success */
    status = ARM_MATH_SUCCESS;

    /* Wav data variables */
    switch (bitWidth)
    {
        case 8:
            temp8     = (uint8_t *)buffer;
            temp8Data = 0;

            /* Copy wav data to fft input array */
            for (counter = 0; counter < complexBuffSize; counter++)
            {
                if (counter % 2 == 0)
                {
                    temp8Data        = (uint8_t)*temp8;
                    fftData[counter] = (float32_t)temp8Data;
                    temp8++;
                }
                else
                {
                    fftData[counter] = 0.0;
                }
            }

            /* Set instance for Real FFT */
            status = arm_cfft_radix2_init_f32(&fft, fftSize, ifftFlag, doBitReverse);

            /* Perform Real FFT on fftData */
            arm_cfft_radix2_f32(&fft, fftData);

            /* Populate FFT bins */
            arm_cmplx_mag_f32(fftData, fftResult, fftSize);

            /* Zero out non-audible, low-frequency noise from FFT Results. */
            fftResult[0] = 0.0;

            /* Find max bin and location of max (first half of bins as this is the only valid section) */
            arm_max_f32(fftResult, fftSize, &maxValue, &testIndex);

            break;

        case 16:
            temp16     = (uint16_t *)buffer;
            temp16Data = 0;

            /* Copy wav data to fft input array */
            for (counter = 0; counter < complexBuffSize; counter++)
            {
                if (counter % 2 == 0)
                {
                    temp16Data       = (int16_t)*temp16;
                    fftData[counter] = (float32_t)temp16Data;
                    temp16++;
                }
                else
                {
                    fftData[counter] = 0.0;
                }
            }

            /* Set instance for Real FFT */
            status = arm_cfft_radix2_init_f32(&fft, fftSize, ifftFlag, doBitReverse);

            /* Perform Real FFT on fftData */
            arm_cfft_radix2_f32(&fft, fftData);

            /* Populate FFT bins */
            arm_cmplx_mag_f32(fftData, fftResult, fftSize);

            /* Zero out non-audible, low-frequency noise from FFT Results. */
            fftResult[0] = 0.0;

            /* Find max bin and location of max (first half of bins as this is the only valid section) */
            arm_max_f32(fftResult, fftSize, &maxValue, &testIndex);

            break;

        case 32:
            temp32     = (uint32_t *)buffer;
            temp32Data = 0;

            /* Copy wav data to fft input array */
            for (counter = 0; counter < complexBuffSize; counter++)
            {
                if (counter % 2 == 0)
                {
                    temp32Data       = (int32_t)*temp32;
                    fftData[counter] = (float32_t)temp32Data;
                    temp32++;
                }
                else
                {
                    fftData[counter] = 0.0;
                }
            }

            /* Set instance for Real FFT */
            status = arm_cfft_radix2_init_f32(&fft, fftSize, ifftFlag, doBitReverse);

            /* Perform Real FFT on fftData */
            arm_cfft_radix2_f32(&fft, fftData);

            /* Populate FFT bins */
            arm_cmplx_mag_f32(fftData, fftResult, fftSize);

            /* Zero out non-audible, low-frequency noise from FFT Results. */
            fftResult[0] = 0.0;

            /* Find max bin and location of max (first half of bins as this is the only valid section) */
            arm_max_f32(fftResult, fftSize, &maxValue, &testIndex);

            break;

        default:
            __asm("NOP");
            break;
    }

    if (status != ARM_MATH_SUCCESS)
    {
        wavFreqHz = 0; /* If an error has occurred set frequency of wav data to 0Hz */
    }
    else
    {
        /* Set wavFreqHz to bin location of max amplitude multiplied by the hz per bin */
        wavFreqHz = testIndex * hzPerBin;
    }

    return wavFreqHz;
}
