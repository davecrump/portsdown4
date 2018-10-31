// siggentouch.c
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

  shapedemo: testbed for OpenVG APIs
  by Anthony Starks (ajstarks@gmail.com)

  Some code by Evariste F5OEO
  Rewitten by Dave, G8GKQ

  Updated 27 Feb 18 to include code for driving an Elcom Microwave signal source
*/
#include <linux/input.h>
#include <string.h>

#include "touch.h"

#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"

#include <pthread.h>
#include <fftw3.h>
#include <math.h>
#include <wiringPi.h>

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_CONFIG "/home/pi/rpidatv/src/siggen/siggenconfig.txt"
#define PATH_CONFIG_PORTSDOWN "/home/pi/rpidatv/scripts/rpidatvconfig.txt" // no longer used
#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"
#define PATH_CAL "/home/pi/rpidatv/src/siggen/siggencal.txt"
#define PATH_TOUCHCAL "/home/pi/rpidatv/scripts/touchcal.txt"
#define PATH_ATTEN "/home/pi/rpidatv/bin/set_attenuator "

int fd=0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;
int swbuttonsize;
int shbuttonsize;

typedef struct {
	int r,g,b;
} color_t;

typedef struct {
	char Text[255];
	color_t  Color;
} status_t;

#define MAX_STATUS 10
typedef struct {
	int x,y,w,h;
	status_t Status[MAX_STATUS];
	int IndexStatus;
	int NoStatus;
	int LastEventTime;
} button_t;

#define MAX_BUTTON 100
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
int IsDisplayOn=0;
#define TIME_ANTI_BOUNCE 500
int CurrentMenu=1;
int Menu1Buttons=0;
int Menu2Buttons=0;
int Inversed=0;            //Display is inversed (Waveshare=1)
int FinishedButton=1;     // Used to signal button has been pushed

//GLOBAL PARAMETERS

int  scaledX, scaledY;
VGfloat CalShiftX = 0;
VGfloat CalShiftY = 0;
float CalFactorX = 1.0;
float CalFactorY = 1.0;
int64_t DisplayFreq = 437000000;  // Input freq and display freq are the same
int64_t OutputFreq = 0;           // Calculated output frequency
int DisplayLevel = 987;           // calculated for display from (LO) level, atten and freq
char osctxt[255]="portsdown";     // current source
int level;                        // current LO level.  Raw data 0-3 for ADF, 0 - 50 for Exp
float atten = 31.5;               // current atten level.  Raw data (0 - 31.75) 0.25 dB steps


char ref_freq_4351[255] = "25000000";        // read on startup from rpidatvconfig.txt
uint64_t SourceUpperFreq = 13600000000;      // set every time an oscillator is selected
int SourceLowerFreq = 54000000;              // set every time an oscillator is selected

char ref_freq_5355[255] = "25000000";        // read from siggenconfig.txt on startup

int OutputStatus = 0;                        // 0 = off, 1 = on
int PresetStoreTrigger = 0;                  // 0 = normal, 1 = next press should be preset
int ModOn = 0;                               // 0 =  modulation off, 1 = modulation on
int AttenIn = 0;                             // 0 = No attenuator, 1 = attenuator in circuit
char AttenType[256] = "PE43703";             // or PE4302 (0.5 dB steps) or HMC1119 (0.25dB) or NONE

// Titles for presets in file
// [0] is the start-up condition. [1] - [4 ] are the presets
char FreqTag[5][255]={"freq","p1freq","p2freq","p3freq","p4freq"};
char OscTag[5][255]={"osc","p1osc","p2osc","p3osc","p4osc"};
char LevelTag[5][255]={"level","p1level","p2level","p3level","p4level"};
char AttenTag[5][255]={"atten","p1atten","p2atten","p3atten","p4atten"};

// Values for presets [0] "Save" value used on startup. [1] - [4] are presets
// May be over-written by values from from siggenconfig.txt:

uint64_t TabFreq[5]={144750000,146500000,437000000,1249000000,1255000000};
char TabOscOption[5][255]={"portsdown","portsdown","portsdown","portsdown","portsdown"};
char TabLevel[5][255]={"0","0","0","0","0"};
char TabAtten[5][255]={"out","out","out","out","out"};

// Button functions (in button order):
char TabOsc[10][255]={"audio","pirf","adf4351","portsdown","express","a", "a","a", "a","adf5355"};

uint64_t CalFreq[50];
int CalLevel[50];
int CalPoints;

int8_t BandLSBGPIO = 31;
int8_t BandMSBGPIO = 24;
int8_t FiltLSBGPIO = 27;
int8_t FiltNSBGPIO = 25;
int8_t FiltMSBGPIO = 28;
int8_t I_GPIO = 26;
int8_t Q_GPIO = 23;

pthread_t thfft,thbutton,thview;

// Function Prototypes

void Start_Highlights_Menu1();
void MsgBox2(const char *, const char *);
void MsgBox4(const char *, const char *, const char *, const char *);
void wait_touch();
void adf4351On(int);
void ResetBandGPIOs();
void ElcomOn();

/***************************************************************************//**
 * @brief Looks up the value of Param in PathConfigFile and sets value
 *        Used to look up the configuration from *****config.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void GetConfigParam(char *PathConfigFile,char *Param, char *Value)
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
			//strncpy(Value,line+strlen(Param)+1,strlen(line)-strlen(Param)-1-1/* pour retour chariot*/);
	    	}
	}
	else
		printf("Config file not found \n");
	fclose(fp);
}

/***************************************************************************//**
 * @brief sets the value of Param in PathConfigFile froma program variable
 *        Used to store the configuration in rpidatvconfig.txt
 *
 * @param PatchConfigFile (str) the name of the configuration text file
 * @param Param the string labeling the parameter
 * @param Value the looked-up value of the parameter
 *
 * @return void
*******************************************************************************/

void SetConfigParam(char *PathConfigFile,char *Param,char *Value)
{
  char * line = NULL;
  size_t len = 0;
  int read;
  char Command[255];
  char BackupConfigName[255];
  strcpy(BackupConfigName,PathConfigFile);
  strcat(BackupConfigName,".bak");
  FILE *fp=fopen(PathConfigFile,"r");
  FILE *fw=fopen(BackupConfigName,"w+");
  if(fp!=0)
  {
    while ((read = getline(&line, &len, fp)) != -1)
    {
      if(strncmp (line,Param,strlen(Param)) == 0)
      {
        fprintf(fw,"%s=%s\n",Param,Value);
      }
      else
        fprintf(fw,line);
    }
    fclose(fp);
    fclose(fw);
    sprintf(Command,"cp %s %s",BackupConfigName,PathConfigFile);
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
 * @brief Looks up the card number for the RPi Audio Card
 *
 * @param card (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetPiAudioCard(char card[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("aplay -l | grep bcm2835 | head -1 | cut -c6-6", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(card, 7, fp) != NULL)
  {
    sprintf(card, "%d", atoi(card));
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks whether the DATV Express is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckExpressConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q 'CY7C68013' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}

/***************************************************************************//**
 * @brief Checks whether the DATV Express Server is Running
 *
 * @param 
 *
 * @return 0 if running, 1 if not running
*******************************************************************************/

int CheckExpressRunning()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("pgrep -x 'express_server' ; echo $?", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 7, fp) != NULL)
  {
    responseint = atoi(response);
  }

  /* close */
  pclose(fp);
  return responseint;
}


/***************************************************************************//**
 * @brief Called to start the DATV Express Server
 *
 * @param 
 *
 * @return 0 if running OK, 1 if not running
*******************************************************************************/

int StartExpressServer()
{
  char BashText[255];
  int responseint;
  // Check if DATV Express is connected
  if (CheckExpressConnect() == 1)   // Not connected
  {
    MsgBox2("DATV Express Not connected", "Connect it or select another mode");
    wait_touch();
  }

  // Check if the server is already running
  if (CheckExpressRunning() == 1)   // Not running, so start it
  {
    // Make sure the control file is not locked by deleting it
    strcpy(BashText, "sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
    system(BashText);

    // Start the server
    strcpy(BashText, "cd /home/pi/express_server; ");
    //system(BashText);
    strcat(BashText, "sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &");
    system(BashText);
    strcpy(BashText, "cd /home/pi");
    system(BashText);
    MsgBox4("Please wait 5 seconds", "while the DATV Express firmware is loaded", "", "");
    usleep(5000000);
    responseint = CheckExpressRunning();
    if (responseint == 0)  // Running OK
    {
      MsgBox4("", "", "", "DATV Express Firmware Loaded");
      usleep(1000000);
    }
    else
    {
      MsgBox4("Failed to Load", "DATV Express Firmware", "Please check connections", "and try again");
      wait_touch();
    }
  }
  else
  {
    responseint = 0;
  }
  return responseint;
}


/***************************************************************************//**
 * @brief Reads the Presets from siggenconfig.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void

uint64_t TabFreq[4]={146500000,437000000,1249000000,1255000000};
char TabOscOption[4][255]={"portsdown","portsdown","portsdown","portsdown"};
char TabLevel[4][255]={"0","0","0","0"};
char TabAtten[4][255]={"out","out","out","out"};

*******************************************************************************/

void ReadPresets()
{
  // Called at application start
  int n;
  char Param[255];
  char Value[255];

  // Read Freqs, Oscs, Levels and Attens

  n = 0; // Read Start-up  value

  strcpy(Param, FreqTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  DisplayFreq = strtoull(Value, 0, 0);

  // Check that a valid number has been read.  If not, reload factory settings

  if (strlen(Value) <=2)  // invalid start-up frequency
  {                       // So copy in factory defaults
    system("sudo cp -f -r /home/pi/rpidatv/src/siggen/factoryconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt"); 

    GetConfigParam(PATH_CONFIG,Param,Value); // read in the frequency again
    DisplayFreq = strtoull(Value, 0, 0);
  }

  strcpy(Param, OscTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy(osctxt, Value);

  strcpy(Param, LevelTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  level=atoi(Value);

  strcpy(Param, AttenTag[n]);
  GetConfigParam(PATH_CONFIG,Param,Value);
  atten=atof(Value);

  for( n = 1; n < 5; n = n + 1)  // Read Presets
  {
    strcpy(Param, FreqTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    TabFreq[n] = strtoull(Value, 0, 0);

    strcpy(Param, OscTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabOscOption[n], Value);

    strcpy(Param, LevelTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabLevel[n], Value);

    strcpy(Param, AttenTag[n]);
    GetConfigParam(PATH_CONFIG,Param,Value);
    strcpy(TabAtten[n], Value);
  }

  // Read ADF5535 Ref Frequency
  strcpy(Param, "adf5355ref");
  GetConfigParam(PATH_CONFIG,Param,Value);
  strcpy (ref_freq_5355, Value);
}

void ShowFreq(uint64_t DisplayFreq)
{
  // Displays the current frequency with leading zeros blanked
  Fontinfo font = SansTypeface;
  int pointsize = 50;
  float vpos = 0.48;
  uint64_t RemFreq;
  uint64_t df01, df02, df03, df04, df05, df06, df07, df08, df09, df10, df11;
  char df01text[16], df02text[16], df03text[16], df04text[16] ,df05text[16];
  char df06text[16], df07text[16], df08text[16], df09text[16] ,df10text[16];
  char df11text[16];

  if (CurrentMenu == 1)
  {
    vpos = 0.57;
  }
  else
  {
    vpos = 0.48;
  }

  df01 = DisplayFreq/10000000000;
  snprintf(df01text, 10, "%lld", df01);
  RemFreq = DisplayFreq - df01 * 10000000000;

  df02 = RemFreq/1000000000;
  snprintf(df02text, 10, "%lld", df02);
  RemFreq = RemFreq - df02 * 1000000000;

  df03 = RemFreq/100000000;
  snprintf(df03text, 10, "%lld", df03);
  RemFreq = RemFreq - (df03 * 100000000);

  df04 = RemFreq/10000000;
  snprintf(df04text, 10, "%lld", df04);
  RemFreq = RemFreq - df04 * 10000000;

  df05 = RemFreq/1000000;
  snprintf(df05text, 10, "%lld", df05);
  RemFreq = RemFreq - df05 * 1000000;

  df06 = RemFreq/100000;
  snprintf(df06text, 10, "%lld", df06);
  RemFreq = RemFreq - df06 * 100000;

  df07 = RemFreq/10000;
  snprintf(df07text, 10, "%lld", df07);
  RemFreq = RemFreq - df07 * 10000;

  df08 = RemFreq/1000;
  snprintf(df08text, 10, "%lld", df08);
  RemFreq = RemFreq - df08 * 1000;

  df09 = RemFreq/100;
  snprintf(df09text, 10, "%lld", df09);
  RemFreq = RemFreq - df09 * 100;

  df10 = RemFreq/10;
  snprintf(df10text, 10, "%lld", df10);
  RemFreq = RemFreq - df10 * 10;

  df11 = RemFreq;
  snprintf(df11text, 10, "%lld", df11);

  // Display Text
  if (DisplayFreq >= 10000000000)
  {
    Text(0.06*wscreen, vpos*hscreen, df01text, font, pointsize);
  }
  if (DisplayFreq >= 1000000000)
  {
    Text(0.14*wscreen, vpos*hscreen, df02text, font, pointsize);
    Text(0.20*wscreen, vpos*hscreen, ",", font, pointsize);
  }
  if (DisplayFreq >= 100000000)
  {
    Text(0.24*wscreen, vpos*hscreen, df03text, font, pointsize);
  }
  if (DisplayFreq >= 10000000)
  {
    Text(0.32*wscreen, vpos*hscreen, df04text, font, pointsize);
  }
  if (DisplayFreq >= 1000000)
  {
    Text(0.40*wscreen, vpos*hscreen, df05text, font, pointsize);
    Text(0.458*wscreen, vpos*hscreen, ".", font, pointsize);
  }
  if (DisplayFreq >= 100000)
  {
    Text(0.495*wscreen, vpos*hscreen, df06text, font, pointsize);
  }
  if (DisplayFreq >= 10000)
  {
    Text(0.575*wscreen, vpos*hscreen, df07text, font, pointsize);
  }
  if (DisplayFreq >= 1000)
  {
    Text(0.655*wscreen, vpos*hscreen, df08text, font, pointsize);
    Text(0.715*wscreen, vpos*hscreen, ",", font, pointsize);
  }
  if (DisplayFreq >= 100)
  {
    Text(0.75*wscreen, vpos*hscreen, df09text, font, pointsize);
  }
  Text(0.83*wscreen, vpos*hscreen, df10text, font, pointsize);
  Text(0.91*wscreen, vpos*hscreen, df11text, font, pointsize);
}

void ShowLevel(int DisplayLevel)
{
  // DisplayLevel is a signed integer in the range +999 to - 999 tenths of dBm
  Fontinfo font = SansTypeface;
  int pointsize = 50;
  float vpos;
  //int DisplayLevel;
  int dl02, dl03, dl04, RemLevel;
  char dl01text[16], dl02text[16], dl03text[16], dl04text[16];

  if (DisplayLevel == 0)
  {
    strcpy(dl01text, " ");
    strcpy(dl02text, "0");
    strcpy(dl03text, "0");
    strcpy(dl04text, "0");
  }
  else
  {  
    strcpy(dl01text, "+");
    RemLevel = DisplayLevel;
    if (DisplayLevel <= 1)
    {
      strcpy(dl01text, "-");
      RemLevel = -1 * DisplayLevel;
    }
    dl02 = RemLevel/100;
    snprintf(dl02text, 10, "%d", dl02);
    RemLevel = RemLevel - dl02 * 100;

    dl03 = RemLevel/10;
    snprintf(dl03text, 10, "%d", dl03);
    RemLevel = RemLevel - dl03 * 10;

    dl04 = RemLevel;
    snprintf(dl04text, 10, "%d", dl04);
  }
  if (CurrentMenu == 1)
  {
    vpos = 0.43;
    pointsize = 35;
    Text(0.03*wscreen, vpos*hscreen, dl01text, font, pointsize);
    if ((DisplayLevel <= -100) || (DisplayLevel >= 100))
    {
      Text(0.08*wscreen, vpos*hscreen, dl02text, font, pointsize);
    }
    Text(0.13*wscreen, vpos*hscreen, dl03text, font, pointsize);
    Text(0.18*wscreen, vpos*hscreen, ".", font, pointsize);
    Text(0.20*wscreen, vpos*hscreen, dl04text, font, pointsize);
    Text(0.25*wscreen, vpos*hscreen, "dBm", font, pointsize);
  }
  else
  {
    vpos = 0.14;
    pointsize = 50;
    Text(0.02*wscreen, vpos*hscreen, dl01text, font, pointsize);
    if ((DisplayLevel <= -100) || (DisplayLevel >= 100))
    {
      Text(0.11*wscreen, vpos*hscreen, dl02text, font, pointsize);
    }
    Text(0.19*wscreen, vpos*hscreen, dl03text, font, pointsize);
    Text(0.25*wscreen, vpos*hscreen, ".", font, pointsize);
    Text(0.29*wscreen, vpos*hscreen, dl04text, font, pointsize);
    Text(0.37*wscreen, vpos*hscreen, "dBm", font, pointsize);
  }
}

void ShowAtten()
{
  // Display the attenuator setting or DATV Express Level on Menu2
  char LevelText[255];
  char AttenSet[255];
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  float vpos = 0.04;
  float hpos = 0.39;
  
  if ((strcmp(osctxt, "audio") != 0) && (CurrentMenu == 2))
  // Not required for audio or menu 1
  {
    if ((strcmp(osctxt, "express") != 0) && (AttenIn == 1))  // do Attenuator text first
    {
      strcpy(LevelText, "Attenuator Set to -");
      snprintf(AttenSet, 7, " %.2f", atten);
      strcat(LevelText, AttenSet);
      strcat(LevelText, " dB");
      Text(hpos*wscreen, vpos*hscreen, LevelText, font, pointsize);
    }
    if (strcmp(osctxt, "express") == 0)                // DATV Express Text
    {
      strcpy(LevelText, "Express Level = ");
      snprintf(AttenSet, 3, "%d", level);
      strcat(LevelText, AttenSet);
      Text(hpos*wscreen, vpos*hscreen, LevelText, font, pointsize);
    }
  }
}

void ShowOPFreq()
{
  // Display the calculated output freq
  char OPFreqText[255];
  char FreqString[255];
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  float vpos = 0.31;
  float hpos = 0.39;
  //OutputFreq = 24048100000;
  
  if ((strcmp(osctxt, "adf5355") == 0) && (CurrentMenu == 2))
  // Not required for audio or menu 1
  {
      strcpy(OPFreqText, "Output Freq = ");
      snprintf(FreqString, 12, "%lld", OutputFreq);
      strcat(OPFreqText, FreqString);
      //strcat(LevelText, " dB");
      Text(hpos*wscreen, vpos*hscreen, OPFreqText, font, pointsize);
  }
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
  tw = TextWidth("BATC Portsdown Signal Generator", font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, "BATC Portsdown Signal Generator", font, pointsize);
}

void AdjustFreq(int button)
{
  char ExpressCommand[255];
  char FreqText[255];
  button=button-Menu1Buttons-8;
  switch (button)
  {
    case 0:
      if (DisplayFreq >= 10000000000)
      {
        DisplayFreq=DisplayFreq-10000000000;
      }
      break;
    case 1:
      if (DisplayFreq >= 1000000000)
      {
        DisplayFreq=DisplayFreq-1000000000;
      }
      break;
    case 2:
      if (DisplayFreq >= 100000000)
      {
        DisplayFreq=DisplayFreq-100000000;
      }
      break;
    case 3:
      if (DisplayFreq >= 10000000)
      {
        DisplayFreq=DisplayFreq-10000000;
      }
      break;
    case 4:
      if (DisplayFreq >= 1000000)
      {
        DisplayFreq=DisplayFreq-1000000;
      }
      break;
    case 5:
      if (DisplayFreq >= 100000)
      {
        DisplayFreq=DisplayFreq-100000;
      }
      break;
    case 6:
      if (DisplayFreq >= 10000)
      {
        DisplayFreq=DisplayFreq-10000;
      }
      break;
    case 7:
      if (DisplayFreq >= 1000)
      {
        DisplayFreq=DisplayFreq-1000;
      }
      break;
    case 8:
      if (DisplayFreq >= 100)
      {
        DisplayFreq=DisplayFreq-100;
      }
      break;
    case 9:
      if (DisplayFreq >= 10)
      {
        DisplayFreq=DisplayFreq-10;
      }
      break;
    case 10:
      if (DisplayFreq >= 1)
      {
        DisplayFreq=DisplayFreq-1;
      }
      break;
    case 11:
      if (DisplayFreq <= 9999999999)
      {
        DisplayFreq=DisplayFreq+10000000000;
      }
      break;
    case 12:
      if (DisplayFreq <= 18999999999)
      {
        DisplayFreq=DisplayFreq+1000000000;
      }
      break;
    case 13:
      if (DisplayFreq <= 19899999999)
      {
        DisplayFreq=DisplayFreq+100000000;
      }
      break;
    case 14:
      if (DisplayFreq <= 19989999999)
      {
        DisplayFreq=DisplayFreq+10000000;
      }
      break;
    case 15:
      if (DisplayFreq <= 19998999999)
      {
        DisplayFreq=DisplayFreq+1000000;
      }
      break;
    case 16:
      if (DisplayFreq <= 19999899999)
      {
        DisplayFreq=DisplayFreq+100000;
      }
      break;
    case 17:
      if (DisplayFreq <= 19999989999)
      {
        DisplayFreq=DisplayFreq+10000;
      }
      break;
    case 18:
      if (DisplayFreq <= 19999998999)
      {
        DisplayFreq=DisplayFreq+1000;
      }
      break;
    case 19:
      if (DisplayFreq <= 19999999899)
      {
        DisplayFreq=DisplayFreq+100;
      }
      break;
    case 20:
      if (DisplayFreq <= 19999999989)
      {
        DisplayFreq=DisplayFreq+10;
      }
      break;
    case 21:
      if (DisplayFreq <= 19999999998)
      {
        DisplayFreq=DisplayFreq+1;
      }
      break;
    default:
      
  break;
  }
  if (DisplayFreq > SourceUpperFreq)
  {
    DisplayFreq = SourceUpperFreq;
  }
  if (DisplayFreq < SourceLowerFreq)
  {
    DisplayFreq = SourceLowerFreq;
  }

  // Set the freq here if output active

  if (OutputStatus == 1)
  {
    if (strcmp(osctxt, "express")==0)
    {
      strcpy(ExpressCommand, "echo \"set freq ");
      snprintf(FreqText, 12, "%lld", DisplayFreq);
      strcat(ExpressCommand, FreqText);
      strcat(ExpressCommand, "\" >> /tmp/expctrl" );
      system(ExpressCommand);
    }

    if (strcmp(osctxt, "portsdown")==0)
    {
      if (ModOn == 0)
      {
        adf4351On(3); // change adf freq at level 3
      }
      else
      {
        adf4351On(0); // change adf freq at level 0
      }
    }

    if (strcmp(osctxt, "adf4351")==0)
    {
      adf4351On(level); // change adf freq at set level
    }

    if (strcmp(osctxt, "adf5355")==0)
    {
      ElcomOn(); // Change Elcom Freq
    }

    // Change the band if required
    ResetBandGPIOs();
  }
}

void CalcOPLevel()
{
  int PointBelow = 0;
  int PointAbove = 0;
  int n = 0;
  float proportion;
  float MinAtten;

  // Calculate output level from Osc based on Cal and frequency *********************

  while ((PointAbove == 0) && (n <= 100))
  {
    n = n+1;
    if (DisplayFreq <= CalFreq[n])
    {
      PointAbove = n;
      PointBelow = n - 1;
    }
  }
  // printf("PointAbove = %d \n", PointAbove);

  if (DisplayFreq == CalFreq[n])
  {
    DisplayLevel = CalLevel[PointAbove];
  }
  else
  {
    proportion = (float)(DisplayFreq - CalFreq[PointBelow])/(CalFreq[PointAbove]- CalFreq[PointBelow]);
    //printf("proportion = %f \n", proportion);
    DisplayLevel = CalLevel[PointBelow] + (CalLevel[PointAbove] - CalLevel[PointBelow]) * proportion;
  }

  // printf("Initial Display Level = %d\n", DisplayLevel);

  // Now correct for set oscillator level ******************************************

  if (strcmp(osctxt, "audio")==0)
  {
    DisplayLevel = 0;
  }

  if (strcmp(osctxt, "pirf")==0)
  {
    DisplayLevel = -70;
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    DisplayLevel = DisplayLevel + 30 * level;
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    ;
    // no correction required
  }

  if (strcmp(osctxt, "express")==0)
  {
    DisplayLevel = DisplayLevel + 10*level;  // assumes 1 dB steps  Could use look-up table for more accuracy
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    DisplayLevel=0;
  }

  // Now apply attenuation *********************************************************************
  if (AttenIn == 1)
  {
    if (strcmp(AttenType, "PE4312")==0)
    {
      MinAtten = 2.5;
      DisplayLevel=round((float)DisplayLevel-10*atten-10*MinAtten);
    }
    if (strcmp(AttenType, "PE43713")==0)
    {
      MinAtten = 2.5;   // Spec says 1.8dB, but measures as 2.5dB
      DisplayLevel=round((float)DisplayLevel-10*atten-10*MinAtten);
    }
    if (strcmp(AttenType, "HMC1119")==0)
    {
      MinAtten = 2.5;
      DisplayLevel=round((float)DisplayLevel-10*atten-10*MinAtten);
    }
  }
  // Now adjust if modulation is on  *********************************************************************
  if ((ModOn == 1) && (strcmp(osctxt, "express")==0))
  {
    DisplayLevel = DisplayLevel - 2;   // Correction for DATV Express mod power (-0.2 dB)
  }
  if ((ModOn == 1) && (strcmp(osctxt, "portsdown")==0))
  {
    DisplayLevel = DisplayLevel - 16;   // Correction for Portsdown mod power (-1.6 dB)
  }
}

void SetAtten(float AttenValue)
{
  // This sets the attenuator to AttenValue
  char AttenCmd[255];
  char AttenSet[255];
  snprintf(AttenSet, 7, " %.2f", AttenValue);
  strcpy(AttenCmd, PATH_ATTEN);
  strcat(AttenCmd, AttenType);
  strcat(AttenCmd, AttenSet);
  printf("%s\n", AttenCmd);
  system(AttenCmd);  
}

void AdjustLevel(int Button)
{
  char ExpressCommand[255];
  char LevelText[255];

  // Deal with audio levels **********************************************
  if (strcmp(osctxt, "audio")==0)
  {
    ;  // Audio behaviour tbd
  }

  // Deal with DATV Express Levels
  else if (strcmp(osctxt, "express")==0)
  {
    if (Button == (Menu1Buttons + 0))  // decrement level by 10
    {
      level = level - 10;
    }
    if (Button == (Menu1Buttons + 1))  // decrement level by 1
    {
      level = level - 1;
    }
    if (Button == (Menu1Buttons +5))  // increment level by 10
    {
      level = level + 10;
    }
    if (Button == (Menu1Buttons +6))  // increment level by 1
    {
      level = level + 1;
    }
    if (level < 0 )
    {
      level = 0;
    }
    if (level > 47 )
    {
      level = 47;
    }

    // Now send command to change level
    strcpy(ExpressCommand, "echo \"set level ");
    snprintf(LevelText, 3, "%d", level);
    strcat(ExpressCommand, LevelText);
    strcat(ExpressCommand, "\" >> /tmp/expctrl" );
    system(ExpressCommand);
  }
  else
  {
    if (AttenIn == 0) // No attenuator
    {
      if (strcmp(osctxt, "pirf")==0)
      {
        ;  // pirf behaviour tbd
      }
      if (strcmp(osctxt, "adf4351")==0)
      {
        if (Button == (Menu1Buttons +1))  // decrement level
        {
          level = level - 1;
          if (level < 0 )
          {
            level = 0;
          }
        }
        if (Button == (Menu1Buttons +6))  // increment level
        {
          level = level + 1;
          if (level > 3 )
          {
            level = 3;
          }
        }
      }
      if (strcmp(osctxt, "adf5355")==0)
      {
        ;  // adf5355 behaviour tbd
      }
    }
    else                         // With attenuator
    {
      if ((strcmp(osctxt, "adf4351")==0) || (strcmp(osctxt, "adf5355")==0))
      // set adf4351 or adf5355 level
      {
        level = 3;
      }
      if ((strcmp(osctxt, "portsdown")==0) || (strcmp(osctxt, "adf4351")==0))
      // portsdown or adf4351 attenuator behaviour here
      {
        if (Button == (Menu1Buttons + 0))  // decrement level by 10 dB
        {
          atten = atten + 10.0;
        }
        if (Button == (Menu1Buttons + 1))  // decrement level by 1 dB
        {
          atten = atten + 1.0;
        }
        if (Button == (Menu1Buttons + 2))  // decrement level by .25 or .5 dB
        {
          if (strcmp(AttenType, "PE4312") == 0) // 0.5 dB steps
          {
            atten = atten + 0.5;
          }
          else                                // 0.25 dB steps
          {
            atten = atten + 0.25;
          }
        }
        if (Button == (Menu1Buttons + 5))  // increment level by 10 dB
        {
          atten = atten - 10.0;
        }
        if (Button == (Menu1Buttons + 6))  // increment level by 1 dB
        {
          atten = atten - 1.0;
        }
        if (Button == (Menu1Buttons + 7))  // increment level by .25 or .5 dB
        {
          if (strcmp(AttenType, "PE4312") == 0) // 0.5 dB steps
          {
            atten = atten - 0.5;
          }
          else                                // 0.25 dB steps
          {
            atten = atten - 0.25;
          }
        }
        // Now check bounds
        if (atten <= 0)                      // Attenuation cannot be less than 0
        {
          atten = 0;
        }
        if ((strcmp(AttenType, "PE4312") == 0) && (atten >= 31.5)) // max 31.5 dB
        {
          atten = 31.5;
        }
        if (atten >= 31.75) // max 31.75 dB for other attenuators
        {
          atten = 31.75;
        }
        SetAtten(atten);
      }
    }
  }
}

void SetBandGPIOs()
{
  // Initialise Band GPIOs high (for 1255) and filter and IQ low 
  // for max output and repeatability

  pinMode(BandLSBGPIO, OUTPUT);
  pinMode(BandMSBGPIO, OUTPUT);
  pinMode(FiltLSBGPIO, OUTPUT);
  pinMode(FiltNSBGPIO, OUTPUT);
  pinMode(FiltMSBGPIO, OUTPUT);
  pinMode(I_GPIO, OUTPUT);
  pinMode(Q_GPIO, OUTPUT);

  digitalWrite(BandLSBGPIO, HIGH);
  digitalWrite(BandMSBGPIO, HIGH);
  digitalWrite(FiltLSBGPIO, LOW);
  digitalWrite(FiltNSBGPIO, HIGH);
  digitalWrite(FiltMSBGPIO, LOW); // 333KS
  digitalWrite(I_GPIO, LOW);
  digitalWrite(Q_GPIO, LOW);
}

void ResetBandGPIOs()
{
  // Reset Band GPIOs to correct band ONLY if mod is on and Portsdown.
  // Else set 1255.
  if ((ModOn == 0) || (strcmp(osctxt, "portsdown")!=0))
  {
    digitalWrite(BandLSBGPIO, HIGH);
    digitalWrite(BandMSBGPIO, HIGH);
  }
  else                 // Portsdown with Modulation
  {
    if (DisplayFreq < 100000000)
    {
      digitalWrite(BandLSBGPIO, LOW);
      digitalWrite(BandMSBGPIO, LOW);
    }
    else if (DisplayFreq < 250000000)
    {
      digitalWrite(BandLSBGPIO, HIGH);
      digitalWrite(BandMSBGPIO, LOW);
    }
    else if (DisplayFreq < 950000000)
    {
      digitalWrite(BandLSBGPIO, LOW);
      digitalWrite(BandMSBGPIO, HIGH);
    }
    else
    {
      digitalWrite(BandLSBGPIO, HIGH);
      digitalWrite(BandMSBGPIO, HIGH);
    }
  }
}

void adf4351On(int adflevel)
{
  char transfer[255];
  char StartPortsdown[255] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  double freqmhz;
  freqmhz=(double)DisplayFreq/1000000;
  snprintf(transfer, 13, "%.6f", freqmhz);
  strcat(StartPortsdown, transfer);
  strcat(StartPortsdown, " ");
  strcat(StartPortsdown, ref_freq_4351);
  snprintf(transfer, 3, " %d", adflevel);
  strcat(StartPortsdown, transfer);                    // Level from input parameter
  printf("Starting portsdown output %s\n", StartPortsdown);
  system(StartPortsdown);
}


void PortsdownOn()
{
  char transfer[255];
  char StartPortsdown[255] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  double freqmhz;

  // Change the band setting if required
  ResetBandGPIOs();

  freqmhz=(double)DisplayFreq/1000000;
  snprintf(transfer, 13, "%.6f", freqmhz);
  strcat(StartPortsdown, transfer);
  strcat(StartPortsdown, " ");
  strcat(StartPortsdown, ref_freq_4351);
  strcat(StartPortsdown, " 3"); // Level 3 for SigGen
  printf("Starting portsdown output\n");
  system(StartPortsdown);
}

void PortsdownOnWithMod()
{
  char transfer[255];
  char StartPortsdown[255]; 
  double freqmhz;

  strcpy(StartPortsdown, "sudo rm videots");
  system(StartPortsdown); 
  strcpy(StartPortsdown, "mkfifo videots");
  system(StartPortsdown);

  // Change the band setting if required
  ResetBandGPIOs();

  strcpy(StartPortsdown, "sudo /home/pi/rpidatv/bin/adf4351 ");
  freqmhz=(double)DisplayFreq/1000000;
  snprintf(transfer, 13, "%.6f", freqmhz);
  strcat(StartPortsdown, transfer);
  strcat(StartPortsdown, " ");
  strcat(StartPortsdown, ref_freq_4351);
  strcat(StartPortsdown, " 0"); // Level 0 for Transmit
  printf("Starting portsdown output with Mod\n");
  system(StartPortsdown);

  strcpy(StartPortsdown, "sudo nice -n -30 /home/pi/rpidatv/bin/rpidatv ");
  strcat(StartPortsdown, "-i videots -s 333 -c 7/8 -f 0 -p 7 -m IQ -x 12 -y 13 &");
  system(StartPortsdown);

  strcpy(StartPortsdown, "/home/pi/rpidatv/bin/avc2ts.old -b 372783 -m 537044 ");
  strcat(StartPortsdown, "-x 720 -y 576 -f 25 -i 100 -o videots -t 3 -p 255 -s SigGen &");
  system(StartPortsdown); 

}


void ElcomOn()
{
  int D[8]= {0, 0, 0, 0, 0, 0, 0, 0};
  int CalcFreq;

  // Nominate pins using WiringPi numbers

  // CLK  pin 29 wPi 21
  // Data pin 31 wPi 22
  // Elcom LE pin 8 wPi 15

	uint8_t LE_Elcom_GPIO = 15;
	uint8_t CLK_GPIO = 21;
	uint8_t DATA_GPIO = 22;

	// Set all nominated pins to outputs

	pinMode(LE_Elcom_GPIO, OUTPUT);
	pinMode(CLK_GPIO, OUTPUT);
	pinMode(DATA_GPIO, OUTPUT);

	// Set idle conditions

	digitalWrite(LE_Elcom_GPIO, HIGH);
	digitalWrite(CLK_GPIO, LOW);
	digitalWrite(DATA_GPIO, LOW);

  // Divide freq (uint64t) by 1,000,000 to get integer MHz
  CalcFreq = DisplayFreq/1000000;

  printf("Display Freq = %lld\n", DisplayFreq);
  printf("CalcFreq = %d MHz\n", CalcFreq);

  // Mult by 3 to get freq for selection
  CalcFreq = CalcFreq * 3;  // 3 * freq in MHz
  //printf("CalcFreq * 3 = %d\n", CalcFreq);

  // Divide by 10 to get integer 10s of setting
  CalcFreq = CalcFreq / 10;
  //printf("CalcFreq * 3 / 10 = %d\n", CalcFreq);

  // Calculate OutputFreq for display
  OutputFreq = CalcFreq * 10000000LL;
  //printf("Output Freq * 3 = %lld\n", OutputFreq);
  OutputFreq = OutputFreq / 3;
  //printf("Output Freq = %lld\n", OutputFreq);


  // break out the 4 active digits of D[7] to D[4].  D[3] thru D[0] = 0
  D[7] = CalcFreq / 1000;
  CalcFreq = CalcFreq - D[7] * 1000;
  D[6] = CalcFreq / 100;
  CalcFreq = CalcFreq - D[6] * 100;
  D[5] = CalcFreq / 10;
  CalcFreq = CalcFreq - D[5] * 10;
  D[4] = CalcFreq;
  D[3] = 0;
  D[2] = 0;
  D[1] = 0;
  D[0] = 0;

  printf("Sending %d%d%d%d%d%d%d%d to Elcom\n", D[7], D[6], D[5], D[4], D[3], D[2], D[1], D[0]);

  // Delay, select device LE low and delay again
  usleep(1000);
  digitalWrite(LE_Elcom_GPIO, LOW);
  usleep(1000);

  // Initialise loop
  uint16_t i;
  uint16_t j;

  // Send 8 4-bit words
  for (i = 0; i <8; i++)
  {
    for (j = 0; j < 4; j++)
    {
      // Test left-most bit
      if (D[i] & 0x00000001)
      {
        digitalWrite(DATA_GPIO, HIGH);
        printf("1");
      }
      else
      {
        digitalWrite(DATA_GPIO, LOW);
        printf("0");
      }
      // Pulse clock
      usleep(1000);
      digitalWrite(CLK_GPIO, HIGH);
      usleep(1000);
      digitalWrite(CLK_GPIO, LOW);
      usleep(1000);

      // shift data right so next bit will be rightmost
      D[i] >>= 1;
    }
    printf(" ");
  }
  printf("\n");
	
  //Set LE high and delay before exit
  digitalWrite(LE_Elcom_GPIO, HIGH);
  usleep(10000);


}


void ExpressOn()
{
  char ExpressCommand[255];
  char LevelText[255];
  char FreqText[255];

  strcpy(ExpressCommand, "echo \"set freq ");
  snprintf(FreqText, 12, "%lld", DisplayFreq);
  strcat(ExpressCommand, FreqText);
  strcat(ExpressCommand, "\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set fec 7/8\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set srate 333000\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set port 0\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set level ");
  snprintf(LevelText, 3, "%d", level);
  strcat(ExpressCommand, LevelText);
  strcat(ExpressCommand, "\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set car on\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set ptt tx\" >> /tmp/expctrl" );
  system(ExpressCommand);
}

void ExpressOnWithMod()
{
  char ExpressCommand[255];
  char LevelText[255];
  char FreqText[255];

  strcpy(ExpressCommand, "sudo rm videots");
  system(ExpressCommand); 
  strcpy(ExpressCommand, "mkfifo videots");
  system(ExpressCommand); 

  strcpy(ExpressCommand, "echo \"set freq ");
  snprintf(FreqText, 12, "%lld", DisplayFreq);
  strcat(ExpressCommand, FreqText);
  strcat(ExpressCommand, "\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set fec 7/8\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set srate 333000\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set port 0\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set level ");
  snprintf(LevelText, 3, "%d", level);
  strcat(ExpressCommand, LevelText);
  strcat(ExpressCommand, "\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set car off\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "echo \"set ptt tx\" >> /tmp/expctrl" );
  system(ExpressCommand);

  strcpy(ExpressCommand, "sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &");
  system(ExpressCommand);

  strcpy(ExpressCommand, "/home/pi/rpidatv/bin/avc2ts.old -b 372783 -m 537044 ");
  strcat(ExpressCommand, "-x 720 -y 576 -f 25 -i 100 -o videots -t 3 -p 255 -s SigGen &");
  system(ExpressCommand);
}

int mymillis()
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}

void ReadTouchCal()
{
  char Param[255];
  char Value[255];

  // Read CalFactors
  strcpy(Param, "CalFactorX");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalFactorX = strtof(Value, 0);
  printf("Starting with CalfactorX = %f\n", CalFactorX);
  strcpy(Param, "CalFactorY");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalFactorY = strtof(Value, 0);
  printf("Starting with CalfactorY = %f\n", CalFactorY);

  // Read CalShifts
  strcpy(Param, "CalShiftX");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalShiftX = strtof(Value, 0);
  printf("Starting with CalShiftX = %f\n", CalShiftX);
  strcpy(Param, "CalShiftY");
  GetConfigParam(PATH_TOUCHCAL,Param,Value);
  CalShiftY = strtof(Value, 0);
  printf("Starting with CalShiftY = %f\n", CalShiftY);
}

void TransformTouchMap(int x, int y)
{
  // This function takes the raw (0 - 4095 on each axis) touch data x and y
  // and transforms it to approx 0 - wscreen and 0 - hscreen in globals scaledX 
  // and scaledY prior to final correction by CorrectTouchMap  

  int shiftX, shiftY;
  double factorX, factorY;
  char Param[255];
  char Value[255];

  // Adjust registration of touchscreen for Waveshare
  shiftX=30; // move touch sensitive position left (-) or right (+).  Screen is 700 wide
  shiftY=-5; // move touch sensitive positions up (-) or down (+).  Screen is 480 high

  factorX=-0.4;  // expand (+) or contract (-) horizontal button space from RHS. Screen is 5.6875 wide
  factorY=-0.3;  // expand or contract vertical button space.  Screen is 8.53125 high

  // Switch axes for normal and waveshare displays
  if(Inversed==0) // Tontec35 or Element14_7
  {
    scaledX = x/scaleXvalue;
    scaledY = hscreen-y/scaleYvalue;
  }
  else //Waveshare (inversed)
  {
    scaledX = shiftX+wscreen-y/(scaleXvalue+factorX);

    strcpy(Param,"display");  //Check for Waveshare 4 inch
    GetConfigParam(PATH_PCONFIG,Param,Value);
    if(strcmp(Value,"Waveshare4")!=0)
    {
      scaledY = shiftY+hscreen-x/(scaleYvalue+factorY);
    }
    else  // Waveshare 4 inch display so flip vertical axis
    {
      scaledY = shiftY+x/(scaleYvalue+factorY); // Vertical flip for 4 inch screen
    }
  }
}

void CorrectTouchMap()
{
  // This function takes the approx touch data and applies the calibration correction
  // It works directly on the globals scaledX and scaledY based on the constants read
  // from the ScreenCal.txt file during initialisation

  scaledX = scaledX * CalFactorX;
  scaledX = scaledX + CalShiftX;

  scaledY = scaledY * CalFactorY;
  scaledY = scaledY + CalShiftY;
}

int IsButtonPushed(int NbButton,int x,int y)
{
  TransformTouchMap(x,y);  // Sorts out orientation and approx scaling of the touch map

  CorrectTouchMap();       // Calibrates each individual screen

  // printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue);

  int margin=1;  // was 20

  if((scaledX<=(ButtonArray[NbButton].x+ButtonArray[NbButton].w-margin))&&(scaledX>=ButtonArray[NbButton].x+margin) &&
    (scaledY<=(ButtonArray[NbButton].y+ButtonArray[NbButton].h-margin))&&(scaledY>=ButtonArray[NbButton].y+margin))
  {
    ButtonArray[NbButton].LastEventTime=mymillis();
    return 1;
  }
  else
  {
    return 0;
  }
}

int AddButton(int x,int y,int w,int h)
{
	button_t *NewButton=&(ButtonArray[IndexButtonInArray]);
	NewButton->x=x;
	NewButton->y=y;
	NewButton->w=w;
	NewButton->h=h;
	NewButton->NoStatus=0;
	NewButton->IndexStatus=0;
	NewButton->LastEventTime=mymillis();
	return IndexButtonInArray++;
}

int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color)
{
	button_t *Button=&(ButtonArray[ButtonIndex]);
	strcpy(Button->Status[Button->IndexStatus].Text,Text);
	Button->Status[Button->IndexStatus].Color=*Color;
	return Button->IndexStatus++;
}

void DrawButton(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  Fill(Button->Status[Button->NoStatus].Color.r, Button->Status[Button->NoStatus].Color.g, Button->Status[Button->NoStatus].Color.b, 1);
  Roundrect(Button->x,Button->y,Button->w,Button->h, Button->w/10, Button->w/10);
  Fill(255, 255, 255, 1);    // White text
  TextMid(Button->x+Button->w/2, Button->y+Button->h/2, Button->Status[Button->NoStatus].Text, SerifTypeface, Button->w/strlen(Button->Status[Button->NoStatus].Text)/*25*/);	
}

void SetButtonStatus(int ButtonIndex,int Status)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  Button->NoStatus=Status;
}

int GetButtonStatus(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  return	Button->NoStatus;
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

/*
Input device name: "ADS7846 Touchscreen"
Supported events:
  Event type 0 (Sync)
  Event type 1 (Key)
    Event code 330 (Touch)
  Event type 3 (Absolute)
    Event code 0 (X)
     Value      0
     Min        0
     Max     4095
    Event code 1 (Y)
     Value      0
     Min        0
     Max     4095
    Event code 24 (Pressure)
     Value      0
     Min        0
     Max      255
*/

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

void UpdateWindow()
// Paint each button on the current Menu
{
  int i;
  int first;
  int last;

  // Calculate the Button numbers for the Current Menu
  switch (CurrentMenu)
  {
    case 1:
      first=0;
      last=Menu1Buttons;
      break;
    case 2:
      first=Menu1Buttons;
      last=Menu1Buttons+Menu2Buttons;
      break;
    default:
      first=0;
      last=Menu1Buttons;
      break;
  }

  for(i=first;i<last;i++)
    DrawButton(i);
  End();
}

void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status)
{
  int i;
  for(i=StartButton;i<=StopButton;i++)
  {
    if(i==NoButton)
      SetButtonStatus(i,Status);
    else
      SetButtonStatus(i,0);
  }
}

void ImposeBounds()  // Constrain DisplayFreq and level to physical limits
{
  if (strcmp(osctxt, "audio")==0)
  {
    SourceUpperFreq = 8000;
    SourceLowerFreq = 100;
  }
  if (strcmp(osctxt, "pirf")==0)
  {
    SourceUpperFreq = 50000000;
    SourceLowerFreq = 10000000;
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    SourceUpperFreq = 4294967295LL;
    SourceLowerFreq = 35000000;
    if (level > 3)
    {
      level = 3;
    }
    if (level < 0)
    {
      level = 0;
    }
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    SourceUpperFreq = 1500000000;
    SourceLowerFreq = 50000000;
  }

  if (strcmp(osctxt, "express")==0)
  {
    SourceUpperFreq = 2450000000LL;
    SourceLowerFreq = 70000000;
    if (level > 47)
    {
      level = 47;
    }
    if (level < 0)
    {
      level = 0;
    }
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    SourceUpperFreq = 13600000000LL;
    SourceLowerFreq =  1000000000;
//    SourceLowerFreq = 54000000;
  }

  if (DisplayFreq > SourceUpperFreq)
  {
    DisplayFreq = SourceUpperFreq;
  }
  if (DisplayFreq < SourceLowerFreq)
  {
    DisplayFreq = SourceLowerFreq;
  }
}

void InitOsc()   
// Check the freq is in bounds and start/stop DATV express if req
// Read in amplitude Cal table and hide unused buttons
// Call CalcOPLevel
{
  char Param[256];
  char Value[256];
  char KillExpressSvr[255];
  int n;
  char PointNumber[255];
  ImposeBounds();
  if (strcmp(osctxt, "express")==0)
  {
    printf("Starting DATV Express\n");
    //MsgBox4("Please wait","Loading Firmware to DATV Express", "", "");
    //system("/home/pi/rpidatv/src/siggen/startexpresssvr.sh");
    n = StartExpressServer();
    printf("Response from StartExpressServer was %d\n", n);
    ShowTitle();
    ShowLevel(DisplayLevel);
    ShowFreq(DisplayFreq);
    UpdateWindow();
  }
  else
  {
    strcpy(KillExpressSvr, "echo \"set kill\" >> /tmp/expctrl");
    system(KillExpressSvr);
  }
  
  // Turn off attenuator if not compatible with mode
  if ((strcmp(osctxt, "audio") == 0) || (strcmp(osctxt, "pirf") == 0) ||(strcmp(osctxt, "express") == 0))
  {
    AttenIn = 0;
    SetAtten(0);
  }

  // Turn off modulation if not compatible with mode
  if ((strcmp(osctxt, "audio") == 0) || (strcmp(osctxt, "pirf") == 0)
    || (strcmp(osctxt, "adf4351") == 0) || (strcmp(osctxt, "adf5355") == 0))
  {
    ModOn = 0;
  }

  // Set adf4351 level correctly for attenuator
  if ((strcmp(osctxt, "adf4351") == 0) && (AttenIn == 1))
  {
    level = 3;
  }

  // Read in amplitude Cal table
  strcpy(Param, osctxt);
  strcat(Param, "points");
  GetConfigParam(PATH_CAL,Param,Value);
  CalPoints = atoi(Value);
  //printf("CalPoints= %d \n", CalPoints);
  for ( n = 1; n <= CalPoints; n = n + 1 )
  {
    snprintf(PointNumber, 4, "%d", n);

    strcpy(Param, osctxt);
    strcat(Param, "freq");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL,Param,Value);
    CalFreq[n] = strtoull(Value, 0, 0);
    //printf("CalFreq= %lld \n", CalFreq[n]);

    strcpy(Param, osctxt);
    strcat(Param, "lev");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL,Param,Value);
    CalLevel[n] = atoi(Value);
    //printf("CalLevel= %d \n", CalLevel[n]);
  }

  // Hide unused buttons
  // First make them all visible
  for (n = 0; n < 3; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,0);         // Show all level decrement
      SetButtonStatus(Menu1Buttons+5+n, 0);     // Show all level increment
    }
  for (n = 8; n < 30; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,0);         // Show all freq inc/decrement
    }

  // Hide the unused frequency increment/decrement buttons
  if (strcmp(osctxt, "audio")==0)
  {
    for (n = 8; n < 14; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,1);         //hide frequency decrement above 99,999 Hz
      SetButtonStatus(Menu1Buttons+n+11, 1);     //hide frequency increment above 99,999 Hz
    }
  }
  if (strcmp(osctxt, "pirf")==0)
  {
    for (n = 8; n < 10; n = n + 1)
    {
      SetButtonStatus(Menu1Buttons+n,1);         //hide frequency decrement above 999 MHz
      SetButtonStatus(Menu1Buttons+n+11, 1);     //hide frequency increment above 999 MHz
    }
  }
  if (strcmp(osctxt, "adf4351")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }
  if (strcmp(osctxt, "portsdown")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }
  if (strcmp(osctxt, "express")==0)
  {
    SetButtonStatus(Menu1Buttons+8,1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(Menu1Buttons+19,1);        //hide frequency increment above 9.99 GHz
  }

  // Hide the unused level buttons
  if (AttenIn == 0)
  {
    if (strcmp(osctxt, "audio")==0)
    {
      ;  // Audio behaviour tbd
    }
    if (strcmp(osctxt, "pirf")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+1,1);         // Hide decrement 1s
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+6,1);         // Hide increment 1s
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "adf4351")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "portsdown")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+1,1);         // Hide decrement 1s
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+6,1);         // Hide increment 1s
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
    if (strcmp(osctxt, "express")==0)
    {
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10ths
    }
    if (strcmp(osctxt, "adf5355")==0)
    {
      SetButtonStatus(Menu1Buttons+0,1);         // Hide decrement 10ths
      SetButtonStatus(Menu1Buttons+2,1);         // Hide decrement 10s
      SetButtonStatus(Menu1Buttons+5,1);         // Hide increment 10ths
      SetButtonStatus(Menu1Buttons+7,1);         // Hide increment 10s
    }
  }
  CalcOPLevel();
  Start_Highlights_Menu1();  
}

void SelectOsc(int NoButton)  // Select Oscillator Source
{
  SelectInGroup(0,4,NoButton,1);
  SelectInGroup(9,9,NoButton,1);
  strcpy(osctxt,TabOsc[NoButton-0]);
  printf("************** Set osc = %s\n", osctxt);

  InitOsc();

  ShowTitle();
  ShowLevel(DisplayLevel);
  ShowFreq(DisplayFreq);

  UpdateWindow();
}

void SavePreset(int PresetNo)
{
  char Param[256];
  char Value[256];

  TabFreq[PresetNo] = DisplayFreq;
  snprintf(Value, 12, "%lld", DisplayFreq);
  strcpy(Param, FreqTag[PresetNo]);
  SetConfigParam(PATH_CONFIG, Param, Value);

  strcpy(TabOscOption[PresetNo], osctxt);
  strcpy(Param, OscTag[PresetNo]);
  SetConfigParam(PATH_CONFIG, Param, osctxt);

  snprintf(Value, 4, "%d", level);
  strcpy(TabLevel[PresetNo], Value);
  strcpy(Param, LevelTag[PresetNo]);
  SetConfigParam(PATH_CONFIG ,Param, Value);

  snprintf(Value, 6, "%.2f", atten);
  strcpy(TabAtten[PresetNo], Value);
  strcpy(Param, AttenTag[PresetNo]);
  SetConfigParam(PATH_CONFIG ,Param, Value);
}

void RecallPreset(int PresetNo)
{
  DisplayFreq = TabFreq[PresetNo];
  strcpy(osctxt, TabOscOption[PresetNo]);
  level=atoi(TabLevel[PresetNo]);
  atten=atof(TabAtten[PresetNo]);

  InitOsc();
}

void SelectMod(int NoButton,int Status)  // Modulation on or off
{
  SelectInGroup(11,11,NoButton,Status);
  SelectInGroup(Menu1Buttons+5,Menu1Buttons+5,NoButton,Status);
}

void OscStart()
{
  //  Look up which oscillator we are using
  // Then use an if statement for each alternative

  // Set the attenuator if required
  if (AttenIn ==1)
  {
    SetAtten(atten);
  }

  printf("Oscillator Start\n");
  char StartPortsdown[255] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  char transfer[255];
  double freqmhz;
  int adf4351_lev = level; // 0 to 3

  if (strcmp(osctxt, "audio")==0)
  {
    printf("\nStarting Audio output\n");
  }

  if (strcmp(osctxt, "pirf")==0)
  {
    printf("\nStarting RPi RF output\n");
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    freqmhz=(double)DisplayFreq/1000000;
    printf("Demanded Frequency = %.6f\n", freqmhz);
    snprintf(transfer, 13, "%.6f", freqmhz);
    strcat(StartPortsdown, transfer);
    strcat(StartPortsdown, " ");
    strcat(StartPortsdown, ref_freq_4351);
    strcat(StartPortsdown, " ");
    snprintf(transfer, 2, "%d", adf4351_lev);
    strcat(StartPortsdown, transfer);
    printf(StartPortsdown);
    printf("\nStarting ADF4351 Output\n");
    system(StartPortsdown);
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    if (ModOn == 0)  // Start Portsdown without Mod
    {
      PortsdownOn();
    }
    else           // Start Portsdown with Mod
    {
      PortsdownOnWithMod();
    }
  }

  if (strcmp(osctxt, "express")==0)
  {
    if (ModOn == 0)  // Start Express without Mod
    {
      ExpressOn();
    }
    else           // Start Express with Mod
    {
      ExpressOnWithMod();
    }
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    //printf("\nStarting ADF5355 output\n");
    printf("\nStarting Elcom Output\n");
    ElcomOn();
  }

  SetButtonStatus(13,1);
  SetButtonStatus(47,1);
  OutputStatus = 1;
}

void OscStop()
{
  char expressrx[256];
  printf("Oscillator Stop\n");

  if (strcmp(osctxt, "audio")==0)
  {
    printf("\nStopping audio output\n");
  }

  if (strcmp(osctxt, "adf4351")==0)
  {
    system("sudo /home/pi/rpidatv/bin/adf4351 off");    
    printf("\nStopping adf4351 output\n");
  }

  if (strcmp(osctxt, "portsdown")==0)
  {
    system("sudo /home/pi/rpidatv/bin/adf4351 off");    
    printf("\nStopping Portsdown output\n");
  }

  if (strcmp(osctxt, "express")==0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    //strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    //system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
    printf("\nStopping Express output\n");
  }

  if (strcmp(osctxt, "adf5355")==0)
  {
    printf("\nStopping ADF5355 output\n");
  }

  // Kill the key processes as nicely as possible
  system("sudo killall rpidatv >/dev/null 2>/dev/null");
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim >/dev/null 2>/dev/null");
  system("sudo killall avc2ts.old >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts.old has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts.old >/dev/null 2>/dev/null");

  // And make sure rpidatv has been stopped (required for brief transmit selections)
  system("sudo killall -9 rpidatv >/dev/null 2>/dev/null");
  SetButtonStatus(13,0);
  SetButtonStatus(47,0);
  OutputStatus = 0;
}

void coordpoint(VGfloat x, VGfloat y, VGfloat size, VGfloat pcolor[4])
{
  setfill(pcolor);
  Circle(x, y, size);
  setfill(pcolor);
}

void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
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

void MsgBox(const char *message)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  TextMid(wscreen/2, hscreen/2, message, SansTypeface, 25);

  VGfloat tw = TextWidth("Touch Screen to Continue", SansTypeface, 25);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SansTypeface, 25);
  End();
}

void MsgBox2(const char *message1, const char *message2)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  VGfloat th = TextHeight(SansTypeface, 25);


  TextMid(wscreen/2, hscreen/2+th, message1, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2-th, message2, SansTypeface, 25);

  VGfloat tw = TextWidth("Touch Screen to Continue", SansTypeface, 25);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SerifTypeface, 25);
  End();
}

void MsgBox4(const char *message1, const char *message2, const char *message3, const char *message4)
{
  //init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text

  VGfloat th = TextHeight(SansTypeface, 25);

  TextMid(wscreen/2, hscreen/2 + 2.1 * th, message1, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 + 0.7 * th, message2, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 - 0.7 * th, message3, SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2 - 2.1 * th, message4, SansTypeface, 25);

  End();
}

void waituntil(int w,int h)
// wait for a screen touch and act on its position
{
  int rawX, rawY, rawPressure, i, ExitSignal;
  ExitSignal=0; 

  // Start the main loop for the Touchscreen
  // Loop forever until exit button touched
  while (ExitSignal==0)
  {
    // Wait here until screen touched
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    // Screen has been touched
    printf("x=%d y=%d\n",rawX,rawY);

    // Check which Menu is currently displayed
    // and then check each button in turn
    // and take appropriate action
    switch (CurrentMenu)
    {
      case 1:
      for(i=0;i<Menu1Buttons;i++)
      {
        if(IsButtonPushed(i,rawX,rawY)==1)
        // So this number (i) button has been pushed
        {
          printf("Button Event %d\n",i);

          // Clear Preset store trigger if not a preset
          if ((i <= 4) || (i >= 9))
          {
            PresetStoreTrigger = 0;
            SetButtonStatus(12,0);
          }

          if((i>=0)&&(i<=4)) // Signal Source
          {
            OscStop();                // Stop the current output
            if ((i == 2) || (i == 3)  || i == 4)
            {
              // Check here that DATV Express is connected
              if (i == 4)
              {
                if (CheckExpressConnect() == 1)   // Not detected
                {
                  MsgBox2("DATV Express Not connected", "Connect it or select another mode");
                  wait_touch();
                }
              }
              SelectOsc(i);
            }
            else
            {
              MsgBox("Output Mode not implemented yet");
              wait_touch();
            }
          }
          if((i>=5)&&(i<=8)) // Presets 1-4
          {
            if (PresetStoreTrigger == 0)
            {
              RecallPreset(i-4);  // Recall preset
              OscStop();
              InitOsc();
              Start_Highlights_Menu1();
            }
            else
            {
              SavePreset(i-4);  // Set preset
              PresetStoreTrigger = 0;
              SetButtonStatus(12,0);
            }
          }
          if(i==9) // Signal Source
          {
            OscStop();               // Stop the current output
            MsgBox2("ADF5355 not implemented yet", "Selecting Elcom instead");
            wait_touch();

            SelectOsc(i);

          }
          if(i==10) // Attenuator in/out
          {
            if ((strcmp(osctxt, "portsdown")==0) || (strcmp(osctxt, "adf4351")==0))
            {
               if (AttenIn == 0)
              {
                if ((strcmp(AttenType, "PE4312")==0) || (strcmp(AttenType, "PE43713")==0) || (strcmp(AttenType, "HMC1119")==0))
                {
                  AttenIn=1;
                  SetAtten(atten);
                  SetButtonStatus(10,1);
                  if (strcmp(osctxt, "adf4351")==0)
                  {
                    level = 3;
                  }
                }
                else
                {
                  MsgBox4("No Attenuator Selected.", "Please select an Attenuator Type"
                    , "from the Console Setup Menu", "and restart SigGen");
                  wait_touch();
                }
              }
              else
              {
                AttenIn=0;
                SetAtten(0);        // Set min attenuation
                MsgBox4("If you had an Attenuator in-circuit", "you need to take it out of circuit,"
                  , "as it has a minimum attenuation of 2 dB", "and displayed output level will be incorrect");
                wait_touch();
                SetButtonStatus(10,0);
              }
              CalcOPLevel();
              InitOsc();
              if ((OutputStatus == 1) && (strcmp(osctxt, "adf4351")==0))  // adf5341 mode active already running
              {
                OscStart();  // Make sure that the adf4351 is running at the correct level
              }
            }
            else
            {
              AttenIn=0;
              MsgBox("Attenuator not implemented for this output");
              wait_touch();
            }
          }
          if(i==11) // Modulation on/off
          {
            if (ModOn == 0)
            {
              if ((strcmp(osctxt, "portsdown")==0) || (strcmp(osctxt, "express")==0))
              {
                ModOn=1;
                if (OutputStatus == 1)  // Oscillator already running
                {
                  OscStop();
                  OscStart();
                }
                SetButtonStatus(11,1);
                SetButtonStatus(Menu1Buttons+4,1);

              }
              else
              {
                MsgBox4("Modulation only available in", "Portsdown and DATV Express", "output modes", "");
                wait_touch();
              }
            }
            else
            {
              ModOn=0;
              if (OutputStatus == 1)  // Oscillator already running
              {
                OscStop();
                OscStart();
              }
              SetButtonStatus(11,0);
              SetButtonStatus(Menu1Buttons+4,0);
            }
            CalcOPLevel();
          }
          if(i==12) // Set Preset
          {
            SetButtonStatus(12,1);
            PresetStoreTrigger = 1;
          }
          if(i==13) // Output on
          {
            if (OutputStatus == 1)  // Oscillator already running
            {
              OscStop();
            }
            else
            {
              OscStart();
            }
          }
          if(i==14) // Output off
          {
            OscStop();
          }
          if(i==15) // Exit to Portsdown
          {
            OscStop();
            printf("Exiting Sig Gen \n");
            ExitSignal=1;
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          if(i==16) // Freq menu
          {
            printf("Switch to FREQ menu \n");
            CurrentMenu=2;
          }
          ShowTitle();
          ShowLevel(DisplayLevel);
          ShowFreq(DisplayFreq);
          printf("Calling ShowAtten\n");
          ShowAtten();
          ShowOPFreq();
          UpdateWindow();
        }
      }
      break;
    case 2:
      for(i=Menu1Buttons;i<(Menu1Buttons+Menu2Buttons);i++)
      {
        if(IsButtonPushed(i,rawX,rawY)==1)
        // So this number (i) button has been pushed
        {
          printf("Button Event %d\n",i);

          if(i>=((Menu1Buttons+0))&&(i<=(Menu1Buttons+2))) // Decrement Level
          {
            AdjustLevel(i);
            CalcOPLevel();
          }
          if(i==(Menu1Buttons+3)) // Save
          {
            SetButtonStatus(Menu1Buttons+3,1);
            UpdateWindow();
            SavePreset(0);
            usleep(250000);
            SetButtonStatus(Menu1Buttons+3,0);
          }
          if(i==(Menu1Buttons+4)) // Modulation on-off
          {
            if (ModOn == 0)
            {
              if ((strcmp(osctxt, "portsdown")==0) || (strcmp(osctxt, "express")==0))
              {
                ModOn=1;
                if (OutputStatus == 1)  // Oscillator already running
                {
                  OscStop();
                  OscStart();
                }
                SetButtonStatus(11,1);
                SetButtonStatus(Menu1Buttons+4,1);
              }
              else
              {
                MsgBox4("Modulation only available in", "Portsdown and DATV Express", "output modes.", "Touch screen to continue");
                wait_touch();
              }
            }
            else
            {
              ModOn=0;
              if (OutputStatus == 1)  // Oscillator already running
              {
                OscStop();
                OscStart();
              }
              SetButtonStatus(11,0);
              SetButtonStatus(Menu1Buttons+4,0);
            }
            CalcOPLevel();
          }
          if(i>=((Menu1Buttons+5))&&(i<=(Menu1Buttons+7))) // Increment Level
          {
            AdjustLevel(i);
            CalcOPLevel();
          }
          if(i>=((Menu1Buttons+8))&&(i<=(Menu1Buttons+29))) // Adjust Frequency
          {
            AdjustFreq(i);
            CalcOPLevel();
            if (OutputStatus == 1)
            {
              // OscStart(); Not required for portsdown, express or adf, but might be required for audio
            }
            BackgroundRGB(0,0,0,255);
            ShowTitle();
            ShowFreq(DisplayFreq);
            ShowLevel(DisplayLevel);
            ShowOPFreq();
            ShowAtten();
          }
          if(i==(Menu1Buttons+30)) // Oscillator on
          {
            if (OutputStatus == 1)  // Oscillator already running
            {
              OscStop();
            }
            else
            {
              OscStart();
            }
          }
          if(i==(Menu1Buttons+31))
          {
            OscStop();
          }
          if(i==(Menu1Buttons+32)) // Exit to Portsdown
          {
            OscStop();
            printf("Exiting Sig Gen \n");
            ExitSignal=1;
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          if(i==(Menu1Buttons+33)) // Freq menu
          {
            printf("Switch to CTRL menu \n");
            CurrentMenu=1;
            BackgroundRGB(0,0,0,255);
            Start_Highlights_Menu1();
            ShowTitle();
            ShowFreq(DisplayFreq);
            ShowLevel(DisplayLevel);
          }
          ShowTitle();
          ShowLevel(DisplayLevel);
          ShowFreq(DisplayFreq);
          ShowAtten();
          ShowOPFreq();
          UpdateWindow();
        }
      }
      break;
    default:
      break;
    }
  }
}

void Start_Highlights_Menu1()
// Retrieves memory value for each group of buttons
// and then sets the correct highlight
// Called on program start and after preset recall
{
  char Value[255];
  strcpy(Value, osctxt);
  if(strcmp(Value,TabOsc[0])==0)
  {
    SelectInGroup(0,4,0,1);
  }
  if(strcmp(Value,TabOsc[1])==0)
  {
    SelectInGroup(0,4,1,1);
  }
  if(strcmp(Value,TabOsc[2])==0)
  {
    SelectInGroup(0,4,2,1);
  }
  if(strcmp(Value,TabOsc[3])==0)
  {
    SelectInGroup(0,4,3,1);
  }
  if(strcmp(Value,TabOsc[4])==0)
  {
    SelectInGroup(0,4,4,1);
  }
  if(strcmp(Value,TabOsc[9])==0)
  {
    SelectInGroup(0,4,9,1);
  }
  if(AttenIn == 1)
  {
    SetButtonStatus(10,1);
  }
  else
  {
    SetButtonStatus(10,0);
  }
}

void Define_Menu1()
{
  Menu1Buttons=17;
  // Source: Audio, Pi RF, ADF4351, Portsdown, Express

	int button=AddButton(0*wbuttonsize+20,0+hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	color_t Col;
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Audio ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Audio ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Pi RF",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Pi RF",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"ADF4351",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"ADF4351",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Portsdown",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Portsdown",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*0+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"Express",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"Express",&Col);

// Presets

	button=AddButton(0*wbuttonsize+20,0+hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P1  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P1  ",&Col);

	button=AddButton(1*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P2  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P2  ",&Col);

	button=AddButton(2*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P3  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P3  ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  P4  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  P4  ",&Col);

  // Extra Source button

	button=AddButton(4*wbuttonsize+20,hbuttonsize*1+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"ADF5355",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"ADF5355",&Col);

// Space for Level then Atten (on/off) Mod (on/off) and Set

	button=AddButton(2*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," Atten ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," Atten ",&Col);

	button=AddButton(3*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  Mod  ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  Mod  ",&Col);

	button=AddButton(4*wbuttonsize+20,hbuttonsize*2+20,wbuttonsize*0.9,hbuttonsize*0.9);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  Save P",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  Save P",&Col);

//  ON, OFF, EXIT, FREQ

	button=AddButton(0*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  ON  ",&Col);
	Col.r=255;Col.g=0;Col.b=0;
	AddButtonStatus(button,"  ON  ",&Col);

        button=AddButton(1.35*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button,"  OFF ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button,"  OFF ",&Col);

        button=AddButton(2.7*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
	Col.r=0;Col.g=0;Col.b=128;
	AddButtonStatus(button," EXIT ",&Col);
	Col.r=0;Col.g=128;Col.b=0;
	AddButtonStatus(button," EXIT ",&Col);

        button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
        Col.r=0;Col.g=0;Col.b=128;
        AddButtonStatus(button,"Freq",&Col);
        Col.r=0;Col.g=128;Col.b=0;
        AddButtonStatus(button,"Freq",&Col);
}

void Define_Menu2()
{
  Menu2Buttons=34;
  // Bottom row: subtract 10s, units and tenths of a dB

  int button=AddButton(1*swbuttonsize+20,0+shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  color_t Col;
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2*swbuttonsize+20,shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.2*swbuttonsize+20,shbuttonsize*0+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  // Save and Mod Buttons

  button=AddButton(3*wbuttonsize+20,hbuttonsize*0.5+20,wbuttonsize*0.9,hbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," Save ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button," Save ",&Col);

  button=AddButton(4*wbuttonsize+20,hbuttonsize*0.5+20,wbuttonsize*0.9,hbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  Mod ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"  Mod ",&Col);

  // Bottom row: add 10s, units and tenths of a dB

  button=AddButton(1*swbuttonsize+20,0+shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2*swbuttonsize+20,shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.2*swbuttonsize+20,shbuttonsize*2+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  // Decrement frequency: subtract 10s, units and tenths of a dB

  button=AddButton(0.4*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(1.4*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2.6*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.6*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(4.6*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(5.8*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(6.8*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(7.8*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(9*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(10*swbuttonsize+20,0+shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(11*swbuttonsize+20,shbuttonsize*3.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," _ ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);


  // Increment frequency

  button=AddButton(0.4*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(1.4*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(2.6*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(3.6*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(4.6*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(5.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(6.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(7.8*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(9*swbuttonsize+20,0+shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(10*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

  button=AddButton(11*swbuttonsize+20,shbuttonsize*5.5+20,swbuttonsize*0.9,shbuttonsize*0.9);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," + ",&Col);
  Col.r=0;Col.g=0;Col.b=0;
  AddButtonStatus(button," ",&Col);

//  ON, OFF, EXIT, CTRL

  button=AddButton(0*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  ON  ",&Col);
  Col.r=255;Col.g=0;Col.b=0;
  AddButtonStatus(button,"  ON  ",&Col);

  button=AddButton(1.35*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"  OFF ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"  OFF ",&Col);

  button=AddButton(2.7*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*1.2,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button," EXIT ",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button," EXIT ",&Col);

  button=AddButton(4*wbuttonsize+20,hbuttonsize*4+20,wbuttonsize*0.9,hbuttonsize*1.2);
  Col.r=0;Col.g=0;Col.b=128;
  AddButtonStatus(button,"Config",&Col);
  Col.r=0;Col.g=128;Col.b=0;
  AddButtonStatus(button,"Config",&Col);
}

static void
terminate(int dummy)
{
  OscStop();
  printf("Terminate\n");
  finish();
  char Commnd[255];
  sprintf(Commnd, "sudo killall express_server");
  system(Commnd);
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// Main initializes the system and shows the Control menu (Menu 1) 

int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  char Param[255];
  char Value[255];

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

  // Determine if using Waveshare or Waveshare B or Waveshare 4 inch screen
  // from rpidatvconfig.txt
  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if((strcmp(Value,"Waveshare")==0) || (strcmp(Value,"WaveshareB")==0) || (strcmp(Value,"Waveshare4")==0))
  {
    Inversed = 1;
  }

  // Look up ADF4351 Ref Freq from Portsdown Config
  strcpy(Param, "adfref");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(ref_freq_4351, Value);

  // Look up attenuator type from Portsdown Config
  strcpy(Param, "attenuator");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(AttenType, Value);

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
  //printf ("Y Scale Factor = %f\n", scaleYvalue);

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize=(wscreen-25)/5;  // width of normal button
  hbuttonsize=hscreen/6;       // height of normal button
  swbuttonsize=(wscreen-25)/12;  // width of small button
  shbuttonsize=hscreen/10;       // height of small button

  // Set the Portsdown band to 1255 to bypass the LO filter
  SetBandGPIOs();
  
  // Read in the touchscreen Calibration
  ReadTouchCal();

  // Read in the presets from the Config file
  ReadPresets();

  // Define the buttons for Menu 1
  Define_Menu1();

  // Define the buttons for Menu 2
  Define_Menu2();

  // Initialise the attenuator
  if ((strcmp(osctxt, "portsdown")==0) || (strcmp(osctxt, "adf4351")==0))
  {
    if ((strcmp(AttenType, "PE4312")==0) || (strcmp(AttenType, "PE43713")==0) || (strcmp(AttenType, "HMC1119")==0))
    {
      AttenIn = 1;
    }
  }

  // Start the button Menu
  Start(wscreen,hscreen);
  IsDisplayOn=1;

  // Determine button highlights
  Start_Highlights_Menu1();

  // Validate the current value and initialise the oscillator
  InitOsc();

  ShowTitle();
  ShowLevel(DisplayLevel);
  ShowFreq(DisplayFreq);

  UpdateWindow();
  printf("Update Window\n");

  // Start the output if called from startup script
  if(argc>1)
  {
    if (strcmp(argv[1], "on") == 0)
    {
      //Start it
      OscStart();
      SetButtonStatus(13, 1);
      SetButtonStatus(Menu1Buttons+30, 1);  ShowTitle();
      ShowLevel(DisplayLevel);
      ShowFreq(DisplayFreq);
      UpdateWindow();

      waituntil(wscreen,hscreen);  // and return control to touch
    }
    else
    {
      waituntil(wscreen,hscreen);
    }
  }
  else
  {
    // Go and wait for the screen to be touched
    waituntil(wscreen,hscreen);
  }

  // Program flow only gets here when exit button pushed
  OscStop();

  // Shutdown the graphics system so that there are no hanging menus
  finish();

  //Tidy up the terminal
  char Commnd[255];
  strcpy(Commnd, "echo \"set kill\" >> /tmp/expctrl");
  system(Commnd);
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);

  // Exit with code 129, so that the scheduler restarts rpidatvgui
  exit(129);
}
