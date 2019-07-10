/*
  ===========================================================================

  Copyright (C) 2018 Emvivre

  This file is part of LIMESDR_TOOLBOX.

  LIMESDR_TOOLBOX is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  LIMESDR_TOOLBOX is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with LIMESDR_TOOLBOX.  If not, see <http://www.gnu.org/licenses/>.

  ===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "limesdr_util.h"

// Global variable used by the signal handler and capture/encoding loop
static int want_quit = 0;

// Global signal handler for trapping SIGINT, SIGTERM, and SIGQUIT
static void signal_handler(int signal)
{
	want_quit = 1;
}

int main(int argc, char** argv)
{
  if ( argc < 2 )
  {
    printf("Usage: %s <OPTIONS>\n", argv[0]);
    printf("  -f <FREQUENCY>\n"
           "  -b <BANDWIDTH_CALIBRATING> (default: 8e6)\n"
           "  -s <SAMPLE_RATE> (default: 2e6)\n"
           "  -g <GAIN_NORMALIZED> (default: 1)\n"
           "  -l <BUFFER_SIZE> (default: 1024*1024)\n"
           "  -p <POSTPONE_EMITTING_SEC> (default: 3)\n"
           "  -d <DEVICE_INDEX> (default: 0)\n"
           "  -c <CHANNEL_INDEX> (default: 0)\n"
           "  -a <ANTENNA> (BAND1 | BAND2) (default: BAND1)\n"
           "  -r <RRC FILTER> (0 | 2 | 4) (default: 0)\n"
           "  -i <INPUT_FILENAME> (default: stdin)\n"
           "  -e <GPIO_BAND> (default: 0)\n"
           "  -q <CalibrationEnable> (default: 1)\n");
    return 1;
  }
	int i;
	unsigned int freq = 0;
	double bandwidth_calibrating = 8e6;
	double sample_rate = 2e6;
	double gain = 1;
	unsigned int buffer_size = 1024*10;
	double postpone_emitting_sec = 0.5;
	unsigned int device_i = 0;
	unsigned int channel = 0;
	int rrc=1;
	char* antenna = "BAND1";
	char* input_filename = NULL;
	uint8_t gpio_band = 0;
	bool WithCalibration=true;
	for ( i = 1; i < argc-1; i += 2 ) {
		if      (strcmp(argv[i], "-f") == 0) { freq = atof( argv[i+1] ); }
		else if (strcmp(argv[i], "-b") == 0) { bandwidth_calibrating = atof( argv[i+1] ); }
		else if (strcmp(argv[i], "-s") == 0) { sample_rate = atof( argv[i+1] ); }
		else if (strcmp(argv[i], "-g") == 0) { gain = atof( argv[i+1] ); }
		else if (strcmp(argv[i], "-l") == 0) { buffer_size = atoi( argv[i+1] ); }
		else if (strcmp(argv[i], "-p") == 0) { postpone_emitting_sec = atof( argv[i+1] ); }
		else if (strcmp(argv[i], "-d") == 0) { device_i = atoi( argv[i+1] ); }
		else if (strcmp(argv[i], "-c") == 0) { channel = atoi( argv[i+1] ); }
		else if (strcmp(argv[i], "-a") == 0) { antenna = argv[i+1]; }
		else if (strcmp(argv[i], "-r") == 0) { rrc = atoi( argv[i+1] ); }
		else if (strcmp(argv[i], "-i") == 0) { input_filename = argv[i+1]; }
                else if (strcmp(argv[i], "-e") == 0) { gpio_band = atoi( argv[i+1] ); }
		else if (strcmp(argv[i], "-q") == 0) { WithCalibration = atoi(argv[i+1]); }
	}
	if ( freq == 0 ) {
		fprintf( stderr, "ERROR: invalid frequency : %d\n", freq );
		return 1;
	}
	struct s16iq_sample_s {
	        short i;
		short q;
	} __attribute__((packed));
	struct s16iq_sample_s *buff = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);
	if ( buff == NULL ) {
		perror("malloc()");
		return 1;
	}
	FILE* fd = stdin;
	if ( input_filename != NULL ) {
		fd = fopen( input_filename, "rb" );
		if ( fd == NULL ) {
			perror("");
			return 1;
		}
	}
    bool isapipe = (fseek(fd, 0, SEEK_CUR) < 0); //Dirty trick to see if it is a pipe or not
	if (isapipe)
		fprintf(stderr, "Using IQ live mode\n");

	lms_device_t* device = NULL;
	double host_sample_rate;
	if(rrc>1) sample_rate=sample_rate*rrc; // Upsampling
	if ( limesdr_init( sample_rate,
			   freq,
			   bandwidth_calibrating,
			   gain,
			   device_i,
			   channel,
			   antenna,
			   LMS_CH_TX,
			   &device,
			   &host_sample_rate,
			   WithCalibration) < 0 ) {
		return 1;
	}
	fprintf(stderr, "sample_rate: %f\n", host_sample_rate);
	lms_stream_t tx_stream = {
		.channel = channel,
		.fifoSize = buffer_size,
		.throughputVsLatency = 0.2,
		.isTx = LMS_CH_TX,
		.dataFmt = LMS_FMT_I16
	};

    	// Set up GPIOs for output
	uint8_t gpio_dir = 0x8F; // set the 4 LSBs and the MSB to write
    	if (LMS_GPIODirWrite(device, &gpio_dir, 1) != 0) //1 byte buffer is enough to configure 8 GPIO pins on LimeSDR-USB
    	{
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
    	}
 
	// Make sure PTT is not set
	if (gpio_band >= 128)
	{
		 gpio_band = gpio_band - 128;
	}

	// Set band
   	if (LMS_GPIOWrite(device, &gpio_band, 1)!=0) //1 byte buffer is enough to write 8 GPIO pins on LimeSDR-USB
    	{
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
    	}

	if ( LMS_SetupStream(device, &tx_stream) < 0 ) {
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
	}
	if(SetGFIR(device,rrc)<0)
	{
		fprintf(stderr, "SetGFIR() : %s\n", LMS_GetLastErrorMessage());
		return -1;
	}

	// Set PTT
	if (gpio_band < 128)
	{
		 gpio_band = gpio_band + 128;
	}

	// Set PTT on
   	if (LMS_GPIOWrite(device, &gpio_band, 1)!=0) //1 byte buffer is enough to write 8 GPIO pins on LimeSDR-USB
    	{
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
    	}

	// Set  Fan on
   	LMS_WriteFPGAReg(device, 0xCC, 0x01);  // Enable manual fan control
        LMS_WriteFPGAReg(device, 0xCD, 0x01);  // Turn fan on


	//LMS_StartStream(&tx_stream);
	if(rrc>1)
		LMS_SetGFIR(device, LMS_CH_TX, 0, LMS_GFIR3, true);
	else
			LMS_SetGFIR(device, LMS_CH_TX, 0, LMS_GFIR3, false);
	lms_stream_meta_t tx_meta;
	tx_meta.waitForTimestamp = true;
	tx_meta.flushPartialPacket = false;

	/*LMS_EnableChannel(device, LMS_CH_RX, 0, true);
	lms_stream_t rx_stream = {
		.channel = 0,
		.fifoSize = buffer_size ,
		.throughputVsLatency = 0.2,
		.isTx = LMS_CH_RX,
		.dataFmt = LMS_FMT_I16
	};
        if ( LMS_SetupStream(device, &rx_stream) < 0 ) {
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
	}
	LMS_StartStream(&rx_stream);
	*/
	
	
	signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGKILL, signal_handler);
        signal(SIGPIPE, signal_handler);

	tx_meta.timestamp = postpone_emitting_sec * sample_rate;
	bool FirstTx=true;
	bool Transition=true;
	int TotalSampleSent=0;
	
	while( !want_quit ) {
		lms_stream_status_t Status;
		LMS_GetStreamStatus(&tx_stream,&Status);
		if(Status.fifoFilledCount<Status.fifoSize*0.4)
		{
				fprintf(stderr,"Fifo=%d/%d\n",Status.fifoFilledCount,Status.fifoSize);
				//memset(buff,0,buffer_size*sizeof(*buff));
				//LMS_SendStream( &tx_stream, buff, (Status.fifoSize-Status.fifoFilledCount)/sizeof( *buff ), NULL, 1000 );
		}		
		int nb_samples_to_send = fread( buff, sizeof( *buff ), buffer_size, fd );
		if(FirstTx)
		{
			
			LMS_StartStream(&tx_stream);
			FirstTx=false;
		}
		if ( nb_samples_to_send == 0 ) { // no more samples to send, quit if pipe, loop if a file
            if(!isapipe)
            {
                 fseek(fd, 0, SEEK_SET);
                 continue;
            }
            else
			    break;
		}
	    int nb_samples = LMS_SendStream( &tx_stream, buff, nb_samples_to_send, NULL/*&tx_meta*/, 1000 );
		TotalSampleSent+=nb_samples;
		if ( nb_samples < 0 ) {
			fprintf(stderr, "LMS_SendStream() : %s\n", LMS_GetLastErrorMessage());
			break;
		}
		if(Transition)
		{
			if(TotalSampleSent>sample_rate) // 1 second
			{
				LMS_SetNormalizedGain( device, LMS_CH_TX, channel, gain );
				Transition=false;		
			}
		}
		tx_meta.timestamp += nb_samples;
	}
	LMS_StopStream(&tx_stream);
	LMS_DestroyStream(device, &tx_stream);
	free( buff );
	fclose( fd );

	// Set PTT off
	gpio_band = gpio_band - 128;
	LMS_GPIOWrite(device, &gpio_band, 1);

	// Set  Fan auto
   	LMS_WriteFPGAReg(device, 0xCC, 0x00);  // Enable auto fan control


	LMS_EnableChannel( device, LMS_CH_TX, channel, false);
	LMS_Close(device);
	return 0;
}
