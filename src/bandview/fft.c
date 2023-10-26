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

// needs sudo apt install libfftw3-dev
#include <fftw3.h>

#include "timing.h"
#include "lime.h"
#include "font/font.h"
#include "graphics.h"

#define PI 3.14159265358979323846
#define FFT_SIZE 512

/* Input from lime.c */
extern lime_fft_buffer_t lime_fft_buffer;

extern bool Range20dB;
extern int BaseLine20dB;
extern int8_t BaselineShift;
extern float smoothing_factor; // 0.0 - 1.0
extern int32_t freqoffset;

static float hanning_window_const[FFT_SIZE];
static float hamming_window_const[FFT_SIZE];

static fftwf_complex* fft_in;
static fftwf_complex* fft_out;
static fftwf_plan fft_plan;

static float fft_data_staging[FFT_SIZE];
static float fft_scaled_data[FFT_SIZE];
int y[515];

void main_fft_init(void)
{
    for(int i = 0; i < FFT_SIZE; i++)
    {
        /* Hanning */
        hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((float)i)/FFT_SIZE)));

        /* Hamming */
        hamming_window_const[i] = 0.54 - (0.46 * cos(2*M_PI*(0.5+((2.0*((float)i/(FFT_SIZE-1))+1.0)/2))));
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
  int16_t y_buffer;

  double pwr_scale = 1.0 / ((float)FFT_SIZE * (float)FFT_SIZE);

  struct timespec ts;

  //uint64_t last_output = monotonic_ms();

  /* Set pthread timer on .signal to use monotonic clock */
  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
  pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  pthread_cond_init (&lime_fft_buffer.signal, &attr);
  pthread_condattr_destroy(&attr);

  while((false == *exit_requested))   // Go around once for each Lime input buffer fill?
  {
    /* Lock input buffer */
    pthread_mutex_lock(&lime_fft_buffer.mutex);

    while(lime_fft_buffer.index >= (lime_fft_buffer.size/(FFT_SIZE * sizeof(float) * 2)) && false == *exit_requested)
    {
      /* Set timer for 10ms */
      clock_gettime(CLOCK_MONOTONIC, &ts);
      ts.tv_nsec += 10 * 1000000;

      pthread_cond_timedwait(&lime_fft_buffer.signal, &lime_fft_buffer.mutex, &ts);
    }

    if((*exit_requested))
    {
      break;
    }

    offset = lime_fft_buffer.index * FFT_SIZE * 2;

    // Set up changing phase and frequency for cal signal (not used for normal operation)
    bool cal  = false;
    static float foffset = 2.55;
    static float phaseoffset  = 0;
    phaseoffset = phaseoffset + 0.0002;
    foffset = foffset + 0.000001;
    float amplitude = 0.1;  // Cal at -20 dBfs
    // lime_fft_buffer.data is in range -1.0 to + 1.0
 
    for (i = 0; i < FFT_SIZE; i++)
    {
      if (cal == false)
      {
        if (freqoffset >= 0) // Check if spectrum needs to be reversed
        {
          fft_in[i][0] = (((float*)lime_fft_buffer.data)[offset + (2 * i)    ] + 0.00048828125) * hanning_window_const[i];
          fft_in[i][1] = (((float*)lime_fft_buffer.data)[offset + (2 * i) + 1] + 0.00048828125) * hanning_window_const[i];
        }
        else                // Reverse Spectrum
        {
          fft_in[i][1] = (((float*)lime_fft_buffer.data)[offset + (2 * i)    ] + 0.00048828125) * hanning_window_const[i];
          fft_in[i][0] = (((float*)lime_fft_buffer.data)[offset + (2 * i) + 1] + 0.00048828125) * hanning_window_const[i];
        }
      }
      else
      {
        fft_in[i][0] = (float)(amplitude * sin((((float)i / foffset) + phaseoffset) * 2.f * PI)) * hanning_window_const[i];
        fft_in[i][1] = (float)(amplitude * cos((((float)i / foffset) + phaseoffset) * 2.f * PI)) * hanning_window_const[i];
      }
    }   
    lime_fft_buffer.index++;

    // Unlock input buffer
    pthread_mutex_unlock(&lime_fft_buffer.mutex);

    // Run FFT
    fftwf_execute(fft_plan);

    for (i = 0; i < FFT_SIZE; i++)
    {
      // shift, normalize and convert to dBFS
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
      pwr = pwr_scale * ((pt[0] * pt[0]) + (pt[1] * pt[1]));
      lpwr = 10.f * log10(pwr + 1.0e-20);

      fft_data_staging[i] = (lpwr * (1.f - smoothing_factor)) + (fft_data_staging[i] * smoothing_factor);

      // Set the scaling and vertical offset
      // 5 pixels per dB.  140 adjusts 0 dBFS to be 0.  baseline shift in dB
      fft_scaled_data[i] = 5 * ((fft_data_staging[i] + 140 ) + (float)BaselineShift);  

      // Correct for the roll-off at the ends of the fft
      if (i < 46)
      {
        fft_scaled_data[i] = fft_scaled_data[i] + ((46 - i) * 2) / 5;
      }
      else if (i > 466)
      {
        fft_scaled_data[i] = fft_scaled_data[i] + ((i - 466) * 2) / 5;
      }

      if (Range20dB) // Range20dB
      {
        fft_scaled_data[i] = fft_scaled_data[i] - 5 * (80 + BaseLine20dB);  
        fft_scaled_data[i] = 4 * fft_scaled_data[i];
      }

      // Convert to int
      y_buffer = (int16_t)(fft_scaled_data[i]);

      // Make sure that the data is within bounds
      if (y_buffer < 1)
      {
        y[i] = 1;
      }
      else if (y_buffer > 399)
      {
        y[i] = 399;
      }
      else
      {
        y[i] = (uint16_t)y_buffer;
      }
    }
    //printf("Max: %f, Min %f\n", int_max, int_min);
  }

  fft_fftw_close();
  printf("fft Thread Closed\n");
  return NULL;
}

