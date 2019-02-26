#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <time.h>
#include "timer.h"

int8_t IndicationGPIO;
int8_t KeyGPIO;
char sdnCommand[32];
char startCommand[64];
char stopCommand1[64];
_timer_t key_timer;
time_t rawtime;
struct tm * timeinfo;
char stream_state [8] = "off";

void Edge_ISR(void);
void Start_Function(void);
void Stop_Function(void);

int main( int argc, char *argv[] )
{
  /* Set Indication GPIO out of bounds to detect valid user input */
  IndicationGPIO = -1; 

  /* Sort out which GPIO Pins to be used */
  if( argc == 1)
  {
    printf("No arguments, setting key to GPIO 1 on pin 12.\n");
    KeyGPIO = 1;
  }  
  else if( argc == 2 )
  {
    /* Key GPIO provided */
    KeyGPIO = atoi(argv[1]);
    if(KeyGPIO < 0 || KeyGPIO > 20)
    {
      printf("ERROR: Key GPIO is out of bounds!\n");
      printf("Key GPIO was %d\n",KeyGPIO);
      return 0;
    }
    else
    {
      if( KeyGPIO == 0 )
      {
        printf("TX always on\n");
      }
      else
      {
        printf("Key GPIO is %d, No LED Indication\n",KeyGPIO);
      }
    }
  }
  else if( argc == 3 )
  {
    /* Both Key and Indication GPIOs provided */
    KeyGPIO = atoi(argv[1]);
    if(KeyGPIO < 1 || KeyGPIO > 20)
    {
      printf("ERROR: Key GPIO is out of bounds!\n");
      printf("Key GPIO was %d\n",KeyGPIO);
      return 0;
    }
    IndicationGPIO = atoi(argv[2]);
    if(IndicationGPIO < 0 || IndicationGPIO > 20)
    {
      printf("ERROR: Indication GPIO is out of bounds!\n");
      printf("Indication GPIO was %d\n",IndicationGPIO);
      return 0;
    }
    else
    {
      printf("Key GPIO is %d, LED GPIO is %d \n",KeyGPIO, IndicationGPIO);
    }
  }
  else
  {
    printf("ERROR: Incorrect number of parameters!\n");
    printf(" == keyedtx == Dave Crump <dave.g8gkq@gmail.com     ==\n");
    printf(" == Based on code from Phil Crump                   ==\n");
    printf("  usage: keyedtx [x] [y]\n");
    printf("    x: GPIO to switch TX on and off (optional)\n");
    printf("    Use 0 for always on with no keying\n");
    printf("    default is GPIO 1 (pin 12 on the RPi3)\n");
    printf("    y: GPIO for LED transmitting indicator (optional)\n");
    printf("    default is nil, recommended is 7 (pin 7)\n");
    printf("GPIO high, Transmitter runs.  GPIO low, transmitter stops.\n");
    printf(" * WiringPi GPIO pin numbers are used. (0-20)\n");
    printf("    http://wiringpi.com/pins/\n");
    return 0;
  }
    
  /* Set up wiringPi module */
  if (wiringPiSetup() < 0)
  {
    return 0;
  }
  
  /* Set up commands in buffers */
  snprintf(sdnCommand,32,"shutdown -h now");
  snprintf(startCommand,64,"sudo /home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null");
  snprintf(stopCommand1,64,"sudo /home/pi/rpidatv/scripts/b.sh 2>/dev/null");
    
  if(KeyGPIO > 0)
  {
    /* Set up KeyGPIO as Input */
    pinMode(KeyGPIO, INPUT);

    /* Set up indication GPIO */
    if(IndicationGPIO >= 0)
    {
      pinMode(IndicationGPIO, OUTPUT);
      digitalWrite(IndicationGPIO, LOW);
    }

    printf("Starting keyed transmitter ....\n");
    printf("Use ctrl-C to exit \n");

    /* Trigger EdgeISR on any change */
    wiringPiISR(KeyGPIO, INT_EDGE_BOTH, Edge_ISR);

    /* Set initial state */
    if(digitalRead(KeyGPIO) == HIGH)
    {
      /* Turn the transmitter on */
      Start_Function();
      if(IndicationGPIO >= 0)
      {
        digitalWrite(IndicationGPIO, HIGH);
      }
    }
  }
  else
  {
    /* Start the always-on transmitter */
    printf("Starting Always on transmitter ....\n");
    printf("Use ctrl-C to exit \n");
    Start_Function();
  }

  /* Now wait here for interrupts - forever! */
    
  /* Spin loop while waiting for interrupt */

  while(1)
  {
    delay(10000);
  }
  return 0;
}

void Edge_ISR(void)
{
  /* Each time this is called, it starts the timer and tries to let it run to completion */
  /* If it gets interrupted (by switch noise) it starts again */

  if(digitalRead(KeyGPIO) == HIGH)
  {
    /* Reset the timer in case it's already running */
    timer_reset(&key_timer);

    /* Set timer to start transmit on completion*/
    timer_set(&key_timer, 100, Start_Function);
  }
  else
  {
     /* Reset the timer in case it's already running */
    timer_reset(&key_timer);

    /* Set timer to stop transmit on completion*/
    timer_set(&key_timer, 100, Stop_Function);
  }
}

void Start_Function(void)
{
  /* Start the transmission */
  system(startCommand);

  // start the PTT for Lime after a delay
  // Should have no effect in other modes as PTT will already be active from rpidatv
  system("/home/pi/rpidatv/scripts/lime_ptt.sh &");

  if(IndicationGPIO >= 0)
  {
    digitalWrite(IndicationGPIO, HIGH);
  }
  strcpy(stream_state, "on");  
  printf("Starting transmission\n");

}

void Stop_Function(void)
{
  /* Stop the transmission */
  system(stopCommand1);

  if(IndicationGPIO >= 0)
  {
    digitalWrite(IndicationGPIO, LOW);
  }
  strcpy(stream_state, "off");  
  printf("Stopping transmission\n");
}
