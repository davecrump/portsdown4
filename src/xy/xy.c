#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include "touch.h"
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

//#include <wiringPi.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>
#include "VG/openvg.h"
#include "VG/vgu.h"


#include "mcp3002.h"

#include "fontinfo.h"
#include "shapes.h"

pthread_t thbutton;

int fd=0;
int wscreen, hscreen;

int FinishedButton = 0;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea


typedef struct {
	int r,g,b;
} color_t;

typedef struct {
	char Text[255];
	color_t  Color;
} status_t;

void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
}

void ShowTitle()
{
  // Initialise and calculate the text display
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  VGfloat linenumber = 1.0;
  VGfloat tw;

  // Display Text
  tw = TextWidth("X-Y Display", font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, "X-Y Display", font, pointsize);
}


void DisplayLogo()
{
  finish();
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

int openTouchScreen(int NoDevice)
{
  char sDevice[255];

  sprintf(sDevice,"/dev/input/event%d",NoDevice);
  if(fd!=0) close(fd);
  if ((fd = open(sDevice, O_RDONLY)) > 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int getTouchScreenDetails(int *screenXmin,int *screenXmax,int *screenYmin,int *screenYmax)
{
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
  char name[256] = "Unknown";
  int abs[6] = {0};

  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  printf("Input device name: \"%s\"\n", name);

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
  printf("Supported events:\n");

  int i,j,k;
  int IsAtouchDevice=0;
  for (i = 0; i < EV_MAX; i++)
  if (test_bit(i, bit[0])) 
  {
    printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
    if (!i) continue;
    ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
    for (j = 0; j < KEY_MAX; j++)
    {
      if (test_bit(j, bit[i]))
      {
        printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
        if(j==330) IsAtouchDevice=1;
        if (i == EV_ABS)
        {
          ioctl(fd, EVIOCGABS(j), abs);
          for (k = 0; k < 5; k++)
          if ((k < 3) || abs[k])
          {
            printf("     %s %6d\n", absval[k], abs[k]);
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
  return IsAtouchDevice;
}

int getTouchSample(int *rawX, int *rawY, int *rawPressure)
{
	int i;
        /* how many bytes were read */
        size_t rb;
        /* the events (up to 64 at once) */
        struct input_event ev[64];
	//static int Last_event=0; //not used?
	rb=read(fd,ev,sizeof(struct input_event)*64);
	*rawX=-1;*rawY=-1;
	int StartTouch=0;
        for (i = 0;  i <  (rb / sizeof(struct input_event)); i++){
              if (ev[i].type ==  EV_SYN)
		{
                         //printf("Event type is %s%s%s = Start of New Event\n",KYEL,events[ev[i].type],KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 1)
		{
			StartTouch=1;
                        //printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s1%s = Touch Starting\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_KEY && ev[i].code == 330 && ev[i].value == 0)
		{
			//StartTouch=0;
			//printf("Event type is %s%s%s & Event code is %sTOUCH(330)%s & Event value is %s0%s = Touch Finished\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,KWHT);
		}
                else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawX = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawY = ev[i].value;
		}
                else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0){
                        //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
			*rawPressure = ev[i].value;
		}
		if((*rawX!=-1)&&(*rawY!=-1)&&(StartTouch==1))
		{
			/*if(Last_event-mymillis()>500)
			{
				Last_event=mymillis();
				return 1;
			}*/
			//StartTouch=0;
			return 1;
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


static void
terminate(int dummy)
{
  printf("Terminate\n");
  finish();
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}


/***************************************************************************//**
 * @brief 
 *
 * @param 
 *
 * @return 
*******************************************************************************/


int main()
{

  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  int NoDeviceEvent=0;
  VGfloat x1, y1, x2, y2;
  int xin, yin;

  saveterm();
  init(&wscreen, &hscreen);
  rawterm();

  // Catch sigaction and call terminate
  for (i = 0; i < 16; i++)
  {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = terminate;
    sigaction(i, &sa, NULL);
  }

  // Set up wiringPi module
  if (wiringPiSetup() < 0)
  {
    return 0;
  }

  // Check for presence of touchscreen
  for(NoDeviceEvent=0;NoDeviceEvent<5;NoDeviceEvent++)
  {
    if (openTouchScreen(NoDeviceEvent) == 1)
    {
      if(getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax)==1) break;
    }
  }
  if(NoDeviceEvent==5) 
  {
    perror("No Touchscreen found");
    exit(1);
  }


  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  //printf ("X Scale Factor = %f\n", scaleXvalue);
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;

  int x0 = 150;
  int y0 = 30;

  int xv[501];
  int yv[501];

  Start(wscreen,hscreen);
 
  ShowTitle();

  // Label Y axis

  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
  int pointsize = 10;

  Text(x0 - 25, y0 + 400 - 5,   "0", font, pointsize);
  Text(x0 - 25, y0 + 350,     "-10", font, pointsize);
  Text(x0 - 25, y0 + 300 - 5, "-20", font, pointsize);
  Text(x0 - 25, y0 + 250,     "-30", font, pointsize);
  Text(x0 - 25, y0 + 200 - 5, "-40", font, pointsize);
  Text(x0 - 25, y0 + 150,     "-50", font, pointsize);
  Text(x0 - 25, y0 + 100 - 5, "-60", font, pointsize);
  Text(x0 - 25, y0 + 50 - 5,  "-70", font, pointsize);
  Text(x0 - 25, y0 + 0 - 5,   "-80", font, pointsize);

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  while (FinishedButton == 0)
  {
    // Draw the graticule     
    StrokeWidth(1);
    Stroke(150, 150, 200, 0.8);

    for (i = 0; i < 9; i = i + 1)
    {
      Line(x0, y0 + (i * 50), x0 + 500, y0 + (i * 50));
    }

    for (i = 0; i < 11; i = i + 1)
    {
      Line(x0 + (i * 50), y0, x0 + (i * 50), y0 + 400);
    }
    // Centre X line brighter
    Stroke(250, 250, 250, 0.8);
    Line(x0 + 250, y0, x0 + 250, y0 + 400);

    StrokeWidth(2);
    Stroke(150, 150, 200, 0.8);

    for (i = 0; i < 11; i = i + 1)
    {
      xv[0] = mcp3002_value(0);
      yv[0] = mcp3002_value(1);
    }

    for (i = 0; i < 501; i = i + 1)
    {
      xv[i] = mcp3002_value(0);
      //usleep(2);
      yv[i] = mcp3002_value(1);
      //usleep(2);
    }

    // Now build picture from sweep
  
    // Check for first flyback
    int flyback = 0;

    for (i = 0; i < 499; i = i + 1)
    {
      if ((xv[i] > 900) && (xv[i] < 1020) && (xv[i + 2] < 250) && (xv[i + 2] >150) && (flyback == 0))
      {
         //flyback is at i+1 or i+2
        if (xv[i + 1] < 609)
        {
          // Flyback is at i+1
          flyback = i + 1;
        }
        else
        {
          // Flyback is at i+2
          flyback = i + 2;
        }
      }
    }
  
    // X scan goes from 200 to 1010

    // Now build matrix of 501 points
    int point[501];
    int ramp[501];
    int currentsample;
    currentsample = flyback;

    for (i = 1; i < 501; i = i + 1)
    {
      point[i] = 0;
      if (yv[currentsample] > 1023)
      {
        yv[currentsample] = 0;
      }
 
      if ((xv[i] - 200) > ((i / 501) * 810))
      {
        point[i] = yv[currentsample]*400/1024;
        currentsample = currentsample + 1;
      }
      else
      {
        point[i] = yv[currentsample]*400/1024;
      }
      ramp[i] = xv[currentsample];
    }  
    point[0] = point[1];
    ramp[0] = ramp[1];
 
    for (i = 0; i < 500; i = i + 1)
    {
      //Line(x1, y1, x2, y2 VGfloat)
      //printf("i = %d, y = %d\n", i, yv[i]);
      //printf("i = %d, x = %d, point = %d\n", i, ramp[i], point[i]);
      Line(i + x0, y0 + point[i], i + x0 + 1, y0 + point[i+1]);
    }

    //printf ("flyback = %d, before = %d, on = %d, after = %d", flyback, xv[flyback-1], xv[flyback], xv[flyback+1]);

    End();

    //wait_touch(); Uncomment for single sweep  
 
   vgClear(x0, y0, x0 + 501, y0 + 401);
  }

  pthread_join(thbutton, NULL);

  // Clear the screen and display the Logo
  DisplayLogo();
}
