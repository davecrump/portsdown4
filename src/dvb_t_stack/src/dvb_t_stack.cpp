//============================================================================
// Name        : dvb_t_stack.cpp
// Author      : Charles Brain G4GUO
// Version     :
// Copyright   : No copyright
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "dvb_t.h"

#ifdef __USE_PLUTO__
#include "pluto.h"
#endif
#ifdef __USE_EXPRESS__
#include "express.h"
#endif
#ifdef __USE_LIME__
#include "lime.h"
#endif

using namespace std;

static bool m_main_loop;
static pthread_t m_tid[1];

//
// Create a NULL packet for testing
//
static uint8_t m_null[188];

//
// Format a transport NULL packet
//
void null_fmt( void )
{
    m_null[0] = 0x47;
    m_null[1] = 0;
    // Add the 13 bit pid
	m_null[1] = 0x1F;
	m_null[2] = 0xFF;
	m_null[3] = 0x10;
    for( int i = 4; i < 188; i++ ) m_null[i] = 0xFF;
}

void sigint_handler(int s){
    printf("Caught signal %d\n",s);
    m_main_loop = false;
	pluto_stop_tx_stream();
	dvb_t_udp_close();
	dvb_t_close();
    exit(1);
}

char *mfgets(char *b, int n, FILE *fp)
{
	int i = 0;
	int r;
	for(;;)
    {
		if((r=fgetc(fp)) != EOF)
        {
			b[i++] = (char)r&0xFF;;
			b[i] = 0;
			if(r == '\n')  return b;
			if(i >= n - 1) return b;
		}
        else
        {
			// Wait a millisecond before trying again
			usleep(1000);
		}
	}
	return b;
}

void *send_thread(void *arg){

    uint8_t tp[188*7];
    DVBTFormat *fmt = (DVBTFormat*)arg;
    //
	// Feed transport stream packets until there is something to transmit
    //
	while(m_main_loop == true){
		if(ts_q_read(tp, 188*7) == 188*7){
			for( int i = 0; i < 7; i++)	dvb_t_encode_and_modulate(&tp[188*i]);
		}
		else{
			dvb_t_encode_and_modulate(m_null);
		}
	}
#ifdef __USE_PLUTO__
	if(fmt->radio == R_PLUTO) pluto_stop_tx_stream();
#endif
#ifdef __USE_EXPRESS__
	if(fmt->radio == R_EXPRESS) express_deinit();
#endif
#ifdef __USE_LIME__
	if(fmt->radio == R_LIME) lime_close();
#endif
	dvb_t_udp_close();
	dvb_t_close();
	return arg;
}

void help_screen(char *name){
	printf("\n");
	printf("-m mode options qpsk 16qam 64qam (defaults to qpsk)\n");
	printf("-f frequency in Hz (defaults to 437 MHz)\n");
	printf("-a attenuator level radio specific (defaults to 0 )\n");
	printf("-r radio pluto lime express (defaults to pluto)\n");
	printf("-g Guard Interval 1/4 1/8 1/16 1/32 (defaults to 1/32)\n");
	printf("-b DVB-T/T2 Channel bandwidth in Hz (defaults to 500000 Hz)\n");
	printf("-p UDP port address (defaults to 1314)\n");
	printf("-t Transmission mode 2K 8K (defaults 2K)\n");
	printf("-e Error correction 1/2 2/3 3/4 5/6 7/8 (defaults to 1/2)\n");
	printf("-n Network address of radio (defaults to 192.168.2.1)\n");
	printf("-i Input control file (defaults to stdin)\n");
	printf("-h Help (this screen)\n");
	printf("\n");
	printf("Example setting transmitter to transmit 500 KHz QPSK on 437 MHz with FEC 1/2 and Guard 1/32\n");
	printf("%s -m qpsk -f 437000000 -b 500000 -e 1/2 -g 1/32\n",name);
	printf("\n");
}
#ifdef __USE_PLUTO__
int config_pluto(DVBTFormat *fmt){
	// initialise Pluto
	if(pluto_open( fmt, M32KS + M8KS ) <0 ) exit(0);

	// Register the Pluto TX  routine
	dvb_t_register_tx(pluto_tx_samples);

	// Set the sample rate
	if(fmt->tx_sample_rate > 3000000){
		pluto_set_tx_sample_rate( fmt->tx_sample_rate );
	}else{
		pluto_set_tx_sample_rate( fmt->tx_sample_rate * 8 );
		pluto_configure_x8_int_dec( fmt->tx_sample_rate );
	}
    // Set the TX frequency
	pluto_set_tx_frequency(fmt->freq);
	if((fmt->level <= 0 ) && (fmt->level > -89.75)){
		pluto_set_tx_level(fmt->level);
	}
	// Start TX streaming
	pluto_start_tx_stream();
	return 0;
}
#endif
#ifdef __USE_LIME__
int config_lime(DVBTFormat *fmt){
	int r = lime_open(fmt);
	dvb_t_register_tx(lime_transmit_samples);
    return r;
}
#endif
#ifdef __USE_EXPRESS__
int config_express(DVBTFormat *fmt){
    if( express_init( "datvexpress16.ihx", "datvexpressraw16.rbf" ) != EXP_OK) exit(0);
	dvb_t_register_tx(express_write_16_bit_samples);
    express_set_freq( fmt->freq );
    express_set_sr( fmt->tx_sample_rate );
    express_set_level( fmt->level );
    express_transmit();
	return 0;
}
#endif
int main(int c, char **argv){

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sigint_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	sigaction(SIGTERM, &sigIntHandler, NULL);

	DVBTFormat fmt;


	// Configure the transmitter
	fmt.co   = CO_64QAM;// Constellation
	fmt.sf   = SF_NH;// single frequency network
	fmt.gi   = GI_132;// guard interval
	fmt.tm   = TM_2K;// Transmission mode 2K or 8K
    fmt.fec  = FEC_12;// FEC code rate
	fmt.ir   = 2;// Interpolation ratio x4
	fmt.chan_bw_hz = 500000;
	fmt.freq = 437000000;// 437 MHz
	fmt.level = 100;
	fmt.port = 1314;
	fmt.radio = R_PLUTO;

	sprintf(fmt.n_addr,"192.168.2.1");
	// Name of File / Fifo for input commands
	char input_cmd_file[256];
	input_cmd_file[0] = 0;


	int skip;

	for( int i = 1; i < c;){
	   skip = 0;

	   if(strncmp(argv[i],"-m",2)==0){
	       if(strncmp(argv[i+1],"qpsk",4)==0){
		       fmt.co = CO_QPSK;
	       }
	       if(strncmp(argv[i+1],"16qam",5)==0){
	           fmt.co = CO_16QAM;
	       }
	       if(strncmp(argv[i+1],"32qam",5)==0){
	           fmt.co = CO_64QAM;
	       }
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-f",2)==0){
	       fmt.freq = strtoull(argv[i+1], NULL, 10);
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-g",2)==0){
	       if(strncmp(argv[i+1],"1/4",3)==0){
		       fmt.gi = GI_14;
	       }
	       if(strncmp(argv[i+1],"1/8",3)==0){
		       fmt.gi = GI_18;
	       }
	       if(strncmp(argv[i+1],"1/16",4)==0){
		       fmt.gi = GI_116;
	       }
	       if(strncmp(argv[i+1],"1/32",4)==0){
		       fmt.gi = GI_132;
	       }
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-b",2)==0){
		   fmt.chan_bw_hz = atoi(argv[i+1]);
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-t",2)==0){
	       if(strncmp(argv[i+1],"8K",2)==0){
		       fmt.tm = TM_8K;
	       }
	       if(strncmp(argv[i+1],"2K",3)==0){
		       fmt.tm = TM_2K;
	       }
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-e",2)==0){
	       if(strncmp(argv[i+1],"1/2",3)==0){
		       fmt.fec = FEC_12;
	       }
	       if(strncmp(argv[i+1],"2/3",3)==0){
		       fmt.fec = FEC_23;
	       }
	       if(strncmp(argv[i+1],"3/4",3)==0){
		       fmt.fec = FEC_34;
	       }
	       if(strncmp(argv[i+1],"5/6",3)==0){
		       fmt.fec = FEC_56;
	       }
	       if(strncmp(argv[i+1],"7/8",3)==0){
		       fmt.fec = FEC_78;
	       }
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-p",2)==0){
		   fmt.port = atoi(argv[i+1]);
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-n",2)==0){
		   strcpy(fmt.n_addr,argv[i+1]);
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-i",2)==0){
		   strcpy(input_cmd_file,argv[i+1]);
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-a",2)==0){
		   float l = atof(argv[i+1]);
		   fmt.level = l;
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-r",2)==0){
	       if(strncmp(argv[i+1],"pluto",5)==0){
		       fmt.radio = R_PLUTO;
	       }
	       if(strncmp(argv[i+1],"lime",4)==0){
		       fmt.radio = R_LIME;
	       }
	       if(strncmp(argv[i+1],"express",7)==0){
		       fmt.radio = R_EXPRESS;
	       }
	       skip = 2;
	   }
	   if(strncmp(argv[i],"-h",2)==0){
		   help_screen(argv[0]);
		   exit(0);
	   }
	   i += skip;
    }
	dvb_t_open();
	dvb_t_configure(&fmt);

	printf("Sample Rate %d\n",fmt.tx_sample_rate);
	printf("Channel capacity %d bps\n",fmt.chan_capacity);
	printf("Tx Frequency %llu\n",fmt.freq);

	int r = -1;
#ifdef __USE_PLUTO__
	if(fmt.radio == R_PLUTO)   r = config_pluto(&fmt);
#endif
#ifdef __USE_LIME__
	if(fmt.radio == R_LIME)    r = config_lime(&fmt);
#endif
#ifdef __USE_EXPRESS__
	if(fmt.radio == R_EXPRESS) r = config_express(&fmt);
#endif
	// Check to see radio is configured

	if(r == 0){

		//
		// Send something
		//
		null_fmt();

		if((r=dvb_t_udp_open(&fmt))<0){
			printf("Cannot open UDP socket %d\n",r);
		}

		m_main_loop = true;

		if(pthread_create(&(m_tid[0]), NULL, &send_thread, &fmt)!=0){
			printf("TS thread cannot be started\n");
			return -1;
		}
		char cmd[256];
		char *c;
		FILE *fp = stdin;
		bool display_prompt = true;
		if(strlen(input_cmd_file) > 0)
		{
			display_prompt = false;
			if((fp=fopen(input_cmd_file,"r"))==NULL){
				printf("Unable to open input command stream defaulting to stdin\n");
				fp = stdin;
				display_prompt = true;
			}
		}
		if(display_prompt == true) printf(">> ");
		while(m_main_loop == true)
        {
			usleep(10);
			if((c = mfgets(cmd, 10, fp))!= NULL)
            {
				if(cmd[0] == 'q')
                {
					m_main_loop = false;
				}
                else
                {
					cmd_parse(&fmt, c);
				}
				if(display_prompt == true)
                {
                    printf(">> ");
                }
			}
		}
        fclose(fp);
	}
	usleep(1000000);
   	printf("DVB-T exited\n");
	return 0;
}
