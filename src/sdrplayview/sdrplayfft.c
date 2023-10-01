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

#include "sdrplayview.h"
#include "timing.h"
#include "sdrplay_api.h"
#include "sdrplayfft.h"

#define PI 3.14159265358979323846

int masterInitialised = 0;
int slaveUninitialised = 0;

int callback_count = 0;

sdrplay_api_DeviceT *chosenDevice = NULL;

#define	SDRPLAY_BUFFER_COPY_SIZE 2048
#define SHORT_EL_SIZE_BYTE (2)

extern bool NewFreq;                 // Set to true to indicate that frequency needs changing
extern uint32_t CentreFreq;          // Frequency in Hz from main

extern bool NewSpan;                 // Set to true to indicate that span needs changing
extern bool prepnewscanwidth;
extern bool readyfornewscanwidth;
extern bool app_exit;

extern bool NewGain;                 // Set to true to indicate that gain needs changing
extern int RFgain;
extern int IFgain;
extern bool agc;
extern int8_t BaselineShift;

extern bool NewPort;                 // Set to true to indicate that port or BiasT needs changing
extern uint8_t Antenna_port;
extern bool BiasT_volts;

extern bool Range20dB;
extern int BaseLine20dB;
extern int fft_size;                 // Number of fft samples.  Depends on scan width
extern float fft_time_smooth; 
extern uint8_t decimation_factor;           // decimation applied by SDRPlay api
extern int span;
extern float SampleRate;
extern int64_t freqoffset;

//extern pthread_t sdrplay_fft_thread_obj;

extern bool NewData; 
bool debug = false;    

int fft_offset;

uint16_t y3[2048];               // Histogram values in range 0 - 399

int force_exit = 0;

pthread_t fftThread;

double hanning_window_const[2048];

typedef struct {
	uint32_t index;
	uint32_t size;
	int16_t idata[SDRPLAY_BUFFER_COPY_SIZE * SHORT_EL_SIZE_BYTE];
	int16_t qdata[SDRPLAY_BUFFER_COPY_SIZE * SHORT_EL_SIZE_BYTE];
	pthread_mutex_t mutex;
	pthread_cond_t 	signal;
} rf_buffer2_t;

rf_buffer2_t rf_buffer2 = {
	.index = 0,
	.size = 0,
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.signal = PTHREAD_COND_INITIALIZER,
	.idata = { 0 },
	.qdata = { 0 }
};

#define FLOAT32_EL_SIZE_BYTE (4)
fftw_complex* fft_in;
fftw_complex*   fft_out;
fftw_plan   fft_plan;

pthread_mutex_t histogram;

static const char *fftw_wisdom_filename = ".fftw_wisdom";
static float fft_output_data[2048];

unsigned long long int total = 0;     /* for debug */
unsigned long long int buf_total = 0; /* for debug */

long int output_buffer_i[FFT_BUFFER_SIZE];
long int output_buffer_q[FFT_BUFFER_SIZE];

sdrplay_api_RxChannelParamsT *chParams;
//sdrplay_api_DeviceT devs[6];
//sdrplay_api_DeviceParamsT *deviceParams = NULL;

int k = 0;
int m = 0;

void setup_fft(void);
void fft();
void fft_to_buffer();
int legal_gain(int demanded_Gain);
void non_filter(short xi, short xq, int reset);
void ConstrainFreq();

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


/* --------------------------------------------------------------------- */
/* display the waterfall on the RPI 7" touch screen                      */
/* --------------------------------------------------------------------- */
void fft()
{
  // note the output buffer is long int (int32_t) sized, max value = 32768 * 256 - 1
  // Called after fft_size samples are in output_buffer_i.
  //  rf_buffer2.idata is int16_t.
  int i;

  pthread_mutex_lock(&rf_buffer2.mutex);
  rf_buffer2.index = 0;

  // Set cal true to to calibrate dBfs
  // It generates a sine wave at -20 dB with varying phase
  bool cal = false;
  float amplitude = 3276.70;
  static float foffset = 2.55;
  static float phaseoffset  = 0;
  phaseoffset = phaseoffset + 0.0002;
  foffset = foffset + 0.000001;

  for (i = 0; i < fft_size; i++)
  {
    if (cal == false)
    {
      rf_buffer2.idata[i]  = (int16_t)(output_buffer_i[i] / 256);
      rf_buffer2.qdata[i]  = (int16_t)(output_buffer_q[i] / 256);
    }
    else
    {
      rf_buffer2.idata[i]  = (int16_t)(amplitude * sin((((float)i / foffset) + phaseoffset) * 2.f * PI));
      rf_buffer2.qdata[i]  = (int16_t)(amplitude * cos((((float)i / foffset) + phaseoffset) * 2.f * PI));
    }
  }   

  pthread_cond_signal(&rf_buffer2.signal);
  pthread_mutex_unlock(&rf_buffer2.mutex);
}


void non_filter(short xi, short xq, int reset)
{
  static int buf_ptr;

  output_buffer_i[buf_ptr] = (long int)xi * 256;
  output_buffer_q[buf_ptr] = (long int)xq * 256;

  buf_ptr++;
  if (buf_ptr == fft_size)
  {
    //printf("Output buffer full\n");
    fft();
    buf_ptr = 0;
  }
}


/* ---------------------------------------------------------------------------------- */
/* the data comes into this callback and we feed it out  to the FFT for display       */
/* ---------------------------------------------------------------------------------- */


void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset, void *cbContext)
{
  short *p_xi, *p_xq;
  int count;

  if (reset)
  {
    printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
    non_filter(0,0, true);
  }

  // note that the API decimation means that we only get 84?? bytes each callback

  total += numSamples;  // for debug purposes
  p_xi = xi;
  p_xq = xq;
        
  for (count=0; count<numSamples; count++)
  {
    non_filter(*p_xi, *p_xq, false);
    p_xi++; // pointer maths ... ugggy but efficient
    p_xq++;
  }
  return;
}


void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
  if (reset) printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
  // Process stream callback data here - this callback will only be used in dual tuner mode
  return;
}


void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
  switch(eventId)
  {
    case sdrplay_api_GainChange:
      printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A":"sdrplay_api_Tuner_B",
				params->gainParams.gRdB, params->gainParams.lnaGRdB,params->gainParams.currGain);
      break;
    case sdrplay_api_PowerOverloadChange:
      printf("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected) ? "sdrplay_api_Overload_Detected":"sdrplay_api_Overload_Corrected");
			// Send update message to acknowledge power overload message received
			sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck,sdrplay_api_Update_Ext1_None);
      break;
    case sdrplay_api_RspDuoModeChange:
      printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
				"sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)?
				"sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)?
				"sdrplay_api_MasterInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)?
				"sdrplay_api_SlaveAttached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)?
				"sdrplay_api_SlaveDetached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)?
				"sdrplay_api_SlaveInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)?
				"sdrplay_api_SlaveUninitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)?
				"sdrplay_api_MasterDllDisappeared":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDllDisappeared)?
				"sdrplay_api_SlaveDllDisappeared": "unknown type");
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
				masterInitialised = 1;
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
				slaveUninitialised = 1;
      break;
    case sdrplay_api_DeviceRemoved:
      printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
      break;
    default:
      printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
    break;
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


typedef struct {
	float data[2048];
	pthread_mutex_t mutex;
} fft_buffer_t;

fft_buffer_t fft_buffer = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

// FFT Thread
void *thread_fft(void *dummy)
{
  (void) dummy;
  int i;
  fftw_complex pt;
  double pwr;
  double lpwr;
  double pwr_scale;

  pwr_scale = 1.0 / ((float)fft_size * (float)fft_size);       // App is restarted on change of fft size

  while (true)
  {
      // Lock input buffer
      pthread_mutex_lock(&rf_buffer2.mutex);

      // Wait for signalled input
      pthread_cond_wait(&rf_buffer2.signal, &rf_buffer2.mutex);

      // Now read it out of the buffer and apply window shaping
      // Reverse i and q if spectrum needs to be reversed
      if (freqoffset < 0)
      {
        for (i = 0; i < fft_size; i++)
        {
          fft_in[i][1] = (float)(rf_buffer2.idata[i]) * hanning_window_const[i];
          fft_in[i][0] = (float)(rf_buffer2.qdata[i]) * hanning_window_const[i];
	    }
      }
      else
      {
        for (i = 0; i < fft_size; i++)
        {
          fft_in[i][0] = (float)(rf_buffer2.idata[i]) * hanning_window_const[i];
          fft_in[i][1] = (float)(rf_buffer2.qdata[i]) * hanning_window_const[i];
	    }
      }

	  // Unlock input buffer
      pthread_mutex_unlock(&rf_buffer2.mutex);

      // Run FFT
      fftw_execute(fft_plan);

      // Take the output of the fft, convert it to dB and smooth it.

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

        // Calculate power from I and Q voltages
	    pwr = pwr_scale * ((pt[0] * pt[0]) + (pt[1] * pt[1]));

        // Convert to dB
        lpwr = 10.f * log10(pwr + 1.0e-20);

        // Time smooth
        fft_buffer.data[i] = (lpwr * (1.f - fft_time_smooth)) + (fft_buffer.data[i] * fft_time_smooth);
      }

      // Unlock output buffer
      pthread_mutex_unlock(&fft_buffer.mutex);

      fft_to_buffer();
  }
  return NULL;
}


// Scale and manage the fft data
void fft_to_buffer()
{
  int32_t j;
  uint32_t average = 0;

  // Lock FFT output buffer for reading
  pthread_mutex_lock(&fft_buffer.mutex);

  for (j = 0; j < fft_size; j++)
  {
    fft_output_data[j] = fft_buffer.data[j];
  }

  // Unlock FFT output buffer
  pthread_mutex_unlock(&fft_buffer.mutex);

  // Calculate the centre of the fft
  if (fft_size == 500)
  {
    fft_offset = -7;
  }
  if (fft_size == 512)
  {
    fft_offset = 0;
  }

  // Scale and limit the samples
  for(j = 0; j < fft_size; j++)
  {
    // Add a constant to position the baseline
    fft_output_data[j] = fft_output_data[j] + 49.8; // 49.8 gives true dBfs (maybe 50??)

    // Shift the display up by 20 dB if required
    fft_output_data[j] = fft_output_data[j] + (float)BaselineShift;

    // Multiply by 5 (pixels per dB on display)
    fft_output_data[j] = fft_output_data[j] * 5;

    if (Range20dB) // Range20dB
    {
                
      fft_output_data[j] = fft_output_data[j] - 5 * (80 + BaseLine20dB);  
      fft_output_data[j] = 4 * fft_output_data[j];
    }

    if (fft_output_data[j] > 399)
    {
      //printf("fft output = %f\n", fft_output_data[j] );
      fft_output_data[j] = 399;
    }
    if (fft_output_data[j] < 1)
    {
      //printf("fft output = %f\n", fft_output_data[j] );
      fft_output_data[j] = 1;
    }
    average = average + fft_output_data[j];
  }
  if(debug)
  {
    printf("Average = %d\n", average);
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
    //printf("%d\n", j + fft_offset);
  }

  y3[506] = fft_output_data[505 + fft_offset];

  // Unlock the histogram buffer
  pthread_mutex_unlock(&histogram);

  // Wait here until data has been read
  NewData = true;
  //uint16_t sleep_count = 0;
  while (NewData == true)
  {
    usleep(100);
    //sleep_count++;
  }
  //printf("fft output slept for %d periods of 100us\n", sleep_count);
}


// Takes demanded gain and returns a valid gain for the device
int legal_gain(int demanded_Gain)
{
  if (chosenDevice->hwVer == SDRPLAY_RSP1_ID)
  {
    if (demanded_Gain > 3)
    {
      return 3;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSP1A_ID)
  {
    if (demanded_Gain > 6)
    {
      return 6;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSP2_ID)
  {
    if (demanded_Gain > 4)
    {
      return 4;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
  {
    if (demanded_Gain > 4)
    {
      return 4;
    }
    else
    {
      return demanded_Gain;
    }
  }
  if (chosenDevice->hwVer == SDRPLAY_RSPdx_ID)
  {
    if (demanded_Gain > 18)
    {
      return 18;
    }
    else
    {
      return demanded_Gain;
    }
  }
  return 0;
}


// Main thread
void *sdrplay_fft_thread(void *arg) {

  sdrplay_api_DeviceT devs[6];
  unsigned int ndev;
  int i;
  float ver = 0.0;
  sdrplay_api_ErrT err;
  sdrplay_api_DeviceParamsT *deviceParams = NULL;
  sdrplay_api_CallbackFnsT cbFns;
  sdrplay_api_RxChannelParamsT *chParams;
  int reqTuner = 0;
  int master_slave = 0;
  unsigned int chosenIdx = 0;
  uint64_t last_output;
  bool *exit_requested = (bool *)arg;

  // Initialise fft
  printf("Initialising FFT (%d bin).. \n", fft_size);
  setup_fft();
  for (i = 0; i < fft_size; i++)
  {
    hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
  }
  printf("FFT Intitialised\n");

  printf("requested Tuner=%c Mode=%s\n", (reqTuner == 0)? 'A': 'B', (master_slave == 0)?
		"Single_Tuner": "Master/Slave");

  // Open API
  if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
  {
    printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
    cleanexit(150);
  }
  else
  {
    // Enable debug logging output
    if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success)
    {
      printf("sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
    }
	
    // Check API versions match
    if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success)
    {
      printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
    }
    if (ver != SDRPLAY_API_VERSION) {
      printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
    }
 
    // Lock API while device selection is performed
    sdrplay_api_LockDeviceApi();

    // Fetch list of available devices
    if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
    {
      printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
      cleanexit(150);
    }

    // Exit if no devices found
    if (ndev == 0)
    {
      printf("No devices found.  Exiting\n");
      cleanexit(150);
    }

    printf("MaxDevs=%d NumDevs=%d\n", sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);
    if (ndev > 0)
    {
      for (i = 0; i < (int)ndev; i++)
      {
        if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
        {
          printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i,devs[i].SerNo, devs[i].hwVer , devs[i].tuner, devs[i].rspDuoMode);
		}
		else
        {
          printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo,devs[i].hwVer, devs[i].tuner);
        }
      }
		
      // Choose device
      if ((reqTuner == 1) || (master_slave == 1))  // requires RSPduo
      {
        // Pick first RSPduo
        for (i = 0; i < (int)ndev; i++)
        {
          if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
          {
            chosenIdx = i;
            break;
          }
        }
      }
      else
      {
        // Pick first device of any type
        for (i = 0; i < (int)ndev; i++)
        {
          chosenIdx = i;
          break;
        }
      }
      if (i == ndev)
      {
        printf("Couldn't find a suitable device to open - exiting\n");
      }
      printf("chosenDevice = %d\n", chosenIdx);
      chosenDevice = &devs[chosenIdx];
      printf("chosenDevice->hwVer = %d\n", chosenDevice->hwVer);

 
      // If chosen device is an RSPduo, assign additional fields
      if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
      {
        // If master device is available, select device as master
        if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master)
        {
          // Select tuner based on user input (or default to TunerA)
          chosenDevice->tuner = sdrplay_api_Tuner_A;
          if (reqTuner == 1)
          {
            chosenDevice->tuner = sdrplay_api_Tuner_B;
          }
          // Set operating mode
          if (!master_slave)
          {
            // Single tuner mode
            chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
            printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x\n", chosenIdx,chosenDevice->rspDuoMode, chosenDevice->tuner);
          }
          else
          {
            chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
            // Need to specify sample frequency in master/slave mode
            chosenDevice->rspDuoSampleFreq = 6000000.0;
            printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x rspDuoSampleFreq=%.1f\n",
              chosenIdx, chosenDevice->rspDuoMode,
              chosenDevice->tuner, chosenDevice->rspDuoSampleFreq);
          }
        }
        else
        {
          // Only slave device available
          // Shouldn't change any parameters for slave device
        }
      }
 
      // Select chosen device
      if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
        cleanexit(150);
      }
		
      // Unlock API now that device is selected
      sdrplay_api_UnlockDeviceApi();
			
      // Retrieve device parameters so they can be changed if wanted
      if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_GetDeviceParams failed %s\n",sdrplay_api_GetErrorString(err));
        cleanexit(150);
      }
			
      // Check for NULL pointers before changing settings
      if (deviceParams == NULL)
      {
        printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
        cleanexit(150);
      }
		
      // Configure dev parameters
      if (deviceParams->devParams != NULL)
      {
        // This will be NULL for slave devices, only the master can change these parameters
        // Only need to update non-default settings
        if (master_slave == 0)
        {
          printf("Sample Rate: %.0f\n", SampleRate);
          deviceParams->devParams->fsFreq.fsHz = SampleRate;
        }
        else
        {
          // Can't change Fs in master/slave mode
        }
      }

      // Configure tuner parameters (depends on selected Tuner which parameters to use)
      chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)? deviceParams->rxChannelB: deviceParams->rxChannelA;
      if (chParams != NULL)
      {
        // Set Frequency
        chParams->tunerParams.rfFreq.rfHz = (float)CentreFreq;  // Frequency is float in Hz
        printf("Frequency: %.0f\n", (float)CentreFreq);

        // Set the bandwidth
        switch (span)
        {
          case 100000:                                          // 100 kHz
          case 200000:                                          // 200 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_0_200;
            break;
          case 500000:                                          // 500 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_0_600;
            break;
          case 1000000:                                         // 1000 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
            break;
          case 2000000:                                         // 2000 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_5_000;
            break;
          case 5000000:                                         // 5000 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_6_000;
            break;
          case 10000000:                                        // 10000 kHz
            chParams->tunerParams.bwType = sdrplay_api_BW_8_000;
            break;
        }
        printf("Span: %d kHz\n", span / 1000);

        // Change single tuner mode to ZIF
        if (master_slave == 0)
        { 
          chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
        }

        // Set the tuner gain
        chParams->tunerParams.gain.gRdB = IFgain;          // Set between 20 and 59.
        printf("IF Gain: %d\n", IFgain);

        chParams->tunerParams.gain.LNAstate = legal_gain(RFgain);
        printf("RF Gain: %d\n", legal_gain(RFgain));

        // Set the Decimation
        if (decimation_factor == 1)
        {
          chParams->ctrlParams.decimation.enable = 0;   // Decimation off
          printf("Decimation in api: off\n");
        }
        else
        {
          chParams->ctrlParams.decimation.enable = 1;            // default 0 (off), 1=on
          chParams->ctrlParams.decimation.decimationFactor = decimation_factor;
          printf("Decimation in api: %d\n", decimation_factor);
        }
				
        // wideband = 1 uses better filters but less efficient on cpu usage
        chParams->ctrlParams.decimation.wideBandSignal = 1;    // default 0
			
        // Set AGC
        if (agc == true)
        {
          chParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
        }
        else
        {
          chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
        }

        // Set extras if it is an RSPdx
        if (chosenDevice->hwVer == SDRPLAY_RSPdx_ID)
        {
          // Set BiasT
          if (BiasT_volts)
          {
            deviceParams->devParams->rspDxParams.biasTEnable = 1;
          }
          else
          {
            deviceParams->devParams->rspDxParams.biasTEnable = 0;
          }

          // Set Antenna Port
          switch (Antenna_port)
          {
            case 0:
              deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_A;
              break;
            case 1:
              deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_B;
              break;
            case 2:
              deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_C;
              break;
          }
        }
      }
      else
      {
        printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
        // goto CloseApi;
      }
		
      // Assign callback functions to be passed to sdrplay_api_Init()
      cbFns.StreamACbFn = StreamACallback;
      cbFns.StreamBCbFn = StreamBCallback;
      cbFns.EventCbFn = EventCallback;
		
      // Now we're ready to start by calling the initialisation function
      // This will configure the device and start streaming
      if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
        if (err == sdrplay_api_StartPending)  // This can happen if we're starting in master/slave mode as a slave and the master is not yet running
        { 
          while(1)
          {
            usleep(1000);
            if (masterInitialised)  // Keep polling flag set in event callback until the master is initialised
            {
              // Redo call - should succeed this time
              if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
              {
                printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
              }
              // goto CloseApi;
            }
            printf("Waiting for master to initialise\n");
          }
        }
        else
        {
          sdrplay_api_ErrorInfoT *errInfo = sdrplay_api_GetLastError(NULL);
          if (errInfo != NULL) printf("Error in %s: %s(): line %d: %s\n", errInfo->file, errInfo->function, errInfo->line, errInfo->message);
          {
            cleanexit(150);
          }
        }
      }

      // Set initial parameters for Local display of waterfall
      last_output = monotonic_ms();

      fft_offset = (fft_size / 2) - 250;

      // zero all the buffers
      for(i = 0; i < 2048; i++)
      {
        y3[i] = 1;
        //hanning_window_const[i] = 0;
        fft_buffer.data[i] = -20.0;
        fft_output_data[i] = 1;
      }

      // Start fft thread
      printf("Starting FFT Thread.. \n");
      if (pthread_create(&fftThread, NULL, thread_fft, NULL))
      {
        printf("Error creating FFT thread\n");
        //return -1;
      }
      pthread_setname_np(fftThread, "FFT Calculation");
      printf("FFT thread running.\n");

      // Copy fft scaled data to display buffer 
      while ((false == *exit_requested) && (app_exit == false)) 
      {
        if(monotonic_ms() > (last_output + 50))  // so 20 Hz refresh
        {
          // Reset timer for 20 Hz refresh
          last_output = monotonic_ms();

         // Check for parameter changes (loop ends here if none)

          // Change of Frequency //////////////////////////////////////////
          if (NewFreq == true)
          {
            sdrplay_api_Uninit(chosenDevice->dev);
            chParams->tunerParams.rfFreq.rfHz = (float)CentreFreq;

            // Restart SDR
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
              printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
            }
            NewFreq = false;
          }

          // Change of gain ////////////////////////////////////////////////
          if (NewGain == true)
          {
            sdrplay_api_Uninit(chosenDevice->dev);

            // Set AGC
            if (agc == true)
            {
              chParams->ctrlParams.agc.enable = sdrplay_api_AGC_100HZ;
            }
            else
            {
              chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
            }

            // Set RF Gain
            chParams->tunerParams.gain.LNAstate = legal_gain(RFgain);

            // Set IF gain
            chParams->tunerParams.gain.gRdB = IFgain;          // Set between 20 (max gain) and 59 (least)
        
            // Restart SDR
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
              printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
            }
            NewGain = false;
          }

          // Change of Display Span /////////////////////////////////////////
          if (prepnewscanwidth == true)
          {
            sdrplay_api_Uninit(chosenDevice->dev);

            printf("prepnewscanwidth == true\n");
            // Notify touchscreen that parameters can be changed
            readyfornewscanwidth = true;
            printf("readyfornewscanwidth == true\n");

            // Wait for new parameters to be calculated
            while (NewSpan == false)
            {
              usleep(100);
            }
            printf("NewSpan == true\n");

            // Set the fft centre offset
            if (fft_size == 500)
            {
              fft_offset = -7;
            }
            if (fft_size == 512)
            {
              fft_offset = 0;
            }
            printf("new fft_size = %d\n", fft_size);
            printf("new fft_offset = %d\n", fft_offset);

            // Reset trigger parameters
            NewSpan = false;
            prepnewscanwidth = false;
            readyfornewscanwidth = false;

            // Initialise fft
            printf("Initialising FFT (%d bin).. \n", fft_size);
            setup_fft();
            for (i = 0; i < fft_size; i++)
            {
              hanning_window_const[i] = 0.5 * (1.0 - cos(2*M_PI*(((double)i)/fft_size)));
            }
            printf("FFT Intitialised\n");

            // Set the new sample rate
            if (deviceParams->devParams != NULL)
            {
              // This will be NULL for slave devices, only the master can change these parameters
              if (master_slave == 0)
              {
                printf("Sample Rate: %.0f\n", SampleRate);
                deviceParams->devParams->fsFreq.fsHz = SampleRate;
              }
            }

            // Set the new Decimation
            if (decimation_factor == 1)
            {
              chParams->ctrlParams.decimation.enable = 0;   // Decimation off
              printf("Decimation in api: off\n");
            }
            else
            {
              chParams->ctrlParams.decimation.enable = 1;            // default 0 (off), 1=on
              chParams->ctrlParams.decimation.decimationFactor = decimation_factor;
              printf("Decimation in api: %d\n", decimation_factor);
            }

             // Set the new bandwidth
            switch (span)
            {
              case 100000:                                          // 100 kHz
              case 200000:                                          // 200 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_0_200;
                break;
              case 500000:                                          // 500 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_0_600;
                break;
              case 1000000:                                         // 1000 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
                break;
              case 2000000:                                         // 2000 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_5_000;
                break;
              case 5000000:                                         // 5000 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_6_000;
                break;
              case 10000000:                                        // 10000 kHz
                chParams->tunerParams.bwType = sdrplay_api_BW_8_000;
                break;
            }

            printf("end of change\n");

            // Restart SDR
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
              printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
            }

          }

          // Change of Port or BiasT /////////////////////////////////////////
          if (NewPort == true)
          {
            sdrplay_api_Uninit(chosenDevice->dev);

            if (chosenDevice->hwVer == SDRPLAY_RSPdx_ID)
            {
              // Set BiasT
              if (BiasT_volts)
              {
                deviceParams->devParams->rspDxParams.biasTEnable = 1;
              }
              else
              {
                deviceParams->devParams->rspDxParams.biasTEnable = 0;
              }

              // Set Antenna Port
              switch (Antenna_port)
              {
                case 0:
                  deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_A;
                  break;
                case 1:
                  deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_B;
                  break;
                case 2:
                  deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_C;
                  break;
              }
            }
            printf("Port or BiasT change for rspDX complete\n");
            NewPort = false;

            // Restart SDR
            if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
            {
              printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
            }
          }
          else
          {
            usleep(100);
          }

          if (*exit_requested == true)
          {
            printf("Exit Requested before delay\n");
          }
          sleep_ms(1);
        }
      }
                        
      // Finished with device so uninitialise it
      if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success)
      {
        printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));

        if (err == sdrplay_api_StopPending) 
        {
          // We’re stopping in master/slave mode as a master and the slave is still running
          while(1)
          {
            usleep(1000);
            if (slaveUninitialised) 
            {
              // Keep polling flag set in event callback until the slave is uninitialised

              // Repeat call - should succeed this time
              if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) 
              {
                printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
              }
              slaveUninitialised = 0;
            }
            printf("Waiting for slave to uninitialise\n");
          }
        }
      }
      // Release device (make it available to other applications)
      sdrplay_api_ReleaseDevice(chosenDevice);
    }
 
    sdrplay_api_UnlockDeviceApi();
    sdrplay_api_Close();
  }

  close_fftw();
  closelog();
  printf("Main fft Thread Closed\n");

  return 0;
}





