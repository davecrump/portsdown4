
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "LimeSuite.h"
#include <getopt.h>
#include <ctype.h>
#include <signal.h>
#define PROGRAM_VERSION "0.0.1"

FILE *input,*output;

typedef struct {
	short re;
	short im;
}scmplx;

//#ifdef ENABLE_LIMESDR

#define EXP_OK    1
#define EXP_FAIL -1
#define EXP_MISS -2
#define EXP_IHX  -3
#define EXP_RBF  -4
#define EXP_CONF -5


static bool m_running = false;
static lms_device_t* device = NULL;
static lms_stream_t streamId,streamIdRx;
static int   m_limesdr_status = EXP_CONF;
bool m_limesdr_tx = false;
bool m_limesdr_rx = false;
double m_sr = 0;
float_type ShiftNCO[16] = { 1000000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
float m_gain = 0.0;
int m_oversample = 0;
int m_Bandwidth = 2000000;
int OVERSAMPLE = 16;
size_t chan = 0;  // 0 gives channel 1 on Lime USB, 1 gives channel 2
lms_info_str_t LimeType = "Not Known";
int LimeBuild = 1804; 
//int LimeBuild = 1806; 
//int LimeBuild = 1810; 

#define FIFO_SIZE (1024 * 1000)


int limesdr_init(int TxOutputSel) 
{
  int n;
  int i;
  lms_info_str_t list[8];
  // TxOutputSel = 1;
  if (m_running == true)
  {
    return 0;
  }

  if ((n = LMS_GetDeviceList(list)) < 0)
  {
    fprintf(stderr,"No Lime found\n");
    return -1;
  }

  if (device == NULL)
  {
    for (i = 0; i < n; i++)
    { 
      printf("Value of list %d is %s\n", i, list[i]);
      if (strstr(list[i], "LimeSDR") != NULL) break;
    }

    // Check whether Lime Mini or Lime USB
    if (strstr(list[i], "LimeSDR-USB") != NULL)
    {
      strcpy(LimeType, "LimeSDR-USB");

      // Lime-USB in use, so reverse antenna connection
      TxOutputSel = TxOutputSel + 1;
      if (TxOutputSel == 3)
      {
        TxOutputSel = 1;
      }
    }
    if (strstr(list[i], "LimeSDR Mini") != NULL)
    {
      strcpy(LimeType, "LimeSDR-Mini");
    }
    printf("LimeType is %s\n\n", LimeType);

    if (i == n) return -1;
    if(LMS_Open(&device, list[i], NULL)!=0) // We can't open any device
    {
      fprintf(stderr,"Can't open Lime\n");
      return -1; 
    }
  }

  if (LMS_Init(device) != 0)
  {
    fprintf(stderr,"LimeSDR failed to init");
    return -1;
  }
  
  //LMS_Reset(device);

  if (LMS_EnableChannel(device, LMS_CH_TX, chan, false) != 0) //Not enable
  {
    return -1;
  }

  // set antenna to 2 for < 2 GHz and 1 for > 2 GHz (reversed for Lime USB)
  if (LMS_SetAntenna(device, LMS_CH_TX, chan, TxOutputSel) != 0)  
  {
    return -1;
  }

  streamId.channel = chan;
  streamId.fifoSize = FIFO_SIZE;  
  streamId.throughputVsLatency = 0.2;
  streamId.isTx = true;
  streamId.dataFmt = LMS_FMT_I16;

  //int res = LMS_SetupStream(device, &streamId);
  LMS_SetupStream(device, &streamId);

  m_running = true;

  return 0;
}

int limesdr_deinit(void) {
	if (m_running == false) return -1;
	m_running = false;
	int res;
	m_limesdr_tx = false;
	res= LMS_StopStream(&streamId);
	res = LMS_EnableChannel(device, LMS_CH_TX, chan, false);
	res= LMS_DestroyStream(device, &streamId);
	//
	res= LMS_StopStream(&streamIdRx);
	res = LMS_EnableChannel(device, LMS_CH_RX, chan, false);
	res= LMS_DestroyStream(device, &streamIdRx);

	res= LMS_Close(device);
	device = NULL;
	m_limesdr_status = EXP_CONF;
    return res;

}

void limesdr_set_freq(double freq) {
	if (m_running == false) return;
		LMS_SetLOFrequency(device, LMS_CH_TX, chan, freq);

}

void limesdr_set_rxfreq(double freq) {
	if (m_running == false) return;
	LMS_SetLOFrequency(device, LMS_CH_RX, chan, freq);

}

//Set level 0-100%
void limesdr_set_level(int level) {

	if (m_running == false) return;
	bool state_before = m_limesdr_tx;
	m_limesdr_tx = false;
	float_type gain = level / 100.0;
	m_gain = gain;
	LMS_SetNormalizedGain(device, LMS_CH_TX, chan, gain);
	LMS_Calibrate(device, LMS_CH_TX, chan, m_Bandwidth, 0);
	m_limesdr_tx = state_before; 
}

void limesdr_set_rx_level(int level) {

	if (m_running == false) return;
	float_type gain = level / 100.0;
	
	LMS_SetNormalizedGain(device, LMS_CH_RX, chan, gain);
}

int limesdr_set_sr(double sr,int OverSample) {


	if (m_running == false) return 0;
		float_type freq = 0;
		lms_range_t Range;
		LMS_GetSampleRateRange(device, LMS_CH_TX, &Range);
		
		if (sr < Range.min)
			m_oversample = 1;
		else
			m_oversample = 0;
		
		m_sr = sr*(1 << m_oversample);
		
		
		
		if ((m_sr < Range.min) || (m_sr > Range.max))
		{
			char sDebug[255];
			sprintf(sDebug, "Valid SR=%f-%f by %f step", Range.min, Range.max, Range.step);
			printf("%s",sDebug);
		}
		
		

		int step_over;
		for (step_over = 32; step_over >= 1; step_over = step_over / 2)
		{
			if ((step_over*m_sr) < 60e6) break;
		}
		OVERSAMPLE = step_over;
		fprintf(stderr,     "Oversample=%d\n", OVERSAMPLE);
		/*
		if (m_sr <= 400000)	OVERSAMPLE = 32;
		if((m_sr>400000)&&(m_sr<=800000)) 	OVERSAMPLE = 16;
		if((m_sr>800000)&&(m_sr<=1600000)) OVERSAMPLE = 8;
		if ((m_sr>1600000) && (m_sr <= 3200000)) OVERSAMPLE = 4;
		if ((m_sr>3200000) && (m_sr <= 6400000)) OVERSAMPLE = 2;
		if ((m_sr>6400000) ) OVERSAMPLE = 1;
		*/

		if (LMS_SetSampleRate(device, m_sr, OVERSAMPLE) != 0)
		{
			printf("SR Not Set");
		}
		m_Bandwidth = (m_sr * 1.4 < 2500000) ? 2500000 : m_sr * 1.4;

                // Cloak this in an IF statement??

		//LMS_SetLPFBW(device, LMS_CH_TX, chan, m_Bandwidth);
		
		float_type HostSR, DacSR;
		LMS_GetSampleRate(device, LMS_CH_TX, chan, &HostSR, &DacSR);
		fprintf(stderr,"SR %f DAC %f\n", HostSR, DacSR);
		
		/*LMS_GetLOFrequency(device, LMS_CH_TX, chan, &freq);
		if (freq != 0) {
			LMS_SetNCOFrequency(device, LMS_CH_TX, chan, ShiftNCO, 0);
			LMS_SetNCOIndex(device, LMS_CH_TX, chan, 0, true);
		}
		*/
	
	return 0;
}

/*
void limesdr_tx_rrc_filter(float rolloff)
{
	if (m_running == false) return;
	int ntaps = 119;
	float *fir = rrc_make_f_filter(rolloff, (1<<m_oversample), ntaps);
	float_type *LimeFir = (float_type*) malloc(ntaps * sizeof(float_type));
	for (int i = 0; i < ntaps; i++)
	{
		LimeFir[i] = fir[i];
	}
	if (LMS_SetGFIRCoeff(device, LMS_CH_TX, chan, LMS_GFIR3, LimeFir, ntaps)<0)
		printf("Unable to set coeff GFIR3");
	LMS_SetGFIR(device, LMS_CH_TX, chan, LMS_GFIR3, true);
	free(LimeFir);

}
*/

void limesdr_transmit(void) {

	if (m_running == false) return;

		//LMS_SetupStream(device, &streamId);
		LMS_EnableChannel(device, LMS_CH_TX, chan, true);
		LMS_StartStream(&streamId);
		LMS_SetGFIR(device, LMS_CH_TX, chan, LMS_GFIR3, false); // When Channel is disable, GFIR enable is lost !!!!!! this is a workaround



		//LMS7_ISINC_BYP_TXTSP
		//Disable ISINC
		/*
		uint16_t Reg;
		LMS_ReadLMSReg(device, 0x208, &Reg);
		Reg = Reg | (1 << 7);
		LMS_WriteLMSReg(device, 0x0208, Reg);
		*/
		//LMS_Calibrate(device, LMS_CH_TX, chan, m_Bandwidth, 0);
		m_limesdr_tx = true;
		

	
}

void limesdr_stoptx(void)
{
	
		if (m_running == false) return;
		m_limesdr_tx = false;
		LMS_StopStream(&streamId);
		LMS_EnableChannel(device, LMS_CH_TX, chan, false);
		
		//LMS_DestroyStream(device, &streamId);

		//LMS_SetNormalizedGain(device, LMS_CH_TX, chan, 0);
		//LMS_DestroyStream(device, &streamId);
	

}

void limesdr_receive(void)
{

	if (m_running == false) return;
	m_limesdr_rx = false;
	LMS_SetLPFBW(device, LMS_CH_RX, chan, m_Bandwidth);
	LMS_EnableChannel(device, LMS_CH_RX, chan, true);
	LMS_StartStream(&streamIdRx);
	m_limesdr_rx = true;
	//LMS_DestroyStream(device, &streamId);

	//LMS_SetNormalizedGain(device, LMS_CH_TX, chan, 0);
	//LMS_DestroyStream(device, &streamId);


}

/*
What you are changing is digital dc offset.
0x0204[15:8] DCCORI
0x0204[7:0] DCCORQ
*/
static uint16_t m_Calib = 0;

void limesdr_set_qcal(char offset)
{

	m_Calib = (m_Calib & 0xFF00) + offset;
	
	LMS_WriteLMSReg(device, 0x204, m_Calib);

}

void limesdr_set_ical(char offset)
{
	m_Calib = (m_Calib & 0xFF) + (offset<<8);
	LMS_WriteLMSReg(device, 0x204, m_Calib);
}

int lime_tx_samples(scmplx *s, int len)
{
	if (m_running == false) return 0;
	if (m_limesdr_tx == false) return 0;
	static int debugCnt = 0;
	int SampleSent;
	static uint64_t TotalSampleSent= 0;
	static scmplx Dummy[1000];
	// lms_stream_meta_t tx_metadata; //Use metadata for additional control over sample send function behavior
	// tx_metadata.flushPartialPacket = false; //do not force sending of incomplete packet
	// tx_metadata.waitForTimestamp = true; //Enable synchronization to HW timestamp
	//memset(Dummy, 0, 1000 * sizeof(scmplx));
	if (m_oversample == 0)
	{
		//tx_metadata.timestamp = TotalSampleSent;
		//if ((SampleSent = LMS_SendStream(&streamId, Dummy, 1000, NULL, 200)) != len)
		//if ((SampleSent = LMS_SendStream(&streamId, s, len, &tx_metadata, 100)) != len)
		if ((SampleSent = LMS_SendStream(&streamId, s, len, NULL/*&tx_metadata*/, 100)) != len)
		{
			fprintf(stderr,"len %d -> SampleSent %d \n", len, SampleSent);
		}
		TotalSampleSent += SampleSent;
	}
	/*else
	{
	static scmplx *AfterInter;
	int InterLen = Interpolate(s, &AfterInter, len);
	if ((SampleSent = LMS_SendStream(&streamId, AfterInter, InterLen, NULL, 100)) != InterLen)
	{
	printf("len %d -> SampleSent %d \n", InterLen, SampleSent);
	}

	}*/

	debugCnt++;
	static lms_stream_status_t TxStatus;
	LMS_GetStreamStatus(&streamId, &TxStatus);


	if ((debugCnt % 100) == 0)
	{
		//static lms_stream_status_t TxStatus;
		//LMS_GetStreamStatus(&streamId, &TxStatus);
		//fprintf(stderr,"Filled %d SymbolRate %f\n", TxStatus.fifoFilledCount,TxStatus.sampleRate);

	}
	return 0;
}

int lime_rx_samples(scmplx *BufferRx, int len)
{
	lms_stream_meta_t rx_metadata;
	int samplesRead = LMS_RecvStream(&streamIdRx, BufferRx, len, &rx_metadata, 100);
	return samplesRead;
}

int SendToOutput(scmplx *BufferRx, int len)
{
	for (int i = 0; i < len; i++)
	{
		unsigned char SymbI, SymbQ;
		SymbI = (unsigned char)((BufferRx[i].re*127.0 / 32767.0) + 127);
		SymbQ = (unsigned char)((BufferRx[i].im*127.0 / 32767.0) + 127);
		fwrite(&SymbI, 1, 1, output);
		fwrite(&SymbQ, 1, 1, output);
	}
  return 0;
}

static bool keep_running=false;

static void signal_handler(int signo)
{
    if (signo == SIGINT)
        fputs("\nCaught SIGINT\n", stderr);
    else if (signo == SIGTERM)
        fputs("\nCaught SIGTERM\n", stderr);
    else if (signo == SIGHUP)
        fputs("\nCaught SIGHUP\n", stderr);
    else if (signo == SIGPIPE)
        fputs("\nReceived SIGPIPE.\n", stderr);
    else
        fprintf(stderr, "\nCaught signal: %d\n", signo);

    keep_running = false;
}

void print_usage()
{

	fprintf(stderr, \
		"limetx -%s\n\
Usage:\nlimetx -s SymbolRate [-i File Input] [-o File Output] [-f Frequency in Khz]  [-g Gain] [-t SampleType] [-h] \n\
-i            Input IQ File I16 format (default stdin) \n\
-i            OutputIQFile (default stdout) \n\
-s            SymbolRate in KS \n\
-f            Frequency in Khz\n\
-g            Gain 0 to 100\n\
-t            Input sample type {float,I16} I16 by default\n\
-h            help (print this help).\n\
Example : ./limetx -s 1000 -f 1242000 -g 80\n\
\n", \
PROGRAM_VERSION);

} /* end function print_usage */

int main(int argc, char **argv) 
{
  input = stdin;
  output = stdout;
  uint64_t SymbolRate = 0;
  uint64_t TxFrequency = 0;
  int TxFreqKHz = 0;
  int Gain = 50;
  int a;
  int anyargs = 0;
  int TxOutputSel;
  enum {TYPE_I16,TYPE_FLOAT};
  int TypeInput = TYPE_I16;
  char *ptr;

  while (1)
  {
    a = getopt(argc, argv, "i:o:s:f:g:ht:");

    if (a == -1)
    {
      if (anyargs) break;
      else a = 'h'; //print usage and exit
    }
    anyargs = 1;
    switch (a)
    {
    case 'i': // InputFile
      input = fopen(optarg, "r");
      if (NULL == input)
      {
        fprintf(stderr, "Unable to open '%s': %s\n",
        optarg, strerror(errno));
        exit(EXIT_FAILURE);
      }
      break;
    case 'o': //output file
			output = fopen(optarg, "wb");
			if (NULL == output) {
				fprintf(stderr, "Unable to open '%s': %s\n",
					optarg, strerror(errno));
				exit(EXIT_FAILURE);
			};
			break;
		case 's': // SymbolRate in KS
			SymbolRate = atol(optarg) * 1000;
			break;
		case 'f': // TxFrequency in KHz (1KHz is lowest resolution supported here)
			TxFrequency = strtoul(optarg, &ptr, 10) * 1000;
                        TxFreqKHz = atoi(optarg);
			break;
		case 'g': // Gain 0..100
			Gain = atoi(optarg);
			break;
        case 't': // Input Type
			if (strcmp("float", optarg) == 0) TypeInput = TYPE_FLOAT;
            if (strcmp("i16", optarg) == 0) TypeInput = TYPE_I16;
			break;
		case 'h': // help
			print_usage();
			exit(0);
			break;
		case -1:
			break;
		case '?':
			if (isprint(optopt))
			{
				fprintf(stderr, "limetx `-%c'.\n", optopt);
			}
			else
			{
				fprintf(stderr, "limetx: unknown option character `\\x%x'.\n", optopt);
			}
			print_usage();

			exit(1);
			break;
		default:
			print_usage();
			exit(1);
			break;
    }  // end switch a
  }    // end while getopt()

  if (TxFrequency == 0)
  {
    fprintf(stderr, "Need set a frequency to tx\n"); exit(0);
  }
  if (SymbolRate == 0)
  {
    fprintf(stderr, "Need set a SampleRate \n"); exit(0);
  }

  // register signal handlers
  if (signal(SIGINT, signal_handler) == SIG_ERR)
    fputs("Warning: Can not install signal handler for SIGINT\n", stderr);
  if (signal(SIGTERM, signal_handler) == SIG_ERR)
    fputs("Warning: Can not install signal handler for SIGTERM\n", stderr);
  if (signal(SIGHUP, signal_handler) == SIG_ERR)
    fputs("Warning: Can not install signal handler for SIGHUP\n", stderr);
  if (signal(SIGPIPE, signal_handler) == SIG_ERR)
    fputs("Warning: Can not install signal handler for SIGPIPE\n", stderr);

  // Set Correct Output based on frequency
  TxOutputSel = 2;
  if(TxFreqKHz > 2000000)
  {
    TxOutputSel = 1; // For f > 2 GHz
  }

    #define BUFFER_SIZE 1000
    scmplx BufferIQ[BUFFER_SIZE];

	scmplx BufferIQRx[BUFFER_SIZE];
    float fBufferIQ[BUFFER_SIZE*2];

    limesdr_init(TxOutputSel);
    limesdr_set_sr(SymbolRate,0);
    limesdr_set_freq(TxFrequency);
	limesdr_stoptx();
	limesdr_set_level(Gain);
    limesdr_transmit();
	limesdr_set_level(Gain);
	/*
	limesdr_set_rx_level(90);
	limesdr_set_rxfreq(2322e6);
	limesdr_receive();
	*/
    keep_running=true;
    while(keep_running)
    { 
#define TV
#ifdef TV

  if(TypeInput==TYPE_I16)
  {
    lms_stream_status_t TxStatus;
    LMS_GetStreamStatus(&streamId, &TxStatus);

    if (TxStatus.fifoFilledCount < FIFO_SIZE / 2)
    {
      int NbRead = fread(BufferIQ, sizeof(scmplx), BUFFER_SIZE, input);
      if (NbRead == 0) { usleep(1000); continue; }
      if (NbRead < 0) break;
      if (NbRead != BUFFER_SIZE) fprintf(stderr, "Incomplete buffer %d/%d\n", NbRead, BUFFER_SIZE);
      lime_tx_samples(BufferIQ, NbRead);
    }
    else
    {
      usleep(1);
    }
  }

  if(TypeInput==TYPE_FLOAT)
  {
    int NbRead=fread(fBufferIQ,sizeof(float),BUFFER_SIZE*2,input);
    if(NbRead<0) break;
    int NbSample=0;
    for(int i=0; i < NbRead; i += 2)
    {
      BufferIQ[NbSample].re=(short)(fBufferIQ[i]*32767);
      BufferIQ[NbSample++].im=(short)(fBufferIQ[i+1]*32767);
    }
    lime_tx_samples(BufferIQ, NbSample);
  }

#endif

#ifdef TPDER
  int LimeRead = lime_rx_samples(BufferIQRx, BUFFER_SIZE);
  lime_tx_samples(BufferIQRx, LimeRead);
#endif
  }
  limesdr_deinit();
}

