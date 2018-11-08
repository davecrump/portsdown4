
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
static lms_stream_t streamId,streamIdRx,streamIdRx2;
static int   m_limesdr_status = EXP_CONF;
bool m_limesdr_tx = false;
bool m_limesdr_rx = false;
double m_sr = 0;
float_type ShiftNCO[16] = { 1000000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
float m_gain = 0.0;
int m_oversample = 0;
int m_Bandwidth = 2000000;
int OVERSAMPLE = 16;
int NbChannelRx=0;
enum {TYPE_U8,TYPE_I16,TYPE_FLOAT};
int TypeInput = TYPE_I16;
#define FIFO_SIZE (1024 * 1000)


int limesdr_init() {

	if (m_running == true) return 0;

	int n;
	lms_info_str_t list[8];
	if ((n = LMS_GetDeviceList(list)) < 0)
    {
        fprintf(stderr,"No LimeSDR found\n");
		return -1;
	}
	if (device == NULL) {
		int i ;
		for (i = 0; i < n; i++) if (strstr(list[i], "LimeSDR") != NULL) break;
		if (i == n) return -1;
		if(LMS_Open(&device, list[i], NULL)!=0) 
        {
            fprintf(stderr,"Can't open Lime\n");
            return -1; // We can't open any of device
        }
	}


	if (LMS_Init(device) != 0)
	{
		fprintf(stderr,"LimeSDR failed to init");
		return -1;
	}
	//LMS_Reset(device);
	
    NbChannelRx=LMS_GetNumChannels(device, LMS_CH_RX);
            fprintf(stderr,"Rx Channels = %d\n",NbChannelRx);

    NbChannelRx=1; // WE force to have only one channel right now

	if (LMS_EnableChannel(device, LMS_CH_RX, 0, false) != 0)
    {
        fprintf(stderr,"Enable channel 0 failed\n");
		return -1;
    }
    if (LMS_SetAntenna(device, LMS_CH_RX, 0, 1) != 0)
    {
        fprintf(stderr,"Antenna channel 0 failed\n");
        return -1;
    }
 
    streamIdRx.channel = 0;
	streamIdRx.fifoSize = 1024 * 1000;
	streamIdRx.throughputVsLatency = 0.2;
	streamIdRx.isTx = false;
	streamIdRx.dataFmt = LMS_FMT_I16;

   	
	int res = LMS_SetupStream(device, &streamIdRx);
    if(res!=0) {fprintf(stderr,"SetupStream failed\n");return -1;}

    if(NbChannelRx>1)
    {
        if (LMS_EnableChannel(device, LMS_CH_RX, 1, true) != 0)
	    {
            fprintf(stderr,"Enable channel 1 failed\n");
		    return -1;
        }
       if (LMS_SetAntenna(device, LMS_CH_RX, 1, 1) != 0)
        {
            fprintf(stderr,"Enable channel 1 failed\n");
		    return -1;
        }
        streamIdRx2.channel = 1;
    	streamIdRx2.fifoSize = 1024 * 1000;
    	streamIdRx2.throughputVsLatency = 0.2;
    	streamIdRx2.isTx = false;
    	streamIdRx2.dataFmt = LMS_FMT_I16;
        res = LMS_SetupStream(device, &streamIdRx2);
    }
	m_running=true;
	return 0;
}

int limesdr_deinit(void) {
	if (m_running == false) return -1;
	m_running = false;
	int res;
	
	res= LMS_StopStream(&streamIdRx);
	res = LMS_EnableChannel(device, LMS_CH_RX, 0, false);
	res= LMS_DestroyStream(device, &streamIdRx);
 if(NbChannelRx>1)
{
    res= LMS_StopStream(&streamIdRx2);
	res = LMS_EnableChannel(device, LMS_CH_RX, 1, false);
	res= LMS_DestroyStream(device, &streamIdRx2);
}
	res= LMS_Close(device);
	device = NULL;
	m_limesdr_status = EXP_CONF;
    return res;

}

void limesdr_set_freq(double freq) {
	if (m_running == false) return;
		LMS_SetLOFrequency(device, LMS_CH_TX, 0, freq);

}

void limesdr_set_rxfreq(double freq) {
	if (m_running == false) return;
	if(LMS_SetLOFrequency(device, LMS_CH_RX, 0, freq)!=0)
    {
        fprintf(stderr,"Set Rx Frequency failed %f\n",freq);
    }

}

//Set level 0-100%
void limesdr_set_level(int level) {

	if (m_running == false) return;
	bool state_before = m_limesdr_tx;
	m_limesdr_tx = false;
	float_type gain = level / 100.0;
	m_gain = gain;
	LMS_SetNormalizedGain(device, LMS_CH_TX, 0, gain);
	LMS_Calibrate(device, LMS_CH_TX, 0, m_Bandwidth, 0);
	m_limesdr_tx = state_before; 
}

void limesdr_set_rx_level(int level) {

	if (m_running == false) return;
	float_type gain = level / 100.0;
	
	LMS_SetNormalizedGain(device, LMS_CH_RX, 0, gain);
    LMS_SetNormalizedGain(device, LMS_CH_RX, 1, gain);
}

int limesdr_set_sr(double sr,int OverSample) {


	if (m_running == false) return 0;
		float_type freq = 0;
		lms_range_t Range;
		LMS_GetSampleRateRange(device, LMS_CH_RX, &Range);
		
		if (sr < Range.min)
			m_oversample = 1;
		else
			m_oversample = 0;
		
		m_sr = sr*(1 << m_oversample);
		
		
		
		if ((m_sr < Range.min) || (m_sr > Range.max))
		{

			fprintf(stderr, "Valid SR=%f-%f by %f step\n", Range.min, Range.max, Range.step);

		}
		
		

		int step_over;
		for (step_over = 32; step_over >= 1; step_over = step_over / 2)
		{
			if ((step_over*m_sr) < 60e6) break;
		}
		OVERSAMPLE = step_over;
		
		

		if (LMS_SetSampleRate(device, m_sr, OVERSAMPLE) != 0)
		{
			fprintf(stderr,"SR Not Set with decimation %d\n",OVERSAMPLE);
		}
        else
            fprintf(stderr,     "SR=%f Decimation=%d\n",m_sr, OVERSAMPLE);
        float_type HostSR, DacSR;

        LMS_GetSampleRate(device, LMS_CH_RX, 0, &HostSR, &DacSR);
		fprintf(stderr,"SR %f DAC %f\n", HostSR, DacSR); 

        lms_range_t RangeBP;       
        LMS_GetLPFBWRange(device,LMS_CH_RX,&RangeBP);	
        fprintf(stderr, "Valid BP=%f-%f by %f step\n", RangeBP.min, RangeBP.max, RangeBP.step);	

        m_Bandwidth=m_sr;
		m_Bandwidth = (m_sr  < RangeBP.min) ? RangeBP.min : m_Bandwidth;
        m_Bandwidth = (m_sr  > RangeBP.max) ? RangeBP.max : m_Bandwidth;
		//if(LMS_SetLPFBW(device, LMS_CH_RX, 0, m_Bandwidth)!=0)
        //    fprintf(stderr,"SetLPF with %d bandwidth failed\n",m_Bandwidth);
      
        float_type BandWidth;
        LMS_GetLPFBW(device,LMS_CH_RX,0,&BandWidth);
        fprintf(stderr,"Bandwidth=%f\n",BandWidth);		

      
		LMS_GetSampleRate(device, LMS_CH_RX, 0, &HostSR, &DacSR);
		fprintf(stderr,"SR %f DAC %f\n", HostSR, DacSR);
		
			
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
	if (LMS_SetGFIRCoeff(device, LMS_CH_TX, 0, LMS_GFIR3, LimeFir, ntaps)<0)
		printf("Unable to set coeff GFIR3");
	LMS_SetGFIR(device, LMS_CH_TX, 0, LMS_GFIR3, true);
	free(LimeFir);

}
*/

void limesdr_transmit(void) {

	if (m_running == false) return;

		//LMS_SetupStream(device, &streamId);
		LMS_EnableChannel(device, LMS_CH_TX, 0, true);
		LMS_StartStream(&streamId);
		LMS_SetGFIR(device, LMS_CH_TX, 0, LMS_GFIR3, false); // When Channel is disable, GFIR enable is lost !!!!!! this is a workaround



		//LMS7_ISINC_BYP_TXTSP
		//Disable ISINC
		/*
		uint16_t Reg;
		LMS_ReadLMSReg(device, 0x208, &Reg);
		Reg = Reg | (1 << 7);
		LMS_WriteLMSReg(device, 0x0208, Reg);
		*/
		//LMS_Calibrate(device, LMS_CH_TX, 0, m_Bandwidth, 0);
		m_limesdr_tx = true;
		

	
}

void limesdr_stoptx(void)
{
	
		if (m_running == false) return;
		m_limesdr_tx = false;
		LMS_StopStream(&streamId);
		LMS_EnableChannel(device, LMS_CH_TX, 0, false);
		
		//LMS_DestroyStream(device, &streamId);

		//LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 0);
		//LMS_DestroyStream(device, &streamId);
	

}

void limesdr_receive(void)
{

	if (m_running == false) return;
	m_limesdr_rx = false;
	//LMS_SetLPFBW(device, LMS_CH_RX, 0, m_Bandwidth);
	LMS_EnableChannel(device, LMS_CH_RX, 0, true);
	LMS_StartStream(&streamIdRx);
	m_limesdr_rx = true;
	//LMS_DestroyStream(device, &streamId);

	//LMS_SetNormalizedGain(device, LMS_CH_TX, 0, 0);
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
	lms_stream_meta_t tx_metadata; //Use metadata for additional control over sample send function behavior
	tx_metadata.flushPartialPacket = false; //do not force sending of incomplete packet
	tx_metadata.waitForTimestamp = true; //Enable synchronization to HW timestamp
	//memset(Dummy, 0, 1000 * sizeof(scmplx));
	if (m_oversample == 0)
	{
		tx_metadata.timestamp = TotalSampleSent;
		//if ((SampleSent = LMS_SendStream(&streamId, Dummy, 1000, NULL, 200)) != len)
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
    
    switch(TypeInput)
    {
            case TYPE_I16:fwrite(BufferRx,sizeof(scmplx),len,output);break;
            case TYPE_U8:
            {
                static unsigned char *BufferOut=NULL;
                if(BufferOut==NULL) BufferOut=(unsigned char *)malloc(2*len); // Assume Len is fixed at each call
                for (int i = 0; i < len; i++)
            	{
		            unsigned char SymbI, SymbQ;
		            //SymbI = (unsigned char)((BufferRx[i].re / 256.0) + 127);
		            //SymbQ = (unsigned char)((BufferRx[i].im / 256.0) + 127);
                    SymbI = (unsigned char)(BufferRx[i].re >>8)+127;
		            SymbQ = (unsigned char)(BufferRx[i].im >>8)+127;
                    BufferOut[i*2]=SymbI;
                    BufferOut[i*2+1]=SymbQ;
		            //fwrite(&SymbI, 1, 1, output);
		            //fwrite(&SymbQ, 1, 1, output);
	            }       
                fwrite(BufferOut,1,len*2,output);
            };
            break;
            case TYPE_FLOAT:
            {
                for (int i = 0; i < len; i++)
            	{
		            float SymbI, SymbQ;
		            SymbI = (float)((BufferRx[i].re/ 32767.0));
		            SymbQ = (float)((BufferRx[i].im/ 32767.0));
		            fwrite(&SymbI, sizeof(float), 1, output);
		            fwrite(&SymbQ, sizeof(float), 1, output);
	            }      
            }
            break;

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
		"limerx -%s\n\
Usage:\nlimerx -s SymbolRate -f Frequency in Khz [-o File Output] [-g Gain] [-t SampleType] [-b BurstSize] [-p Partial Index] [-h] \n\
-o            OutputIQFile (default stdout) \n\
-s            SymbolRate in KS \n\
-f            Frequency in Khz\n\
-g            Gain 0 to 100\n\
-t            Input sample type {float,i16,u8} i16 by default\n\
-b            Burst Size buffer (1024 default)\n\
-p            Partial write : only 1/p is sent\n\
-h            help (print this help).\n\
Example : ./limerx -s 1000 -f 1242000 -g 80\n\
\n", \
PROGRAM_VERSION);

} /* end function print_usage */

int main(int argc, char **argv) 
{
    
	input = stdin;
	output = stdout;
	uint64_t SymbolRate = 0;
	uint64_t TxFrequency = 0;
	int Gain = 50;
	int a;
	int anyargs = 0;
    int Burst=1024;
    int Partial=1;
    
	while (1)
	{
		a = getopt(argc, argv, "i:o:s:f:g:ht:b:p:");

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
		case 'f': // TxFrequency in Khz
			TxFrequency = atol(optarg)*1000;
			break;
		case 'g': // Gain 0..100
			Gain = atoi(optarg);
			break;
        case 't': // Input Type
			if (strcmp("float", optarg) == 0) TypeInput = TYPE_FLOAT;
            if (strcmp("i16", optarg) == 0) TypeInput = TYPE_I16;
             if (strcmp("u8", optarg) == 0) TypeInput = TYPE_U8;
			break;
  		case 'b': // Burst buffer
			Burst = atoi(optarg);
			break;
  		case 'p': // Partial buffer
			Partial = atoi(optarg);
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
		}/* end switch a */
	}/* end while getopt() */

	if (TxFrequency == 0) {
		fprintf(stderr, "Need set a frequency to tx\n"); exit(0);
	}
	if (SymbolRate == 0) {
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

    #define BUFFER_SIZE 10000
    scmplx BufferIQ[BUFFER_SIZE];

	scmplx *BufferIQRx=(scmplx *)malloc(Burst*sizeof(scmplx));
    float fBufferIQ[BUFFER_SIZE*2];

    limesdr_init();
    limesdr_set_rxfreq(TxFrequency);
    limesdr_set_sr(SymbolRate,0);
    limesdr_set_rx_level(Gain);
	
	limesdr_receive();
	
    keep_running=true;
    int CountBuffer=0;
    while(keep_running)
    { 
        int LimeRead=lime_rx_samples(BufferIQRx, Burst);
        CountBuffer=(CountBuffer+1)%Partial;
        if((LimeRead>0)&&(CountBuffer==0))
    		SendToOutput(BufferIQRx,LimeRead);
    }

    limesdr_deinit();
        free(BufferIQRx);
}

