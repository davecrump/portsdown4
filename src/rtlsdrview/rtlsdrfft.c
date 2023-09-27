#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <fftw3.h>
#include <stdbool.h>

//#include "libairspy/libairspy/src/airspy.h"
#include "timing.h"

#include <rtl-sdr.h>

extern bool NewFreq;                 // Set to true to indicate that frequency needs changing
extern uint32_t CentreFreq;          // Frequency from main
extern bool NewSpan;                 // Set to true to indicate that span needs changing
extern bool prepnewscanwidth;
extern bool readyfornewscanwidth;

extern uint32_t SpanWidth;           // Sample rate from main
extern bool NewGain;                 // Set to true to indicate that gain needs changing
extern int gain;                     // Gain (20 - 50) from main 60 = auto
extern bool Range20dB;
extern int BaseLine20dB;
extern int fft_size;                 // Number of fft samples.  Depends on scan width
extern float fft_time_smooth; 

extern int span;      

int fft_offset;
int nearest_gain_value;

uint16_t y3[1250];               // Histogram values in range 0 - 399

int force_exit = 0;

pthread_t fftThread;

/* Sample rate */
uint32_t sample_rate_val;

/* DC Bias Tee -> 0 (disabled) */
uint32_t biast_val = 0;

/* Linear Gain */
#define LINEAR
uint32_t linearity_gain_val;   // MAX=21

/* Sensitive Gain */
//#define SENSITIVE
uint32_t sensitivity_gain_val = 10; // MAX=21

/* Frequency */
uint32_t freq_hz = 437000000;

double hanning_window_const[1250];

#define FLOAT32_EL_SIZE_BYTE (4)
fftw_complex* fft_in;
fftw_complex*   fft_out;
fftw_plan   fft_plan;

typedef struct 
{
  float data[1250];
  pthread_mutex_t mutex;
} fft_buffer_t;

fft_buffer_t fft_buffer = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

#define DEFAULT_SAMPLE_RATE 2048000
//#define DEFAULT_SAMPLE_RATE 1024000
static rtlsdr_dev_t *dev = NULL;

static uint8_t *buffer;
static uint32_t dev_index = 0;
static uint32_t samp_rate; // = DEFAULT_SAMPLE_RATE;
static uint32_t buff_len = 2048;
static int      ppm_error = 0;
static float    lut[256];       /* look-up table to convert U8 to +/- 1.0f */



pthread_mutex_t histogram;

static const char *fftw_wisdom_filename = ".fftw_wisdom";
static float fft_output_data[1250];

void setup_fft(void)
{
  int i;

  // Set up FFTW
  fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_size);
  fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_size);
  i = fftw_import_wisdom_from_filename(fftw_wisdom_filename);

  // Always calculate fft plan
  i = 0;
  if (i == 0)
  {
    printf("Computing fft plan...\n");
  }
  fft_plan = fftw_plan_dft_1d(fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_EXHAUSTIVE);

  if (i == 0)
  {
    fftw_export_wisdom_to_filename(fftw_wisdom_filename);
  }
}

static void close_fftw(void)
{
    /* De-init fftw */
    fftw_free(fft_in);
    fftw_free(fft_out);
    fftw_destroy_plan(fft_plan);
    fftw_forget_wisdom();
}

int nearest_gain(rtlsdr_dev_t *dev, int target_gain)
{
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	count = rtlsdr_get_tuner_gains(dev, NULL);
	if (count <= 0) {
		return 0;
	}
	gains = malloc(sizeof(int) * count);
	count = rtlsdr_get_tuner_gains(dev, gains);
	nearest = gains[0];
	for (i=0; i<count; i++) {
		err1 = abs(target_gain - nearest);
		err2 = abs(target_gain - gains[i]);
		if (err2 < err1) {
			nearest = gains[i];
		}
	}
	free(gains);
	return nearest;
}


static uint8_t setup_rtlsdr()
{
  int device_count;
  int r;

  // Allocate the buffer for samples
  printf("Allocating buffer of %d bytes\n", buff_len * sizeof(uint8_t));
  buffer = malloc(buff_len * sizeof(uint8_t));

  // Check for presences of RTL-SDR
  device_count = rtlsdr_get_device_count();
  if (!device_count)
  {
    printf("No supported devices found.\n");
    return 1;
  }

  r = rtlsdr_open(&dev, dev_index);
  if (r < 0)
  {
    printf("Failed to open rtlsdr device #%d.\n", dev_index);
    return 1;
  }

  if (ppm_error != 0)
  {
    r = rtlsdr_set_freq_correction(dev, ppm_error);
    if (r < 0)
    {
      printf("WARNING: Failed to set PPM error.  Continuing.\n");
    }
  }

  if (SpanWidth == 1024000)  // for 500 kHz
  {
    samp_rate = DEFAULT_SAMPLE_RATE / 2;
  }
  else
  {
    samp_rate = DEFAULT_SAMPLE_RATE;
  }

  r = rtlsdr_set_sample_rate(dev, samp_rate);
  if (r < 0)
  {
    printf("Failed to set sample rate.\n");
    return 1;
  }

  r = rtlsdr_set_center_freq(dev, freq_hz);
  if (r < 0)
  {
    printf("Failed to set center freq.\n");
    return 1;
  }
  else
  {
    printf("Tuned to %u Hz.\n", freq_hz);
  }

  if (gain == 60)  // Auto
  {
    r = rtlsdr_set_tuner_gain_mode(dev, 0);
    if (r < 0)
    {
      printf("WARNING: Failed to enable automatic gain.\n");
    }
  }
  else  // manual gain
  {
    r = rtlsdr_set_tuner_gain_mode(dev, 1);
    if (r < 0) 
    {
      printf("WARNING: Failed to enable manual gain.\n");
      return 1;
	}

    nearest_gain_value = nearest_gain(dev, 10 * gain);

    r = rtlsdr_set_tuner_gain(dev, nearest_gain_value);
    if (r != 0) 
    {
      printf("WARNING: Failed to set tuner gain.\n");
    }
    else
    {
      printf("Tuner gain set to %0.2f dB.\n", nearest_gain_value/10.0);
    }
  }

  r = rtlsdr_reset_buffer(dev);
  if (r < 0)
  {
    printf("WARNING: Failed to reset buffers.\n");
  }

  return 0;
}


static bool read_rtlsdr()
{
    bool        error = false;
    int             n_read;
    int             r;

    r = rtlsdr_read_sync(dev, buffer, buff_len, &n_read);
    if (r < 0)
    {
        printf("WARNING: sync read failed.\n");
        error = true;
    }

    if ((uint32_t) n_read < buff_len)
    {
        printf("Short read (%d / %d), samples lost, exiting!\n",
                n_read, buff_len);
        error = true;
    }

    return error;
}


void perform_fft()
{
  int i;
  fftw_complex pt;
  double pwr;
  double lpwr;
  double pwr_scale;

  pwr_scale= 1.0 / ((float)fft_size * (float)fft_size);

  // Copy data out of rf buffer into fft_input buffer
  for (i = 0; i < fft_size; i++)
  {
    fft_in[i][0] = lut[buffer[2 * i]] * hanning_window_const[i];
    fft_in[i][1] = lut[buffer[2 * i + 1]] * hanning_window_const[i];
  }

  // Run FFT
  fftw_execute(fft_plan);

  // Lock output buffer
  pthread_mutex_lock(&fft_buffer.mutex);

  for (i = 0; i < fft_size; i++)
  {
	// shift, normalize and convert to dBFS
	if (i < fft_size / 2)
	{
	  pt[0] = fft_out[fft_size / 2 + i][0] / fft_size;
	  pt[1] = fft_out[fft_size / 2 + i][1] / fft_size;
	}
	else
	{
	  pt[0] = fft_out[i - fft_size / 2][0] / fft_size;
	  pt[1] = fft_out[i - fft_size / 2][1] / fft_size;
	}
	pwr = pwr_scale * ((pt[0] * pt[0]) + (pt[1] * pt[1]));
	lpwr = 10.f * log10(pwr + 1.0e-20);
	        
	fft_buffer.data[i] = (lpwr * (1.f - fft_time_smooth)) + (fft_buffer.data[i] * fft_time_smooth);
  }

  // Unlock output buffer
  pthread_mutex_unlock(&fft_buffer.mutex);
}


// Scale and manage the fft data
void fft_to_buffer()
{
  int32_t j;

  // Lock FFT output buffer for reading
  //pthread_mutex_lock(&fft_buffer.mutex);

  for (j = 0; j < fft_size; j++)
  {
    fft_output_data[j] = fft_buffer.data[j];
  }

  // Unlock FFT output buffer
  //pthread_mutex_unlock(&fft_buffer.mutex);

  // Scale and limit the samples
  for(j = 0; j < fft_size; j++)
  {
    // Add 110 to put the baseline at -110 dB and another 30!
    fft_output_data[j] = fft_output_data[j] + 145.0;

    // Multiply by 5 (pixels per dB on display)
    fft_output_data[j] = fft_output_data[j] * 5;

    if (Range20dB) // Range20dB
    {
                
      fft_output_data[j] = fft_output_data[j] - 5 * (80 + BaseLine20dB);  
      fft_output_data[j] = 4 * fft_output_data[j];
    }

    if (fft_output_data[j] > 399)
    {
      fft_output_data[j] = 399;
    }
    if (fft_output_data[j] < 1)
    {
      fft_output_data[j] = 1;
    }
  }

  // y3 needs valid values from [6] to [506]
  // Only [7] through [505] are displayed (499 columns)
  // [6] is lowest cal line
  // [256] is middle cal line
  // [506] is highest cal line



  // Lock the histogram buffer for writing
  pthread_mutex_lock(&histogram);
  
  y3[6] = fft_output_data[7 + fft_offset];

  for (j = 7; j <= 505; j++)
  {
    y3[j] = fft_output_data[j + fft_offset];
  }

  y3[506] = fft_output_data[505 + fft_offset];

  // Unlock the histogram buffer
  pthread_mutex_unlock(&histogram);
}

// Main thread
void *rtlsdr_fft_thread(void *arg)
{
  bool *exit_requested = (bool *)arg;
  int i;
  int result;
  uint64_t last_output;
  int r;

  // Set initial parameters
  last_output = monotonic_ms();
  freq_hz = CentreFreq;
  sample_rate_val = SpanWidth;
  linearity_gain_val = (uint32_t)gain;

  fft_offset = (fft_size / 2) - 256;

  // zero all the buffers
  for(i = 0; i <= 1249; i++)
  {
    y3[i] = 1;
    hanning_window_const[i] = 0;
    fft_buffer.data[i] = -100.0;
    fft_output_data[i] = 1;
  }

   for (i = 0; i < 256; i++)
  {
    lut[i] = (float)i / 127.5f - 1.f;
  }

  // Initialise fft
  printf("Initialising FFT (%d bin).. \n", fft_size);
  setup_fft();
  for (i = 0; i < fft_size; i++)
  {
    hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
  }
  printf("FFT Initialised\n");

  // Initialise RTL-SDR
  printf("Initialising RTL-SDR (%.01fMSPS, %.03fMHz).. \n",(float)sample_rate_val/1000000,(float)freq_hz/1000000);

  if (setup_rtlsdr() != 0)
  {
    printf("RTLSDR init failed.\n");
  }
  else
  {
    printf("setup_rtlsdr Done.\n");
  }

  // Copy fft scaled data to display buffer 
  while((false == *exit_requested)) 
  {
    if(monotonic_ms() > (last_output + 2))  // so 500 Hz refresh
    {
      read_rtlsdr();

      perform_fft();

      fft_to_buffer();

      // Reset timer for refresh time
      last_output = monotonic_ms();

      // Check for parameter changes (loop ends here if none)

      // Change of Frequency
      if (NewFreq == true)
      {
        freq_hz = CentreFreq;
        result = rtlsdr_set_center_freq(dev, freq_hz);
        NewFreq = false;
        if (result != 0 )
        {
	      printf("Change of Frequency Failed");
        }
      }

      // Change of Display Span
      if (prepnewscanwidth == true)
      {
        // Shut it all down

        close_fftw();
        //closelog();
        rtlsdr_close(dev);
        usleep(1000000);

        // Notify touchscreen that parameters can be changed
        readyfornewscanwidth = true;

        // zero all the buffers
        for (i = 0; i <= 1249; i++)
        {
          y3[i] = 1;
          hanning_window_const[i] = 0;
          fft_buffer.data[i] = -100.0;
          fft_output_data[i] = 1;
        }

        // Wait for new parameters to be calculated
        while (NewSpan == false)
        {
          usleep(100);
        }

        // Set the new sample rate
        sample_rate_val = SpanWidth;

        usleep(100000);

        // set the new fft centre
        fft_offset = (fft_size / 2) - 256;

        // Initialise fft
        printf("Initialising FFT (%d bin).. \n", fft_size);
        setup_fft();
        for (i = 0; i < fft_size; i++)
        {
          hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
        }
        printf("FFT Intitialised\n");

        // Initialise RTL-SDR
        printf("Initialising RTL-SDR (%.01fMSPS, %.03fMHz).. \n",(float)sample_rate_val/1000000,(float)freq_hz/1000000);

        if (setup_rtlsdr() != 0)
        {
          printf("RTLSDR init failed.\n");
        }
        else
        {
          printf("setup_rtlsdr Done.\n");
        }

        // Reset trigger parameters
        NewSpan = false;
        prepnewscanwidth = false;
        //readyfornewscanwidth = false;
      }

      // Change of gain
      if (NewGain == true)
      {
        if (gain == 60)  // Auto
        {
          r = rtlsdr_set_tuner_gain_mode(dev, 0);
          if (r < 0)
          {
            printf("WARNING: Failed to enable automatic gain.\n");
          }
        }
        else  // manual gain
        {
          r = rtlsdr_set_tuner_gain_mode(dev, 1);
          if (r < 0) 
          {
            printf("WARNING: Failed to enable manual gain.\n");
          }
          nearest_gain_value = nearest_gain(dev, 10 * gain);
          r = rtlsdr_set_tuner_gain(dev, nearest_gain_value);
          if (r != 0) 
          {
            printf("WARNING: Failed to set tuner gain.\n");
          }
          else
          {
            printf("Tuner gain set to %0.2f dB.\n", nearest_gain_value/10.0);
          }
        }
        NewGain = false;
      }

    }
    sleep_ms(1);
  }

  // On exit
  close_fftw();
  closelog();

  rtlsdr_close(dev);
  printf("RTL-SDR Closed\n");
  free (buffer);

  return NULL;
}
