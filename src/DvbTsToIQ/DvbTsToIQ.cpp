// DvbTsToIQ.cpp�: d�finit le point d'entr�e pour l'application console.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "../libdvbmod/libdvbmod.h"
#include <getopt.h>
#include <ctype.h>
#define PROGRAM_VERSION "0.0.1"

#ifndef WINDOWS

/* Non-windows includes */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <netinet/in.h> /* IPPROTO_IP, sockaddr_in, htons(), 
htonl() */
#include <arpa/inet.h>  /* inet_addr() */
#include <netdb.h>
#else

/* Windows-specific includes */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#endif /* WINDOWS */
#include <time.h>
#include <sys/ioctl.h>

FILE *input, *output;
enum
{
	DVBS,
	DVBS2
};
int Bitrate = 0;
int ModeDvb = 0;
#define BUFFER_SIZE (188)
int Pilot = 0;
unsigned int SymbolRate = 0;
int FEC = CR_1_2;
bool Simu=false;
float GainSimu=1.0; // Gain QSB
float TimingSimu=1.0; //Cycle QSB

static uint64_t _timestamp_ns(void)
{
	struct timespec tp;

	if (clock_gettime(CLOCK_REALTIME, &tp) != 0)
	{
		return (0);
	}

	return ((int64_t)tp.tv_sec * 1e9 + tp.tv_nsec);
}

unsigned int NullFiller(int NbPacket)
{
	unsigned char NullPacket[188] = {0x47, 0x1F, 0xFF};
	unsigned int TotalSampleWritten = 0;
	for (int i = 0; i < NbPacket; i++)
	{
		int len;
		if (ModeDvb == DVBS)
			len = DvbsAddTsPacket(NullPacket);
		if (ModeDvb == DVBS2)
			len = Dvbs2AddTsPacket(NullPacket);
		if (len != 0)
		{
			sfcmplx *Frame = NULL;
			//fprintf(stderr, "Len %d\n", len);
			if (ModeDvb == DVBS)
				Frame = Dvbs_get_IQ();
			if (ModeDvb == DVBS2)
				Frame = Dvbs2_get_IQ();

			fwrite(Frame, sizeof(sfcmplx), len, output);
			TotalSampleWritten += len;
		}
	}
	return TotalSampleWritten;
}

unsigned int CalibrateOutput()
{
	int n, ret;
	static uint64_t TimeBefore = 0;
	ret = ioctl(fileno(output), FIONREAD, &n);
	int n_start = 0;
	usleep(1e6);
	NullFiller(10000);

	ret = ioctl(fileno(output), FIONREAD, &n);
	n_start = n;
	TimeBefore = _timestamp_ns();
	fprintf(stderr, "Fillstart is %i %lu\n", n_start, TimeBefore);
	uint WaitSampleWritten = 0;
	while (WaitSampleWritten < SymbolRate)
	{
		WaitSampleWritten += NullFiller(1);
	}

	fprintf(stderr, "Fillstop is %i %lu\n", n, _timestamp_ns());
	unsigned int OutSampleRate = (WaitSampleWritten)*1e9 / (_timestamp_ns() - TimeBefore);
	return OutSampleRate;
}

bool Tune(int Frequency)
{
	
	#define LEN_CARRIER 1000
	static sfcmplx Frame[LEN_CARRIER];
	if(Frequency==0)
	for(int i=0;i<LEN_CARRIER;i++)
	{
		Frame[i].re=0x7fff;
		Frame[i].im=0;
	}
	else
	{
		//write a Chirp or Spectrum paint something ?
	}
	fwrite(Frame, sizeof(sfcmplx), LEN_CARRIER, output);
	return true;
}

bool RunWithFile(bool live)
{

	static unsigned char BufferTS[BUFFER_SIZE];

	static uint64_t DebugReceivedpacket = 0;
	//fprintf(stderr, "Output samplerate is %u\n", CalibrateOutput());
	if(FEC == -1)
	{
		Tune(0);
		return true; // Bypass real modulation
	}

	if (live)
	{
		int nin, nout;
		int ret = ioctl(fileno(input), FIONREAD, &nin);
		int ret2 = ioctl(fileno(output), FIONREAD, &nout);

		if ((ret == 0) && (nin < 18000))
		{

			while (nin < 1800)
			{
				if ((nout < 32000) )
				{
					NullFiller(100); //Should make restamping PCR !!!
					fprintf(stderr, "!! in =%d out%d\n", nin, nout);
					fprintf(stderr, "DVB2IQ Underflow -> Filling\n");
					
				}
				else
				{
					usleep(1);
					//fprintf(stderr, "OK in =%d out%d\n", nin, nout);
				}
				ret = ioctl(fileno(input), FIONREAD, &nin);
				
			}
		}
		else
		{
			//fprintf(stderr,"OK in =%d out%d\n",nin,nout);
		}
		if ((nin > 64000) && (nout > 64000))
			fprintf(stderr, "DVB2IQ fifoin %d fifoout %d\n", nin, nout);
	}
	int NbRead = fread(BufferTS, 1, BUFFER_SIZE, input);
	DebugReceivedpacket += NbRead;
	if (NbRead != BUFFER_SIZE)
	{
		if (!live)
			return false; //end of file
	}

	if (NbRead % 188 != 0)
		fprintf(stderr, "TS alignment Error\n");
	if (BufferTS[0] != 0x47)
	{
		fprintf(stderr, "TS Sync Error Try to Resynch\n");
		for (int i = 0; i < NbRead; i++)
		{
			if (BufferTS[i] == 0x47)
			{
				fread(BufferTS, 1, i, input); //Try to read garbage
				fprintf(stderr, "TS Sync recovery\n");
				break;
			}
		}
		fread(BufferTS, 1, BUFFER_SIZE, input); //Read the next aligned
	}
	for (int i = 0; i < NbRead; i += 188)
	{
		int len = 0;
		if ((BufferTS[i + 1] == 0x1F) && (BufferTS[i + 2] == 0xFF))
			continue; // Remove Null packets
		if (ModeDvb == DVBS)
			len = DvbsAddTsPacket(BufferTS + i);
		if (ModeDvb == DVBS2)
			len = Dvbs2AddTsPacket(BufferTS + i);

		if (len != 0)
		{
			sfcmplx *Frame = NULL;
			//fprintf(stderr, "Len %d\n", len);
			if (ModeDvb == DVBS)
				Frame = Dvbs_get_IQ();
			if (ModeDvb == DVBS2)
				Frame = Dvbs2_get_IQ();
			if(Simu==false)
				fwrite(Frame, sizeof(sfcmplx), len, output);
			/*else
			{
				sfcmplx *FrameSimu=(sfcmplx *)malloc(len*sizeof(sfcmplx));
				for(int j=0;j<len;j++)
				{
					static int ComptSample=0;
					FrameSimu[j].re=Frame[j].re*(0.5+0.5*GainSimu*sin(2*3.1415*(ComptSample/(SymbolRate*TimingSimu)))));
					FrameSimu[j].im=Frame[j].im*(0.5+0.5*GainSimu*sin(2*3.1415*(ComptSample/(SymbolRate*TimingSimu)))));
				}

			}*/

		}
	}
	int n, ret;

	return true;
}

// Global variable used by the signal handler and capture/encoding loop
static int want_quit = 0;

// Global signal handler for trapping SIGINT, SIGTERM, and SIGQUIT
static void signal_handler(int signal)
{
	want_quit = 1;
}

void print_usage()
{

	fprintf(stderr,
			"dvb2iq -%s\n\
Usage:\ndvb2iq -s SymbolRate [-i File Input] [-o File Output] [-f Fec]  [-m Modulation Type]  [-c Constellation Type] [-p] [-h] \n\
-i            Input Transport stream File (default stdin) \n\
-o            OutputIQFile (default stdout) \n\
-s            SymbolRate in KS (10-4000) \n\
-f            Fec : {1/2,3/4,5/6,7/8} for DVBS {1/4,1/3,2/5,1/2,3/5,2/3,3/4,5/6,7/8,8/9,9/10} for DVBS2 \n\
-m            Modulation Type {DVBS,DVBS2}\n\
-c 	      Constellation mapping (DVBS2) : {QPSK,8PSK,16APSK,32APSK}\n\
-p 	      Pilots on(DVBS2)\n\
-r 	      upsample (1,2,4) Better MER for low SR(<1M) choose 4\n\
-h            help (print this help).\n\
Example : ./dvb2iq -s 1000 -f 7/8 -m DVBS2 -c 8PSK -p\n\
\n",
			PROGRAM_VERSION);

} /* end function print_usage */

int main(int argc, char **argv)
{
	input = stdin;
	output = stdout;
	
	int Constellation = M_QPSK;
	int a;
	int anyargs = 0;
	int upsample = 1;

	ModeDvb = DVBS;
	while (1)
	{
		a = getopt(argc, argv, "i:o:s:f:c:hf:m:c:pr:");

		if (a == -1)
		{
			if (anyargs)
				break;
			else
				a = 'h'; //print usage and exit
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
			if (NULL == output)
			{
				fprintf(stderr, "Unable to open '%s': %s\n",
						optarg, strerror(errno));
				exit(EXIT_FAILURE);
			};
			break;
		case 's': // SymbolRate in KS
			SymbolRate = atoi(optarg) * 1000;
			break;
		case 'f': // FEC
			if (strcmp("1/2", optarg) == 0)
				FEC = CR_1_2;
			if (strcmp("2/3", optarg) == 0)
				FEC = CR_2_3;
			if (strcmp("3/4", optarg) == 0)
				FEC = CR_3_4;
			if (strcmp("5/6", optarg) == 0)
				FEC = CR_5_6;
			if (strcmp("7/8", optarg) == 0)
				FEC = CR_7_8;

			//DVBS2 specific
			if (strcmp("1/4", optarg) == 0)
				FEC = CR_1_4;
			if (strcmp("1/3", optarg) == 0)
				FEC = CR_1_3;
			if (strcmp("2/5", optarg) == 0)
				FEC = CR_2_5;
			if (strcmp("3/5", optarg) == 0)
				FEC = CR_3_5;
			if (strcmp("4/5", optarg) == 0)
				FEC = CR_4_5;
			if (strcmp("8/9", optarg) == 0)
				FEC = CR_8_9;
			if (strcmp("9/10", optarg) == 0)
				FEC = CR_9_10;

			if (strcmp("carrier", optarg) == 0)
			{
				FEC = -1;
			} //CARRIER MODE
			if (strcmp("test", optarg) == 0)
				FEC = -2; //TEST MODE
			break;
		case 'h': // help
			print_usage();
			exit(0);
			break;
		case 'l': // loop mode
			break;
		case 'm': //Modulation DVBS or DVBS2
			if (strcmp("DVBS", optarg) == 0)
				ModeDvb = DVBS;
			if (strcmp("DVBS2", optarg) == 0)
				ModeDvb = DVBS2;
		case 'c': // Constellation DVB S2
			if (strcmp("QPSK", optarg) == 0)
				Constellation = M_QPSK;
			if (strcmp("8PSK", optarg) == 0)
				Constellation = M_8PSK;
			if (strcmp("16APSK", optarg) == 0)
				Constellation = M_16APSK;
			if (strcmp("32APSK", optarg) == 0)
				Constellation = M_32APSK;
			break;
		case 'p':
			Pilot = 1;
			break;
		case 'r':
			upsample = atoi(optarg);
			if (upsample > 4)
				upsample = 4;
			break;
		case -1:
			break;
		case '?':
			if (isprint(optopt))
			{
				fprintf(stderr, "dvb2iq `-%c'.\n", optopt);
			}
			else
			{
				fprintf(stderr, "dvb2iq: unknown option character `\\x%x'.\n", optopt);
			}
			print_usage();

			exit(1);
			break;
		default:
			print_usage();
			exit(1);
			break;
		} /* end switch a */
	}	 /* end while getopt() */

	if (SymbolRate == 0)
	{
		fprintf(stderr, "SymbolRate is mandatory !\n");
		exit(0);
	}
	if ((ModeDvb == DVBS)&&(FEC>=0))
	{
		Bitrate = DvbsInit(SymbolRate, FEC, Constellation, upsample);
	}
	if ((ModeDvb == DVBS2)&&(FEC>=0))
	{
		Bitrate = Dvbs2Init(SymbolRate, FEC, Constellation, Pilot, RO_0_35, upsample);
	}

	fprintf(stderr, "Net TS bitrate input should be %d\n", Bitrate);
	bool isapipe = (fseek(input, 0, SEEK_CUR) < 0); //Dirty trick to see if it is a pipe or not
	if (isapipe)
		fprintf(stderr, "Using live mode\n");
	while (!want_quit && RunWithFile(isapipe))
		;

	return 0;
}
