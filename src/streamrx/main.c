#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#define PATH_STREAMPRESETS "/home/pi/rpidatv/scripts/stream_presets.txt"

int8_t IndicationGPIO;
char startCommand[127];
time_t rawtime;
struct tm * timeinfo;
char previous_hour [8] = "00";
char hour [8];
char Value[127] = "";
char Param[20];
int count;
int StreamStatus;
int i;
char DisplayCaption[127];

pthread_t dailyreset;    // thread to reset VLC every hour

void *DailyReset(void * arg);
int CheckVLCStream(void);
void DisplayHere(void);

/***************************************************************************//**
 * @brief Looks up the value of a Param in PathConfigFile and sets value
 *        Used to look up the configuration from portsdown_config.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void GetConfigParam(char *PathConfigFile, char *Param, char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  FILE *fp=fopen(PathConfigFile,"r");
  if(fp!=0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line,Param,strlen(Param)) == 0)
      {
        strcpy(Value,line+strlen(Param)+1);
        char *p;
        if((p=strchr(Value,'\n'))!=0) *p=0; //Remove \n
        break;
      }
    }
  }
  else
  {
    printf("Config file not found \n");
  }
  fclose(fp);
}

static void
terminate(int dummy)
{
  printf("Terminate\n");
  digitalWrite(IndicationGPIO, LOW);
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png >/dev/null 2>/dev/null");  // Restore logo image
  system("/home/pi/rpidatv/scripts/vlc_stream_player_stop.sh");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
  exit(1);
}


int main(int argc, char *argv[])
{

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  /* Set Indication GPIO out of bounds to detect valid user input */
  IndicationGPIO = -1; 

  /* Sort out which GPIO Pins to be used */
  if( argc == 1)
  {
    printf("No arguments, setting indication to GPIO 7 on pin 7.\n");
    IndicationGPIO = 7;
  }  
  else if( argc == 2 )
  {
    /* Indication GPIO provided */
    IndicationGPIO = atoi(argv[2]);
    if(IndicationGPIO < 0 || IndicationGPIO > 20)
    {
      printf("ERROR: Indication GPIO is out of bounds!\n");
      printf("Indication GPIO was %d\n",IndicationGPIO);
      return 0;
    }
    else
    {
      printf("Indication GPIO is %d \n", IndicationGPIO);
    }
  }
  else
  {
    printf("ERROR: Incorrect number of parameters!\n");
    printf(" == streamrx VLC == Dave Crump <dave.g8gkq@gmail.com ==\n");
    printf(" == Based on code from Phil Crump                       ==\n");
    printf("  usage: streamrx [x]\n");
    printf("    x: GPIO for streaming indicator (optional)\n");
    printf("    default is 7, recommended is 7 (pin 7)\n");
    printf("    GPIO goes high when stream is received.\n");  
    printf("    GPIO goes low when stream stops.\n");
    printf(" * WiringPi GPIO pin numbers are used. (0-20)\n");
    printf("    http://wiringpi.com/pins/\n");
    return 0;
  }
    
  /* Set up wiringPi module */
  if (wiringPiSetup() < 0)
  {
    return 0;
  }
  
  /* Set up indication GPIO */
  pinMode(IndicationGPIO, OUTPUT);
  digitalWrite(IndicationGPIO, LOW);

  // Read in the streamname and set up the command
  GetConfigParam(PATH_STREAMPRESETS, "stream0", Value);

  strcpy(startCommand, "/home/pi/rpidatv/scripts/vlc_stream_player.sh ");
  strcat(startCommand, Value);
  strcat(startCommand, " &");

  printf("Starting VLC Stream receiver on %s \n", Value);
  printf("Use ctrl-C to exit \n");

  strcpy(DisplayCaption, "Waiting for Stream");
  DisplayHere();

  // open the dailyreset (actually hourly) thread here
  pthread_create (&dailyreset, NULL, &DailyReset, NULL);

  while(1)
  {
    // With no stream, this loop is executed about once every 10 seconds

    // first make sure that the stream status is not stale
    system("rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
    usleep(500000);

    // run the VLC player script
    system(startCommand);

    StreamStatus = CheckVLCStream();
    //printf("Stream Status File value = %d\n", StreamStatus);

    // = 0 Stream running
    // = 1 Not started yet
    // = 3 terminated
    
    // Now wait for VLC to respond
    // checking every 0.5 seconds. 

    while (StreamStatus == 1)
    {
      usleep(500000); 
      StreamStatus = CheckVLCStream();
      //printf("Stream Status File value = %d\n", StreamStatus);
    }

    // If it is running properly, set GPIO high and wait here
    if (StreamStatus == 0)
    {
      digitalWrite(IndicationGPIO, HIGH);
      strcpy(DisplayCaption, "Valid Stream Detected");
      DisplayHere();

      while (StreamStatus == 0)
      {
        // Wait in this loop while the stream is running
        usleep(500000); // Check every 0.5 seconds
        StreamStatus = CheckVLCStream();
        // printf("Stream Status File value = %d\n", StreamStatus);
      }

      strcpy(DisplayCaption, "Stream Dropped Out");
      DisplayHere();
      usleep(500000); // Display dropout message for 0.5 sec
    }

    // So stream has either stopped, or not started properly
    digitalWrite(IndicationGPIO, LOW);
    
    if ((StreamStatus == 3) || (StreamStatus == 1))  // Nothing detected
    {
      strcpy(DisplayCaption, "Waiting for Stream");
      DisplayHere();
    }

    // Make sure that VLC player is no longer running
    system("/home/pi/rpidatv/scripts/vlc_stream_player_stop.sh");

  }

  // code will never get to here
  // close the dailyreset thread to be tidy
  pthread_join(dailyreset, NULL);
}

int CheckVLCStream()
{
  // first check file exists, if not, return 1
  // then read first 5 characters of file
  // If less than 5 characters (invalid) return 1
  // if Video, return 0
  // else return 3

  FILE *fp;
  char response[127] = "";

  fp = popen("cat /home/pi/tmp/stream_status.txt 2>/dev/null", "r");
  if (fp == NULL)
  {
    //printf("Failed to run command\n" );
    return 1;
  }

  // Read the output a line at a time
  while (fgets(response, 127, fp) != NULL)
  {
    //printf("Response was %s", response);
  }
  pclose(fp);

  if (strlen(response) < 5)  // Null string or single <CR>
  {
    return 1;
  }
 
  response[5] = '\0'; // Truncate response to 5 characters

  if (strcmp(response, "Video") == 0)
  {
    return 0;
  }
  else
  {
    return 3;
  }
}

void DisplayHere(void)
{
  // Displays a caption on top of the standard logo
  char ConvertCommand[255];

  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous fbi processes
  system("rm /home/pi/tmp/captionlogo.png >/dev/null 2>/dev/null");
  system("rm /home/pi/tmp/streamcaption.png >/dev/null 2>/dev/null");

  strcpy(ConvertCommand, "convert -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 \"");
  strcat(ConvertCommand, DisplayCaption);
  strcat(ConvertCommand, "\" /home/pi/tmp/captionlogo.png");
  system(ConvertCommand);

  strcpy(ConvertCommand, "convert /home/pi/rpidatv/scripts/images/BATC_Black.png /home/pi/tmp/captionlogo.png ");
  strcat(ConvertCommand, "-geometry +0+475 -composite /home/pi/tmp/streamcaption.png");
  system(ConvertCommand);

  system("sudo fbi -T 1 -noverbose -a /home/pi/tmp/streamcaption.png  >/dev/null 2>/dev/null");  // Add logo image
}

void *DailyReset(void * arg)  // Actually an hourly reset and 12-hourly reboot
{
  while(1)
  {
    delay(10000); // 10 seconds
    time ( &rawtime );
    timeinfo = gmtime ( &rawtime );
    strftime (hour, 8, "%I", timeinfo);

/*  // Not to be used until reboots are reliable!!
    // 12-hourly reboot 
    if (strcmp(previous_hour, "02") == 0 && strcmp(hour, "03") == 0)
    {
      // 3am or 3 pm, so reboot
      system("sudo reboot now");
    }
*/
    if (strcmp(previous_hour, hour) != 0)
    {
      // Top of the hour, so stop omxplayer.  Main will restart it.
      system("sudo killall -9 omxplayer.bin >/dev/null 2>/dev/null");
      // and delete the status file so the main while loop needs to restart
      system("sudo rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
      // and set the GPIO low in case it has hung
      digitalWrite(IndicationGPIO, LOW);
    }
    strcpy(previous_hour, hour);
  }
  return 0;
}

