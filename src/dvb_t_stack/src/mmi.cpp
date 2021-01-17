#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "dvb_t.h"
#include "pluto.h"
#include "express.h"
#include "lime.h"

void pluto_help(void){
	printf("a 10        (set 10 dB of attenuation)\n");
	printf("f 437000000 (set frequency to 437 MHz)\n");
	printf("tx          (transmit)\n");
	printf("rx          (receive)\n");
	printf("q           (quit)\n");
}
void express_help(void){
	printf("a 10        (set 10 dB of attenuation)\n");
	printf("f 437000000 (set frequency to 437 MHz)\n");
	printf("tx          (transmit)\n");
	printf("rx          (receive)\n");
	printf("q           (quit)\n");
}
void lime_help(void){
	printf("g 0.0 - 1.0 (set power gain)\n");
	printf("p 1 - 2     (set output port)\n");
	printf("f 437000000 (set frequency to 437 MHz)\n");
	printf("tx          (transmit)\n");
	printf("rx          (receive)\n");
	printf("q           (quit)\n");
}
void cmd_parse(DVBTFormat *fmt, char *cmd)
{
	int i = 0;
	bool valid = false;
	char *token[10];
	token[i] = strtok(cmd," ");
	while(token[i] != NULL){
		if( ++i == 10 ) break;
		token[i] = strtok(NULL," ");
	}
#ifdef __USE_PLUTO__
	//
	// Pluto commands
	//
	if(fmt->radio == R_PLUTO){
		if(strncmp(token[0],"a",1)==0){
			float att = atof(token[1]);
			if((att > 0 ) && (att < 89.75)){
				pluto_set_tx_level(-att);
				valid = true;
			}
		}
		if(strncmp(token[0],"f",1)==0){
			uint64_t freq = atol(token[1]);
			pluto_set_tx_frequency(freq);
			valid = true;
		}
		if(strncmp(token[0],"tx",2)==0){
			pluto_transmit();
			valid = true;
		}
		if(strncmp(token[0],"rx",2)==0){
			pluto_receive();
			valid = true;
		}
		if(strncmp(token[0],"h",1)==0){
			pluto_help();
			valid = true;
		}
	}
#endif

#ifdef __USE_EXPRESS__
	//
	// Express commands
	//
	if(fmt->radio == R_EXPRESS){
		if(strncmp(token[0],"a",1)==0){
			int att = atoi(token[1]);
			if((att >= 0 ) && (att <= 47 )){
				express_set_level(att);
				valid = true;
			}
		}
		if(strncmp(token[0],"f",1)==0){
			uint64_t freq = atol(token[1]);
			express_set_freq(freq);
			valid = true;
		}
		if(strncmp(token[0],"tx",2)==0){
			express_set_ports(0x01);
			valid = true;
		}
		if(strncmp(token[0],"rx",2)==0){
			express_set_ports(0x00);
			valid = true;
		}
		if(strncmp(token[0],"h",1)==0){
			express_help();
			valid = true;
		}
	}
#endif

#ifdef __USE_LIME__
	if(fmt->radio == R_LIME){
		if(strncmp(token[0],"g",1)==0){
			float gain = atof(token[1]);
			if((gain <= 1.0)&&(gain>0)){
				lime_set_gain(gain);
				valid = true;
			}
		}
		if(strncmp(token[0],"f",1)==0){
			uint64_t freq = atol(token[1]);
			lime_set_freq(freq);
			valid = true;
		}
		if(strncmp(token[0],"p",1)==0){
			int port = atoi(token[1]);
			lime_set_port(port);
			valid = true;
		}
		if(strncmp(token[0],"tx",2)==0){
			lime_transmit();
			valid = true;
		}
		if(strncmp(token[0],"rx",2)==0){
			lime_receive();
			valid = true;
		}
		if(strncmp(token[0],"h",1)==0){
			lime_help();
			valid = true;
		}
	}
#endif
	if(valid == false) printf("Invalid command\n");
}
