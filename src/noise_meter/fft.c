#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <pthread.h>
#include <math.h>

// sudo apt install libfftw3-dev
#include <fftw3.h>

#include "timing.h"
#include "lime.h"
#include "font/font.h"
#include "graphics.h"

/* Input from lime.c */
extern lime_fft_buffer_t lime_fft_buffer;

extern bool NewSettings;
extern bool NFMeter;
extern char mode[31];

//extern double bandwidth;

#define FFT_SIZE    256 //512 //2048
//#define FFT_TIME_SMOOTH 0.999f // 0.0 - 1.0
#define FFT_TIME_SMOOTH 0.96f // 0.0 - 1.0

extern float MAIN_SPECTRUM_TIME_SMOOTH; // 0.0 - 1.0

static float hanning_window_const[FFT_SIZE];
static float hamming_window_const[FFT_SIZE];
static float flattop_window_const[FFT_SIZE];

static fftwf_complex* fft_in;
static fftwf_complex* fft_out;
static fftwf_plan fft_plan;

static float fft_data_staging[FFT_SIZE];
static float fft_scaled_data[FFT_SIZE];
int y[515];
float rawpwr[260];

void main_fft_init(void)
{
    for(int i=0; i<FFT_SIZE; i++)
    {
        /* Hanning */
        // Approx bw/bins: 1dB/1, 3dB/1.5, 10dB/2.5, 20dB/3.4 30dB/4 Carrier 4 dB more than Flat Top
        hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((float)i)/FFT_SIZE)));

        /* Hamming */
        hamming_window_const[i] = 0.54 - (0.46 * cos(2*M_PI*(0.5+((2.0*((float)i/(FFT_SIZE-1))+1.0)/2))));

        /* Flat Top Constants from uk.mathworks.com*/
        // Approx bw/bins: 1dB/3, 3dB/4, 10dB/5, 20dB/7 30dB/8 Carrier 4 dB less than Hanning
        flattop_window_const[i] = 0.21557895
                                - 0.41663158 *  cos(2 * M_PI * (float)i / (FFT_SIZE - 1))
                                + 0.277263158 * cos(4 * M_PI * (float)i / (FFT_SIZE - 1))
                                - 0.083578947 * cos(6 * M_PI * (float)i / (FFT_SIZE - 1))
                                + 0.006947368 * cos(8 * M_PI * (float)i / (FFT_SIZE - 1));
    }

    /* Set up FFTW */
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * FFT_SIZE);
    fft_plan = fftwf_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_PATIENT);
    printf(" "); fftwf_print_plan(fft_plan); printf("\n");
}

static void fft_fftw_close(void)
{
    /* De-init fftw */
    fftwf_free(fft_in);
    fftwf_free(fft_out);
    fftwf_destroy_plan(fft_plan);
    fftwf_forget_wisdom();
}

/* FFT Thread */
void *fft_thread(void *arg)
{
    bool *exit_requested = (bool *)arg;

    int i, offset;
    fftw_complex pt;
    double pwr, lpwr;

    double pwr_scale = 1.0 / ((float)FFT_SIZE * (float)FFT_SIZE);

    struct timespec ts;

    /* Set pthread timer on .signal to use monotonic clock */
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init (&lime_fft_buffer.signal, &attr);
    pthread_condattr_destroy(&attr);

    while((false == *exit_requested)) 
    {
        /* Lock input buffer */
        pthread_mutex_lock(&lime_fft_buffer.mutex);

        while(lime_fft_buffer.index >= (lime_fft_buffer.size/(FFT_SIZE * sizeof(float) * 2))
            && false == *exit_requested)
        {
            /* Set timer for 100ms */
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_nsec += 10 * 1000000;

            pthread_cond_timedwait(&lime_fft_buffer.signal, &lime_fft_buffer.mutex, &ts);
        }

        if((*exit_requested))
        {
            break;
        }

        offset = lime_fft_buffer.index * FFT_SIZE * 2;

        /* Copy data out of rf buffer into fft_input buffer */

        if (strcmp(mode, "carrier") != 0)      // Normal, so use Hanning Window
        {
          for (i = 0; i < FFT_SIZE; i++)
          {
            fft_in[i][0] = (((float*)lime_fft_buffer.data)[offset+(2*i)]+0.00048828125) * hanning_window_const[i];
            fft_in[i][1] = (((float*)lime_fft_buffer.data)[offset+(2*i)+1]+0.00048828125) * hanning_window_const[i];
          }
        }
        else                                   // Carrier measurement, so use Flat Top Window
        {
          for (i = 0; i < FFT_SIZE; i++)
          {
            fft_in[i][0] = (((float*)lime_fft_buffer.data)[offset+(2*i)]+0.00048828125) * flattop_window_const[i];
            fft_in[i][1] = (((float*)lime_fft_buffer.data)[offset+(2*i)+1]+0.00048828125) * flattop_window_const[i];
          }
        }

        lime_fft_buffer.index++;

        /* Unlock input buffer */
        pthread_mutex_unlock(&lime_fft_buffer.mutex);

        /* Run FFT */
        fftwf_execute(fft_plan);

        //float int_max = -9999.0;
        //float int_min = 9999.0;

        for (i = 0; i < FFT_SIZE; i++)
        {
            /* shift, normalize and convert to dBFS */
            if (i < FFT_SIZE / 2)
            {
                pt[0] = fft_out[FFT_SIZE / 2 + i][0] / FFT_SIZE;
                pt[1] = fft_out[FFT_SIZE / 2 + i][1] / FFT_SIZE;
            }
            else
            {
                pt[0] = fft_out[i - FFT_SIZE / 2][0] / FFT_SIZE;
                pt[1] = fft_out[i - FFT_SIZE / 2][1] / FFT_SIZE;
            }
            pwr = pwr_scale * (pt[0] * pt[0]) + (pt[1] * pt[1]);
            rawpwr[i] = pwr;
            lpwr = 10.f * log10(pwr + 1.0e-20);

            fft_data_staging[i] = (lpwr * (1.f - FFT_TIME_SMOOTH)) + (fft_data_staging[i] * FFT_TIME_SMOOTH);
            //fft_data_staging[i] = lpwr;
            {
              // before scaling, fft_data_staging is in unit dBs

              // Set the scaling and vertical offset
              fft_scaled_data[i] = 5 * (fft_data_staging[i] + 100);  // So raise by 100 dB for display

              // At this point, 0 is equivalent to -80 dB
              // and 400 is equivalent to 0 dB
              // so 5 units per dB

              // Correct for the roll-off at the ends of the fft
              if (i < 23)
              {
                fft_scaled_data[i] = fft_scaled_data[i] + ((23 - i) * 2) / 5;
              }

              if (i > 233)
              {
                fft_scaled_data[i] = fft_scaled_data[i] + ((i - 233) * 2) / 5;
              }

    

              // Make sure that the data is within bounds for display
              if(fft_scaled_data[i] < 2) fft_scaled_data[i] = 2;
              if(fft_scaled_data[i] > 399) fft_scaled_data[i] = 399;

              // Convert to int
              y[i] = (((uint16_t)(fft_scaled_data[i])) / 2) + 200; // half height and raised
            }
        }
        //printf("Max: %f, Min %f\n", int_max, int_min);

    }

    fft_fftw_close();
    printf("fft Thread Closed\n");
    return NULL;
}

