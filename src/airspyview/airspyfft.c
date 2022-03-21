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

#include "libairspy/libairspy/src/airspy.h"
#include "timing.h"

#define FFT_SIZE 512                 // Note - FFT size needs to be 512 for this to work
#define FFT_TIME_SMOOTH 0.999f       // 0.0 - 1.0

extern bool NewFreq;                 // Set to true to indicate that frequency needs changing
extern uint32_t CentreFreq;          // Frequency from main
extern bool NewSpan;                 // Set to true to indicate that span needs changing
extern uint32_t SpanWidth;           // Sample rate from main
extern bool NewGain;                 // Set to true to indicate that gain needs changing
extern float gain;                   // Gain (0 - 21) from main
extern bool Range20dB;
extern int BaseLine20dB;


uint16_t y3[FFT_SIZE];               // Histogram values in range 0 - 399


//#define AIRSPY_SAMPLE   10000000
//#define AIRSPY_SAMPLE     2500000

//#define AIRSPY_SERIAL	0x644064DC2354AACD // WB

int force_exit = 0;

pthread_t fftThread;


/** AirSpy Vars **/
struct airspy_device* device = NULL;
/* Sample type -> 32bit Complex Float */
enum airspy_sample_type sample_type_val = AIRSPY_SAMPLE_FLOAT32_IQ;
/* Sample rate */
//uint32_t sample_rate_val = AIRSPY_SAMPLE;
uint32_t sample_rate_val;
/* DC Bias Tee -> 0 (disabled) */
uint32_t biast_val = 0;
/* Linear Gain */
#define LINEAR
//uint32_t linearity_gain_val = 12; // MAX=21
;;uint32_t linearity_gain_val = 20; // MAX=21
uint32_t linearity_gain_val;
/* Sensitive Gain */
//#define SENSITIVE
uint32_t sensitivity_gain_val = 10; // MAX=21
/* Frequency */
uint32_t freq_hz;

double hanning_window_const[FFT_SIZE];

int airspy_rx(airspy_transfer_t* transfer);

#define FLOAT32_EL_SIZE_BYTE (4)
fftw_complex* fft_in;
fftw_complex*   fft_out;
fftw_plan   fft_plan;

pthread_mutex_t histogram;

static const char *fftw_wisdom_filename = ".fftw_wisdom";
static float fft_output_data[FFT_SIZE];

void setup_fft(void)
{
  int i;

  // Set up FFTW
  fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
  fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
  i = fftw_import_wisdom_from_filename(fftw_wisdom_filename);
  if (i == 0)
  {
    printf("Computing fft plan...\n");
  }
  fft_plan = fftw_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_EXHAUSTIVE);

  if (i == 0)
  {
    fftw_export_wisdom_to_filename(fftw_wisdom_filename);
  }
}

static void close_airspy(void)
{
    int result;
    
    /* De-init AirSpy device */
    if(device != NULL)
    {
	    result = airspy_stop_rx(device);
	    if( result != AIRSPY_SUCCESS ) {
		    printf("airspy_stop_rx() failed: %s (%d)\n", airspy_error_name(result), result);
	    }

	    result = airspy_close(device);
	    if( result != AIRSPY_SUCCESS ) 
	    {
		    printf("airspy_close() failed: %s (%d)\n", airspy_error_name(result), result);
	    }
	
	    airspy_exit();
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

static uint8_t setup_airspy()
{
    int result;

    result = airspy_init();
    if( result != AIRSPY_SUCCESS ) {
	    printf("airspy_init() failed: %s (%d)\n", airspy_error_name(result), result);
	    return 0;
    }
    #ifdef AIRSPY_SERIAL
    	result = airspy_open_sn(&device, AIRSPY_SERIAL);
    #else
    	result = airspy_open(&device);
    #endif
    if( result != AIRSPY_SUCCESS ) {
	    printf("airspy_open() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_exit();
	    return 0;
    }

    result = airspy_set_sample_type(device, sample_type_val);
    if (result != AIRSPY_SUCCESS) {
	    printf("airspy_set_sample_type() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_close(device);
	    airspy_exit();
	    return 0;
    }

    result = airspy_set_samplerate(device, sample_rate_val);
    if (result != AIRSPY_SUCCESS) {
	    printf("airspy_set_samplerate() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_close(device);
	    airspy_exit();
	    return 0;
    }

    result = airspy_set_rf_bias(device, biast_val);
    if( result != AIRSPY_SUCCESS ) {
	    printf("airspy_set_rf_bias() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_close(device);
	    airspy_exit();
	    return 0;
    }

    #ifdef LINEAR
	    result =  airspy_set_linearity_gain(device, linearity_gain_val);
	    if( result != AIRSPY_SUCCESS ) {
		    printf("airspy_set_linearity_gain() failed: %s (%d)\n", airspy_error_name(result), result);
	    }
    #elif defined SENSITIVE
	    result =  airspy_set_sensitivity_gain(device, sensitivity_gain_val);
	    if( result != AIRSPY_SUCCESS ) {
		    printf("airspy_set_sensitivity_gain() failed: %s (%d)\n", airspy_error_name(result), result);
	    }
    #endif

    result = airspy_start_rx(device, airspy_rx, NULL);
    if( result != AIRSPY_SUCCESS ) {
	    printf("airspy_start_rx() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_close(device);
	    airspy_exit();
	    return 0;
    }

    result = airspy_set_freq(device, freq_hz);
    if( result != AIRSPY_SUCCESS ) {
	    printf("airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
	    airspy_close(device);
	    airspy_exit();
	    return 0;
    }
    
    return 1;
}

/* transfer->sample_count is normally 65536 */
#define	AIRSPY_BUFFER_COPY_SIZE	65536

typedef struct {
	uint32_t index;
	uint32_t size;
	char data[AIRSPY_BUFFER_COPY_SIZE * FLOAT32_EL_SIZE_BYTE];
	pthread_mutex_t mutex;
	pthread_cond_t 	signal;
} rf_buffer_t;

rf_buffer_t rf_buffer = {
	.index = 0,
	.size = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.signal = PTHREAD_COND_INITIALIZER,
	.data = { 0 }
};

/* Airspy RX Callback, this is called by a new thread within libairspy */
int airspy_rx(airspy_transfer_t* transfer)
{    
    if(transfer->samples != NULL && transfer->sample_count >= AIRSPY_BUFFER_COPY_SIZE)
    {
        pthread_mutex_lock(&rf_buffer.mutex);
        rf_buffer.index = 0;
        memcpy(
            rf_buffer.data,
            transfer->samples,
            (AIRSPY_BUFFER_COPY_SIZE * FLOAT32_EL_SIZE_BYTE)
        );
        rf_buffer.size = AIRSPY_BUFFER_COPY_SIZE / (FFT_SIZE * 2);
        pthread_cond_signal(&rf_buffer.signal);
        pthread_mutex_unlock(&rf_buffer.mutex);
    }
	return 0;
}

typedef struct {
	float data[FFT_SIZE];
	pthread_mutex_t mutex;
} fft_buffer_t;

fft_buffer_t fft_buffer = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

// FFT Thread
void *thread_fft(void *dummy)
{
    (void) dummy;
    int             i, offset;
    fftw_complex    pt;
    double           pwr, lpwr;

	double pwr_scale = 1.0 / ((float)FFT_SIZE * (float)FFT_SIZE);

    while(1)
    {
    	/* Lock input buffer */
    	pthread_mutex_lock(&rf_buffer.mutex);

    	if(rf_buffer.index == rf_buffer.size)
    	{
	    	/* Wait for signalled input */
	    	pthread_cond_wait(&rf_buffer.signal, &rf_buffer.mutex);
    	}

    	offset = rf_buffer.index * FFT_SIZE * 2;

    	/* Copy data out of rf buffer into fft_input buffer */
    	for (i = 0; i < FFT_SIZE; i++)
	    {
	        fft_in[i][0] = ((float*)rf_buffer.data)[offset+(2*i)] * hanning_window_const[i];
	        fft_in[i][1] = ((float*)rf_buffer.data)[offset+(2*i)+1] * hanning_window_const[i];
	    }

	    rf_buffer.index++;

	    /* Unlock input buffer */
    	pthread_mutex_unlock(&rf_buffer.mutex);

    	/* Run FFT */
    	fftw_execute(fft_plan);

    	/* Lock output buffer */
    	pthread_mutex_lock(&fft_buffer.mutex);

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
	        lpwr = 10.f * log10(pwr + 1.0e-20);
	        
	        fft_buffer.data[i] = (lpwr * (1.f - FFT_TIME_SMOOTH)) + (fft_buffer.data[i] * FFT_TIME_SMOOTH);
	    }


	    /* Unlock output buffer */
    	pthread_mutex_unlock(&fft_buffer.mutex);
    }

}


// Scale and manage the fft data
void fft_to_buffer()
{
  int32_t j;

  // Lock FFT output buffer for reading
  pthread_mutex_lock(&fft_buffer.mutex);

  for (j = 0; j < FFT_SIZE; j++)
  {
    fft_output_data[j] = fft_buffer.data[j];
  }

  // Unlock FFT output buffer
  pthread_mutex_unlock(&fft_buffer.mutex);

  // Scale and limit the samples
  for(j = 0; j < FFT_SIZE; j++)
  {
    // Add 110 to put the baseline at -110 dB
    fft_output_data[j] = fft_output_data[j] + 110.0;

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

  // Lock the histogram buffer for writing
  pthread_mutex_lock(&histogram);

  for (j = 0; j < FFT_SIZE; j++)
  {
    y3[j] = fft_output_data[j];
  }

  // Unlock the histogram buffer
  pthread_mutex_unlock(&histogram);
}

// Main thread
void *airspy_fft_thread(void *arg)
{
  bool *exit_requested = (bool *)arg;
  int i;
  int result;
  uint64_t last_output;

  // Set initial parameters
  last_output = monotonic_ms();
  freq_hz = CentreFreq;
  sample_rate_val = SpanWidth;
  linearity_gain_val = (uint32_t)gain;

  // Initialise fft
  printf("Initialising FFT (%d bin).. \n", FFT_SIZE);
  setup_fft();
  for (i = 0; i < FFT_SIZE; i++)
  {
    hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/FFT_SIZE)));
  }
  printf("Done.\n");

  //Initialise Airspy
  printf("Initialising AirSpy (%.01fMSPS, %.03fMHz).. \n",(float)sample_rate_val/1000000,(float)freq_hz/1000000);
  if(!setup_airspy())
  {
	printf("AirSpy init failed.\n");
  //return -1;
  }
  printf("Done.\n");

  // Start Airspy fft thread
  printf("Starting Airspy FFT Thread.. \n");
  if (pthread_create(&fftThread, NULL, thread_fft, NULL))
  {
    printf("Error creating Airspy FFT thread\n");
    //return -1;
  }
  pthread_setname_np(fftThread, "FFT Calculation");
  printf("Done.\n");
  printf("Airspy Server running.\n");

  // Copy fft scaled data to display buffer 
  while((false == *exit_requested)) 
  {
    if(monotonic_ms() > (last_output + 50))  // so 20 Hz refresh
    {
      fft_to_buffer();

      // Reset timer
      last_output = monotonic_ms();

      // Check for parameter change
      if (NewFreq == true)
      {
        freq_hz = CentreFreq;
        result = airspy_set_freq(device, freq_hz);
        NewFreq = false;
        if( result != AIRSPY_SUCCESS )
        {
	      printf("airspy_set_freq() failed: %s (%d)\n", airspy_error_name(result), result);
	      airspy_close(device);
	      airspy_exit();
        }
      }
      if (NewSpan == true)
      {
        close_airspy();
        sample_rate_val = SpanWidth;
        setup_airspy();
        NewSpan = false;
      }
      if (NewGain == true)
      {
        close_airspy();
        linearity_gain_val = (uint32_t)gain;
        setup_airspy();
        NewGain = false;
      }
    }
    sleep_ms(1);
  }

  // On exit
  close_airspy();
  close_fftw();
  closelog();
  printf("Airspy fft Thread Closed\n");

  return NULL;
}
