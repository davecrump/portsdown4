#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPi.h>

#include "timer.h"

int8_t IndicationGPIO;
int8_t ButtonGPIO;
_timer_t shutdown_button_timer;

void Edge_ISR(void);
void Shutdown_Function(void);

int main( int argc, char *argv[] )
{
    IndicationGPIO = -1;
    if( argc == 2 )
    {
        /* Only Input GPIO provided */
        ButtonGPIO = atoi(argv[1]);
        if(ButtonGPIO < 0 || ButtonGPIO > 20)
        {
            printf("ERROR: Input GPIO is out of bounds!\n");
            printf("Input GPIO was %d\n",ButtonGPIO);
            return 0;
        }
    }
    else if( argc == 3 ) 
    {
        /* Both Input and Output GPIO provided */
        ButtonGPIO = atoi(argv[1]);
        if(ButtonGPIO < 0 || ButtonGPIO > 20)
        {
            printf("ERROR: Button GPIO is out of bounds!\n");
            printf("Button GPIO was %d\n",ButtonGPIO);
            return 0;
        }
        IndicationGPIO = atoi(argv[2]);
        if(IndicationGPIO < 0 || IndicationGPIO > 20)
        {
            printf("ERROR: Indication GPIO is out of bounds!\n");
            printf("Indication GPIO was %d\n",IndicationGPIO);
            return 0;
        }
    }
    else
    {
        printf("ERROR: Incorrect number of parameters!\n");
        printf(" == pi-sdn == Phil Crump <phil@philcrump.co.uk ==\n");
        printf("  usage: pi-sdn x [y]\n");
        printf("    x: Button GPIO to listen for rising edge to trigger shutdown.\n");
        printf("    y: Indication GPIO to output HIGH, to detect successful shutdown (optional).\n");
        printf(" ----- \n");
        printf("Notes:\n");
        printf(" * The Button GPIO will be configured with the Pi's internal pulldown resistor (~50KOhm)\n");
        printf(" * The Indication GPIO will be reset to Input (High-Z) state on Shutdown.\n");
        printf("    An external pulldown resistor should be used to guarantee a voltage transition to LOW.\n");
        printf(" * WiringPi GPIO pin numbers are used. (0-20)\n");
        printf("    http://wiringpi.com/pins/\n");
        return 0;
    }
    
    /* Set up wiringPi module */
    if (wiringPiSetup() < 0)
	{
		return 0;
	}
    
    if(IndicationGPIO >= 0)
    {
        pinMode(IndicationGPIO, OUTPUT);
        digitalWrite(IndicationGPIO, HIGH);
    }

	/* Set up GPIOi as Input */
	pinMode(ButtonGPIO, INPUT);
	pullUpDnControl(ButtonGPIO, PUD_DOWN);
	wiringPiISR(ButtonGPIO, INT_EDGE_BOTH, Edge_ISR);
    
    /* Spin loop while waiting for ISR */
    while(1)
    {
        delay(10000);
    }
    
    return 0;
}

void Edge_ISR(void)
{
    if(digitalRead(ButtonGPIO) == HIGH)
    {
        /* Try to reset the timer incase it's already running */
        timer_reset(&shutdown_button_timer);

        /* Set timer */
        timer_set(&shutdown_button_timer, 100, Shutdown_Function);
    }
    else
    {
        /* Disarm timer */
        timer_reset(&shutdown_button_timer);
    }
}

void Shutdown_Function(void)
{
    /* Shut. It. Down */
    system("/home/pi/rpidatv/scripts/shutdown_script.sh &");
    exit;
}
