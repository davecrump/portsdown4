#include "main.h"
#include <float.h>

#define WS_INTERVAL_FAST    100

#define FFT_SIZE        1024
#define FFT_TIME_SMOOTH 0.999f // 0.0 - 1.0

#define AIRSPY_FREQ     745000000

#define AIRSPY_SAMPLE   10000000

//#define AIRSPY_SERIAL	0x644064DC2354AACD // WB

volatile int force_exit = 0;

pthread_t fftThread;

static void sleep_ms(uint32_t _duration)
{
    struct timespec req, rem;
    req.tv_sec = _duration / 1000;
    req.tv_nsec = (_duration - (req.tv_sec*1000))*1000*1000;

    while(nanosleep(&req, &rem) == EINTR)
    {
        /* Interrupted by signal, shallow copy remaining time into request, and resume */
        req = rem;
    }
}

/** AirSpy Vars **/
struct airspy_device* device = NULL;
/* Sample type -> 32bit Complex Float */
enum airspy_sample_type sample_type_val = AIRSPY_SAMPLE_FLOAT32_IQ;
/* Sample rate */
uint32_t sample_rate_val = AIRSPY_SAMPLE;
/* DC Bias Tee -> 0 (disabled) */
uint32_t biast_val = 0;
/* Linear Gain */
#define LINEAR
uint32_t linearity_gain_val = 12; // MAX=21
/* Sensitive Gain */
//#define SENSITIVE
uint32_t sensitivity_gain_val = 10; // MAX=21
/* Frequency */
uint32_t freq_hz = AIRSPY_FREQ;

double hanning_window_const[FFT_SIZE];

int airspy_rx(airspy_transfer_t* transfer);

#define FLOAT32_EL_SIZE_BYTE (4)
fftw_complex* fft_in;
fftw_complex*   fft_out;
fftw_plan   fft_plan;

static const char *fftw_wisdom_filename = ".fftw_wisdom";

void setup_fft(void)
{
    int i;
    /* Set up FFTW */
    fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    i = fftw_import_wisdom_from_filename(fftw_wisdom_filename);
    if(i == 0)
    {
        fprintf(stdout, "Computing plan...");
	fflush(stdout);
    }
    fft_plan = fftw_plan_dft_1d(FFT_SIZE, fft_in, fft_out, FFTW_FORWARD, FFTW_EXHAUSTIVE);
    if(i == 0)
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

/* FFT Thread */
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


#define FFT_PRESCALE 3.0

#define FFT_OFFSET  92
#define FFT_SCALE   (FFT_PRESCALE * 3000)


#define FLOOR_TARGET	(FFT_PRESCALE * 47000)
#define FLOOR_TIME_SMOOTH 0.995

#define FLOOR_OFFSET    (FFT_PRESCALE * 38000)

static uint32_t lowest_smooth = FLOOR_TARGET;

static uint32_t fft_output_data[FFT_SIZE];
static uint32_t y2[FFT_SIZE];

void fft_to_buffer()
{
	int32_t i, j;
    uint32_t lowest;
    int32_t offset;

    /* Create and append data points */
    i = 0;
    //uint32_t min = 0xFFFFFFFF, max = 0;
    //float fmin = FLT_MAX, fmax = -FLT_MAX; 

    /* Lock FFT output buffer for reading */
    pthread_mutex_lock(&fft_buffer.mutex);

    for(j=(FFT_SIZE*0.05);j<(FFT_SIZE*0.95);j++)
    {
        fft_output_data[i] = (uint32_t)(FFT_SCALE * (fft_buffer.data[j] + FFT_OFFSET)) + (FFT_PRESCALE*fft_line_compensation[j]); // (fft_line_compensation[j] / 3.0);
        //if(fft_buffer.data[j] > fmax) fmax = fft_buffer.data[j];
        //if(fft_buffer.data[j] < fmin) fmin = fft_buffer.data[j];

        //if(fft_output_data[i]  > max) max = fft_output_data[i] ;
        //if(fft_output_data[i]  < min) min = fft_output_data[i] ;

        i++;
    }

    /* Unlock FFT output buffer */
    pthread_mutex_unlock(&fft_buffer.mutex);

    //printf("min: %"PRIu32"\n", min);
    //printf("max: %"PRIu32"\n", max);
    //printf("fmin: %f\n", fmin);
    //printf("fmax: %f\n", fmax);

   	/* Calculate noise floor */
   	lowest = 0xFFFFFFFF;
   	for(j = (FFT_SIZE*0.05); j < i - (FFT_SIZE*0.1); j++)
    {
    	if(fft_output_data[j] < lowest)
    	{
    		lowest = fft_output_data[j];
    	}
    }
    lowest_smooth = (lowest * (1.f - FLOOR_TIME_SMOOTH)) + (lowest_smooth * FLOOR_TIME_SMOOTH);

    /* Compensate for noise floor */
    offset = (FLOOR_TARGET) - lowest_smooth;
    //printf("lowest: %d, lowest_smooth: %d, offset: %d\n", lowest, lowest_smooth, offset);

    for(j = 0; j < i; j++)
    {
        /* Add noise-floor AGC offset (can be negative) */
        //fft_output_data[j] += offset;

        /* Subtract viewport floor offset and set to zero if underflow */
        //if(__builtin_usub_overflow(fft_output_data[j], (uint32_t)FLOOR_OFFSET, &fft_output_data[j]))
        //{
        //    fft_output_data[j] = 0;
        //}

        /* Divide output by FFT_PRESCALE to scale for uint16_t */
        fft_output_data[j] /= FFT_PRESCALE;

        /* Catch data overflow */
        if(fft_output_data[j] > 0xFFFF)
        {
            fft_output_data[j] = 0xFFFF;
        }
    }

    /* Lock websocket output buffer for writing */
    //pthread_mutex_lock(&_websocket_output->mutex);

    for(j = 0; j < i; j++)
    {
        y2[j] = fft_output_data[j];
    }
	//pthread_mutex_unlock(&_websocket_output->mutex);

}


void sighandler(int sig)
{
	(void) sig;
	
	force_exit = 1;
}


int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	struct timeval tv;
	unsigned int ms; 
    unsigned int oldms_fast = 0;
	int i;

	signal(SIGINT, sighandler);

	fprintf(stdout, "Initialising FFT (%d bin).. ", FFT_SIZE);
	fflush(stdout);
	setup_fft();
	for(i=0; i<FFT_SIZE; i++)
	{
		hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/FFT_SIZE)));
	}
	fprintf(stdout, "Done.\n");
	
	fprintf(stdout, "Initialising AirSpy (%.01fMSPS, %.03fMHz).. ",(float)sample_rate_val/1000000,(float)freq_hz/1000000);
	fflush(stdout);
	if(!setup_airspy())
	{
	    fprintf(stderr, "AirSpy init failed.\n");
		return -1;
	}
	fprintf(stdout, "Done.\n");
	
	fprintf(stdout, "Starting FFT Thread.. ");
	if (pthread_create(&fftThread, NULL, thread_fft, NULL))
	{
		fprintf(stderr, "Error creating FFT thread\n");
		return -1;
	}
	pthread_setname_np(fftThread, "FFT Calculation");
	fprintf(stdout, "Done.\n");

	fprintf(stdout, "Server running.\n");
	fflush(stdout);

	while (!force_exit)
	{
		gettimeofday(&tv, NULL);
		ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

        if ((ms - oldms_fast) > WS_INTERVAL_FAST)
        {
            /* Copy latest FFT data to WS Output Buffer */
            fft_to_buffer();

            printf("y[512] = %d\n", y2[512]); 

            /* Reset timer */
            oldms_fast = ms;
        }
		
        sleep_ms(10);
	}

	close_airspy();
	close_fftw();
	closelog();

	return 0;
}
