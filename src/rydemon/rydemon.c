// App to monitor for touchscreen touches or web clicks to terminate a non-screen-aware appluication

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>

#include "touch.h"
#include "ffunc.h"

#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"

int TouchX;
int TouchY;
int TouchPressure;
int TouchTrigger = 0;
bool touchneedsinitialisation = true;
bool webcontrol = false;   // Enables webcontrol on a Portsdown 4
bool webclicklistenerrunning = false; // Used to only start thread if required
char WebClickForAction[7] = "no";  // no/yes
char ProgramName[255];             // used to pass prog name char string to listener
int *web_x_ptr;                // pointer
int *web_y_ptr;                // pointer
int web_x;                     // click x 0 - 799 from left
int web_y;                     // click y 0 - 480 from top
int exit_code;
static bool app_exit = false;
char DisplayType[31];
bool touchscreen_present = false;
int fd = 0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea

pthread_t thbutton;
pthread_t thwebclick;     //  Listens for mouse clicks from web interface
pthread_t thtouchscreen;  //  listens to the touchscreen   

void GetConfigParam(char *PathConfigFile, char *Param, char *Value);
void SetConfigParam(char *PathConfigFile, char *Param, char *Value);
void ReadSavedParams();
void *WaitTouchscreenEvent(void * arg);
void *WebClickListener(void * arg);
void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr);
FFUNC touchscreenClick(ffunc_session_t * session);
int openTouchScreen(int NoDevice);
int getTouchScreenDetails(int *screenXmin, int *screenXmax,int *screenYmin,int *screenYmax);
int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure);
int getTouchSample(int *rawX, int *rawY, int *rawPressure);
void wait_touch();
static void cleanexit(int calling_exit_code);
static void terminate(int sig);


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
  char ParamWithEquals[255];
  strcpy(ParamWithEquals, Param);
  strcat(ParamWithEquals, "=");

  //printf("Get Config reads %s for %s ", PathConfigFile , Param);

  FILE *fp=fopen(PathConfigFile, "r");
  if(fp != 0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line, ParamWithEquals, strlen(Param) + 1) == 0)
      {
        strcpy(Value, line+strlen(Param)+1);
        char *p;
        if((p=strchr(Value,'\n')) !=0 ) *p=0; //Remove \n
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


/***************************************************************************//**
 * @brief sets the value of Param in PathConfigFile from a program variable
 *        Used to store the configuration in portsdown_config.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void SetConfigParam(char *PathConfigFile, char *Param, char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  char Command[511];
  char BackupConfigName[240];
  strcpy(BackupConfigName,PathConfigFile);
  strcat(BackupConfigName,".bak");
  FILE *fp=fopen(PathConfigFile,"r");
  FILE *fw=fopen(BackupConfigName,"w+");
  char ParamWithEquals[255];
  strcpy(ParamWithEquals, Param);
  strcat(ParamWithEquals, "=");

  if(fp!=0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line, ParamWithEquals, strlen(Param) + 1) == 0)
      {
        fprintf(fw, "%s=%s\n", Param, Value);
      }
      else
      {
        fprintf(fw, line);
      }
    }
    fclose(fp);
    fclose(fw);
    snprintf(Command, 511, "cp %s %s", BackupConfigName, PathConfigFile);
    system(Command);
  }
  else
  {
    printf("Config file not found \n");
    fclose(fp);
    fclose(fw);
  }
}


/***************************************************************************//**
 * @brief Reads the Config file into global variables
 *
 * @param None
 *
 * @return void
 * 
*******************************************************************************/

void ReadSavedParams()
{
  char response[63] = "0";

  GetConfigParam(PATH_PCONFIG, "webcontrol", response);
  if (strcmp(response, "enabled") == 0)
  {
    webcontrol = true;
    pthread_create (&thwebclick, NULL, &WebClickListener, NULL);
    webclicklistenerrunning = true;
  }
  else
  {
    webcontrol = false;
    system("cp /home/pi/rpidatv/scripts/images/web_not_enabled.png /home/pi/tmp/screen.png");
  }
}


void *WaitTouchscreenEvent(void * arg)
{
  int TouchTriggerTemp;
  int rawX;
  int rawY;
  int rawPressure;
  while (true)
  {
    TouchTriggerTemp = getTouchSampleThread(&rawX, &rawY, &rawPressure);
    TouchX = rawX;
    TouchY = rawY;
    TouchPressure = rawPressure;
    TouchTrigger = TouchTriggerTemp;
  }
  return NULL;
}


void *WebClickListener(void * arg)
{
  while (webcontrol)
  {
	ffunc_run(ProgramName);
  }
  webclicklistenerrunning = false;
  return NULL;
}


void parseClickQuerystring(char *query_string, int *x_ptr, int *y_ptr)
{
  char *query_ptr = strdup(query_string),
  *tokens = query_ptr,
  *p = query_ptr;

  while ((p = strsep (&tokens, "&\n")))
  {
    char *var = strtok (p, "="),
         *val = NULL;
    if (var && (val = strtok (NULL, "=")))
    {
      if(strcmp("x", var) == 0)
      {
        *x_ptr = atoi(val);
      }
      else if(strcmp("y", var) == 0)
      {
        *y_ptr = atoi(val);
      }
    }
  }
}


FFUNC touchscreenClick(ffunc_session_t * session)
{
  ffunc_str_t payload;

  if( (webcontrol == false) || ffunc_read_body(session, &payload) )
  {
    if( webcontrol == false)
    {
      return;
    }

    ffunc_write_out(session, "Status: 200 OK\r\n");
    ffunc_write_out(session, "Content-Type: text/plain\r\n\r\n");
    ffunc_write_out(session, "%s\n", "click received.");
    fprintf(stderr, "Received click POST: %s (%d)\n", payload.data?payload.data:"", payload.len);

    int x = -1;
    int y = -1;
    parseClickQuerystring(payload.data, &x, &y);
    printf("After Parse: x: %d, y: %d\n", x, y);

    if((x >= 0) && (y >= 0))
    {
      web_x = x;                 // web_x is a global int
      web_y = y;                 // web_y is a global int
      strcpy(WebClickForAction, "yes");
      printf("Web Click Event x: %d, y: %d\n", web_x, web_y);
    }
  }
  else
  {
    ffunc_write_out(session, "Status: 400 Bad Request\r\n");
    ffunc_write_out(session, "Content-Type: text/plain\r\n\r\n");
    ffunc_write_out(session, "%s\n", "payload not found.");
  }
}


int openTouchScreen(int NoDevice)
{
  char sDevice[255];

  sprintf(sDevice, "/dev/input/event%d", NoDevice);
  if(fd != 0) close(fd);
  if ((fd = open(sDevice, O_RDONLY)) > 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


int getTouchScreenDetails(int *screenXmin, int *screenXmax,int *screenYmin,int *screenYmax)
{
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  char name[256] = "Unknown";
  int abs[6] = {0};

  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  //printf("Input device name: \"%s\"\n", name);

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
  //printf("Supported events:\n");

  int i,j,k;
  int IsAtouchDevice=0;
  for (i = 0; i < EV_MAX; i++)
  {
    if (test_bit(i, bit[0]))
    {
      //printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
      if (!i) continue;
      ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
      for (j = 0; j < KEY_MAX; j++)
      {
        if (test_bit(j, bit[i]))
        {
          //printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
          if(j==330) IsAtouchDevice=1;
          if (i == EV_ABS)
          {
            ioctl(fd, EVIOCGABS(j), abs);
            for (k = 0; k < 5; k++)
            {
              if ((k < 3) || abs[k])
              {
                //printf("     %s %6d\n", absval[k], abs[k]);
                if (j == 0)
                {
                  if ((strcmp(absval[k],"Min  ")==0)) *screenXmin =  abs[k];
                  if ((strcmp(absval[k],"Max  ")==0)) *screenXmax =  abs[k];
                }
                if (j == 1)
                {
                  if ((strcmp(absval[k],"Min  ")==0)) *screenYmin =  abs[k];
                  if ((strcmp(absval[k],"Max  ")==0)) *screenYmax =  abs[k];
                }
              }
            }
          }
        }
      }
    }
  }
  return IsAtouchDevice;
}


int getTouchSampleThread(int *rawX, int *rawY, int *rawPressure)
{
  int i;
  static bool awaitingtouchstart;
  static bool touchfinished;

  if (touchneedsinitialisation == true)
  {
    awaitingtouchstart = true;
    touchfinished = true;
    touchneedsinitialisation = false;
  }

  /* how many bytes were read */
  size_t rb;

  /* the events (up to 64 at once) */
  struct input_event ev[64];

  if (((strcmp(DisplayType, "Element14_7") == 0) || (strcmp(DisplayType, "Browser") == 0))
      && (strcmp(DisplayType, "dfrobot5") != 0))   // Browser or Element14_7, but not dfrobot5
  {
    // Thread flow blocks here until there is a touch event
    rb = read(fd, ev, sizeof(struct input_event) * 64);

    *rawX = -1;
    *rawY = -1;
    int StartTouch = 0;

    for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
    {
      if (ev[i].type ==  EV_SYN)
      {
        //printf("Event type is %s%s%s = Start of New Event\n",
        //        KYEL, events[ev[i].type], KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
      {
        StartTouch = 1;
        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
      {
        //StartTouch=0;
        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
	    *rawX = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawY = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value,KWHT);
        *rawPressure = ev[i].value;
      }

      if((*rawX != -1) && (*rawY != -1) && (StartTouch == 1))  // 1a
      {
        printf("7 inch Touchscreen Touch Event: rawX = %d, rawY = %d, rawPressure = %d\n", 
                *rawX, *rawY, *rawPressure);
        return 1;
      }
    }
  }

  if (strcmp(DisplayType, "dfrobot5") == 0)
  {
    // Program flow blocks here until there is a touch event
    rb = read(fd, ev, sizeof(struct input_event) * 64);

    if (awaitingtouchstart == true)
    {    
      *rawX = -1;
      *rawY = -1;
      touchfinished = false;
    }

    for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
    {
      //printf("rawX = %d, rawY = %d, rawPressure = %d, \n\n", *rawX, *rawY, *rawPressure);

      if (ev[i].type ==  EV_SYN)
      {
        //printf("Event type is %s%s%s = Start of New Event\n",
        //        KYEL, events[ev[i].type], KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
      {
        awaitingtouchstart = false;
        touchfinished = false;

        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
      {
        awaitingtouchstart = false;
        touchfinished = true;

        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n",
        //        KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
      }

      else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawX = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value, KWHT);
        *rawY = ev[i].value;
      }

      else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0)
      {
        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n",
        //        KYEL, events[ev[i].type], KWHT, KYEL, KWHT, KYEL, ev[i].value,KWHT);
        *rawPressure = ev[i].value;
      }

      if((*rawX != -1) && (*rawY != -1) && (touchfinished == true))  // 1a
      {
        printf("DFRobot Touch Event: rawX = %d, rawY = %d, rawPressure = %d\n", 
                *rawX, *rawY, *rawPressure);
        awaitingtouchstart = true;
        touchfinished = false;
        return 1;
      }
    }
  }
  return 0;
}


int getTouchSample(int *rawX, int *rawY, int *rawPressure)
{
  while (true)
  {
    if (TouchTrigger == 1)
    {
      *rawX = TouchX;
      *rawY = TouchY;
      *rawPressure = TouchPressure;
      printf("Touchtrigger was 1\n");
      TouchTrigger = 0;
      return 1;
    }
    else if ((webcontrol == true) && (strcmp(WebClickForAction, "yes") == 0))
    {
      *rawX = web_x;
      *rawY = web_y;
      *rawPressure = 0;
      strcpy(WebClickForAction, "no");
      printf("Web rawX = %d, rawY = %d, rawPressure = %d\n", *rawX, *rawY, *rawPressure);
      return 1;
    }
    else
    {
      usleep(1000);
    }
  }
  return 0;
}


void wait_touch()
// Wait for Screen touch, ignore position, but then move on
// Used to let user acknowledge displayed text
{
  int rawX, rawY, rawPressure;
  printf("wait_touch called\n");

  // Check if screen touched, if not, wait 0.1s and check again
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
  {
    usleep(100000);
  }
  // Screen has been touched
  printf("wait_touch exit\n");
}


static void cleanexit(int calling_exit_code)
{
  exit_code = calling_exit_code;
  app_exit = true;
  printf("Clean Exit Code %d\n", exit_code);
  usleep(1000000);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  exit(exit_code);
}


static void terminate(int sig)
{
  app_exit = true;
  printf("Terminating\n");
  usleep(1000000);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  exit(0);
}


int main(void)
{
  int NoDeviceEvent=0;
  wscreen = 800;
  hscreen = 480;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  char response[63];

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  // Check the display type in the config file
  strcpy(response, "Element14_7");
  GetConfigParam(PATH_PCONFIG, "display", response);
  strcpy(DisplayType, response);

  // Check for presence of touchscreen
  for(NoDeviceEvent = 0; NoDeviceEvent < 7; NoDeviceEvent++)
  {
    if (openTouchScreen(NoDeviceEvent) == 1)
    {
      if(getTouchScreenDetails(&screenXmin, &screenXmax, &screenYmin, &screenYmax) == 1) break;
    }
  }
  if(NoDeviceEvent != 7) 
  {
    // Create Touchscreen thread
    pthread_create (&thtouchscreen, NULL, &WaitTouchscreenEvent, NULL);
  }
  else // No touchscreen detected
  {
    touchscreen_present = false;

    if ((strcmp(DisplayType, "Browser") != 0) && (strcmp(DisplayType, "hdmi") != 0)
     && (strcmp(DisplayType, "hdmi480") != 0) && (strcmp(DisplayType, "hdmi720") != 0)
     && (strcmp(DisplayType, "hdmi1080") != 0))
    {  
      SetConfigParam(PATH_PCONFIG, "webcontrol", "enabled");
      SetConfigParam(PATH_PCONFIG, "display", "Browser");
      system ("/home/pi/rpidatv/scripts/set_display_config.sh");
      system ("sudo reboot now");
    }

    // Set Screen parameters
    screenXmax = 799;
    screenXmin = 0;
    wscreen = 800;
    screenYmax = 479;
    screenYmin = 0;
    hscreen = 480;
  }

  ReadSavedParams();

  // Calculate screen parameters
  scaleXvalue = ((float)(screenXmax - screenXmin)) / wscreen;
  scaleYvalue = ((float)(screenYmax - screenYmin)) / hscreen;

  wait_touch();  // Process blocks until screen is touched (here while Ryde runs)

  // Stop Ryde
  system("sudo killall python3 >/dev/null 2>/dev/null");
  system("sudo killall longmynd >/dev/null 2>/dev/null");

  // Start Portsdown
  cleanexit(129);
}


