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
char previous_hour [8] = "00";
char hour [8];
char stream_state [8] = "off";
char ffmpegPID[256];

void Edge_ISR(void);
void Start_Function(void);
void Stop_Function(void);
void GetffmpegPID(char[256]);

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
        printf("Streamer always on\n");
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
    printf(" == keyedstream == Dave Crump <dave.g8gkq@gmail.com ==\n");
    printf(" == Based on code from Phil Crump                   ==\n");
    printf("  usage: keyedstream [x] [y]\n");
    printf("    x: GPIO to switch streamer on and off (optional)\n");
    printf("    Use 0 for always on with no keying\n");
    printf("    default is GPIO 1 (pin 12 on the RPi3)\n");
    printf("    y: GPIO for LED streaming indicator (optional)\n");
    printf("    default is nil, recommended is 7 (pin 7)\n");
    printf("GPIO high, streamer runs.  GPIO low, streamer stops.\n");
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
  snprintf(startCommand,64,"/home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null");
  snprintf(stopCommand1,64,"sudo killall ffmpeg >/dev/null 2>/dev/null");
    
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

    printf("Starting keyed streamer ....\n");
    printf("Use ctrl-C to exit \n");

    /* Trigger EdgeISR on any change */
    wiringPiISR(KeyGPIO, INT_EDGE_BOTH, Edge_ISR);

    /* Set initial state */
    if(digitalRead(KeyGPIO) == HIGH)
    {
      // Delay 5 seconds for .bashrc to finish
      usleep(5000000);
      // Run the stream for 5 seconds in a jumpy mode (don't know why)
      Start_Function();
      usleep(5000000);
      Stop_Function();
      // let it stabilise
      usleep(5000000);
      if(IndicationGPIO >= 0)
      {
        digitalWrite(IndicationGPIO, HIGH);
      }
      // Set the demanded stream state on so that it is started in the while loop
      strcpy(stream_state, "on");
    }
  }
  else
  {
    // Delay 5 seconds for .bashrc to finish
    usleep(5000000);
    // Run the stream for 5 seconds in a jumpy mode (don't know why)
    Start_Function();
    usleep(5000000);
    Stop_Function();
    // let it stabilise
    usleep(5000000);
    // Set the demanded stream state on so that it is started in the while loop
    strcpy(stream_state, "on");
  }

  /* Now wait here for interrupts - forever! */
  /* But stop/start the stream at 0300Z and 1500Z each day */
    
  /* Spin loop while waiting for interrupt */
  /* Check time for 12 hour restart        */
  /* and check process still running if    */
  /* required every 10 seconds             */

  while(1)
  {
    delay(10000);
    time ( &rawtime );
    timeinfo = gmtime ( &rawtime );
    strftime (hour, 8, "%I", timeinfo);

    if (strcmp(previous_hour, "02") == 0 && strcmp(hour, "03") == 0)
    {
      /* 3am or 3 pm, so stop and restart stream to zero delays */
      if (strcmp(stream_state, "on") == 0)
      {
        Stop_Function();
        delay(5000);
        Start_Function();
        printf("Stream restarted at %s o'clock", hour);
      }
      else
      {
        Stop_Function();
      }
    }
    strcpy(previous_hour, hour);
    if (strcmp(stream_state, "on") == 0)
    {
      GetffmpegPID(ffmpegPID);
      if (atoi(ffmpegPID) < 1)  // ffmpeg not running when it should be
      {
        Start_Function();
        printf("after crashing in the hour after %s o'clock\n", hour);
      }
    }
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

    /* Set timer to start stream on completion*/
    timer_set(&key_timer, 100, Start_Function);
  }
  else
  {
     /* Reset the timer in case it's already running */
    timer_reset(&key_timer);

    /* Set timer to stop stream on completion*/
    timer_set(&key_timer, 100, Stop_Function);
  }
}

void Start_Function(void)
{
  /* Start the stream */
  system(startCommand);
    
  if(IndicationGPIO >= 0)
  {
    digitalWrite(IndicationGPIO, HIGH);
  }
  strcpy(stream_state, "on");  
  printf("Starting stream\n");

}

void Stop_Function(void)
{
  /* Stop the stream */
  system(stopCommand1);

  if(IndicationGPIO >= 0)
  {
    digitalWrite(IndicationGPIO, LOW);
  }
  strcpy(stream_state, "off");  
  printf("Stopping stream\n");
}

/* GetffmpegPID returns a parameter that is a character string of the ffmpeg PID */
/* Parameter is 0 if ffmpeg not running */
void GetffmpegPID(char ffmpegPID[256])
{
  FILE *fp;
  /* Open the command for reading. */
  fp = popen("pgrep ffmpeg", "r");
  strcpy(ffmpegPID,"0");
  /* Read the output a line at a time - output it. */
  while (fgets(ffmpegPID, 7, fp) != NULL)
  {
    sprintf(ffmpegPID, "%d", atoi(ffmpegPID));
  }
  /* close */
  pclose(fp);
}

