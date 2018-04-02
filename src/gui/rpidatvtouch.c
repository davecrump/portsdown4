// rpidatvtouch.c
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

  Initial code by Evariste F5OEO
  Rewitten by Dave, G8GKQ
*/
//
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
#include <ctype.h>

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

#define PATH_CONFIG "/home/pi/rpidatv/scripts/rpidatvconfig.txt"
#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"
#define PATH_PPRESETS "/home/pi/rpidatv/scripts/portsdown_presets.txt"
#define PATH_TOUCHCAL "/home/pi/rpidatv/scripts/touchcal.txt"
#define PATH_SGCONFIG "/home/pi/rpidatv/src/siggen/siggenconfig.txt"

char ImageFolder[]="/home/pi/rpidatv/image/";

int fd=0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;

typedef struct {
	int r,g,b;
} color_t;

typedef struct {
	char Text[255];
	color_t  Color;
} status_t;

#define MAX_STATUS 10
typedef struct {
	int x,y,w,h;                   // Position and size
	status_t Status[MAX_STATUS];   // Array of text and required colour for each status
	int IndexStatus;               // The number of valid status definitions.  0 = do not display
	int NoStatus;                  // This is the active status (colour and text)
} button_t;

// 	int LastEventTime; Was part of button_t.  No longer used

#define MAX_BUTTON 600
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
#define TIME_ANTI_BOUNCE 500
int CurrentMenu=1;

//GLOBAL PARAMETERS

int fec;
int SR;
char ModeInput[255];
// char freqtxt[255]; not global
char ModeAudio[255];
char ModeOutput[255];
char ModeSTD[255];
char ModeVidIP[255];
char ModeOP[255];
char Caption[255];
char CallSign[255];
char Locator[15];
char PIDstart[15];
char PIDvideo[15];
char PIDaudio[15];
char PIDpmt[15];
char ADFRef[3][15];
char CurrentADFRef[15];
char UpdateStatus[31] = "NotAvailable";
//char ADF5355Ref[15];
int  scaledX, scaledY;
VGfloat CalShiftX = 0;
VGfloat CalShiftY = 0;
float CalFactorX = 1.0;
float CalFactorY = 1.0;
int GPIO_PTT = 29;
int GPIO_SPI_CLK = 21;
int GPIO_SPI_DATA = 22;
int GPIO_4351_LE = 30;
int GPIO_Atten_LE = 16;
int GPIO_5355_LE = 15;

char ScreenState[255] = "NormalMenu";  // NormalMenu SpecialMenu TXwithMenu TXwithImage RXwithImage VideoOut SnapView VideoView Snap SigGen
char MenuTitle[40][127];

// Values for buttons
// May be over-written by values from from portsdown_config.txt:
char TabBand[9][3]={"d1", "d2", "d3", "d4", "d5", "t1", "t2", "t3", "t4"};
char TabBandLabel[9][15]={"71_MHz", "146_MHz", "70_cm", "23_cm", "13_cm", "9_cm", "6_cm", "3_cm", "1.2_cm"};
char TabPresetLabel[4][15]={"-", "-", "-", "-"};
float TabBandAttenLevel[9]={-10, -10, -10, -10, -10, -10, -10, -10, -10};
int TabBandExpLevel[9]={30, 30, 30, 30, 30, 30, 30, 30, 30};
int TabBandExpPorts[9]={2, 2, 2, 2, 2, 2, 2, 2, 2};
float TabBandLO[9]={0, 0, 0, 0, 0, 3024, 5328, 9936, 23616};
char TabBandNumbers[9][10]={"1111", "2222", "3333", "4444", "5555", "6666", "7777", "8888", "9999"};
int TabSR[9]={125,333,1000,2000,4000, 88, 250, 500, 3000};
char SRLabel[9][255]={"SR 125","SR 333","SR1000","SR2000","SR4000", "SR 88", "SR 250", "SR 500", "SR3000"};
int TabFec[5]={1,2,3,5,7};
char TabModeInput[12][255]={"CAMMPEG-2","CAMH264","PATERNAUDIO","ANALOGCAM","CARRIER","CONTEST"\
  ,"IPTSIN","ANALOGMPEG-2", "CARDMPEG-2", "CAMHDMPEG-2", "DESKTOP", "FILETS"};
char TabFreq[9][255]={"71", "146.5", "437", "1249", "1255", "436", "436.5", "437.5", "438"};
char FreqLabel[31][255];
char TabModeAudio[6][15]={"auto", "mic", "video", "bleeps", "no_audio", "webcam"};
char TabModeSTD[2][7]={"6","0"};
char TabModeVidIP[2][7]={"0","1"};
char TabModeOP[8][31]={"IQ", "QPSKRF", "DATVEXPRESS", "BATC", "STREAMER", "COMPVID", "DTX1", "IP"};
char TabModeOPtext[8][31]={"Portsdown", " UGLY ", "EXPRESS", " BATC ", "STREAM", "ANALOG", " DTX1 ", "IPTS out"};
char TabAtten[4][15] = {"NONE", "PE4312", "PE43713", "HMC1119"};
char CurrentModeOP[31] = "QPSKRF";
char CurrentModeOPtext[31] = " UGLY ";
char TabTXMode[2][255] = {"DVB-S", "Carrier"};
char CurrentTXMode[255] = "DVB-S";
char CurrentModeInput[255] = "DESKTOP";
char TabEncoding[4][255] = {"H264", "MPEG-2", "IPTS in", "TS File"};
char CurrentEncoding[255] = "H264";
char TabSource[8][15] = {"Pi Cam", "CompVid", "TCAnim", "TestCard", "PiScreen", "Contest", "Webcam", "C920"};
char CurrentSource[15] = "PiScreen";
char TabFormat[4][15] = {"4:3", "16:9","720p", "1080p"};
char CurrentFormat[15] = "4:3";
char CurrentCaptionState[255] = "on";
char CurrentAudioState[255] = "auto";
char CurrentAtten[255] = "NONE";
int CurrentBand = 2; // 0 thru 8
char KeyboardReturn[64];
char FreqBtext[31];
char MenuText[5][63];

int Inversed=0;           //Display is inversed (Waveshare=1)
int PresetStoreTrigger=0; //Set to 1 if awaiting preset being stored

pthread_t thfft,thbutton,thview,thwait3;

// Function Prototypes

void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();
void Start_Highlights_Menu11();
void Start_Highlights_Menu12();
void Start_Highlights_Menu13();
void Start_Highlights_Menu14();
void Start_Highlights_Menu15();
void Start_Highlights_Menu16();
void Start_Highlights_Menu17();
void Start_Highlights_Menu18();
void Start_Highlights_Menu19();
void Start_Highlights_Menu21();
void Start_Highlights_Menu22();
void Start_Highlights_Menu23();
void Start_Highlights_Menu24();
void Start_Highlights_Menu26();
void Start_Highlights_Menu27();
void Start_Highlights_Menu28();
void Start_Highlights_Menu29();
//void Start_Highlights_Menu30();
//void Start_Highlights_Menu31();
void Start_Highlights_Menu32();
void Start_Highlights_Menu33();
void MsgBox(const char *);
void MsgBox2(const char *, const char *);
void MsgBox4(const char *, const char *, const char *, const char *);
void wait_touch();
void waituntil(int, int);
void Keyboard(char *, char *, int);
void DoFreqChange();


int getTouchSample(int *, int *, int *);
void TransformTouchMap(int, int);
int ButtonNumber(int, int);

/***************************************************************************//**
 * @brief Looks up the value of Param in PathConfigFile and sets value
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
			//strncpy(Value,line+strlen(Param)+1,strlen(line)-strlen(Param)-1-1/* pour retour chariot*/);
	    	}
	}
	else
		printf("Config file not found \n");
	fclose(fp);

}

/***************************************************************************//**
 * @brief sets the value of Param in PathConfigFile froma program variable
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
 * @brief Looks up the current Linux Version
 *
 * @param nil
 *
 * @return 8 for Jessie, 9 for Stretch
*******************************************************************************/

int GetLinuxVer()
{
  FILE *fp;
  char version[7];
  int ver = 0;

  /* Open the command for reading. */
  fp = popen("cat /etc/issue | grep -E -o \"8|9\"", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(version, 6, fp) != NULL)
  {
    //printf("%s", version);
  }

  /* close */
  pclose(fp);
  if ((atoi(version) == 8) || (atoi(version) == 9))
  {
    ver = atoi(version);
  }
  return ver;
}


/***************************************************************************//**
 * @brief Looks up the current IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetIPAddr(char IPAddress[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' | head -1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 16, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}
/***************************************************************************//**
 * @brief Looks up the current second IPV4 address
 *
 * @param IPAddress (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetIPAddr2(char IPAddress[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("ifconfig | grep -Eo \'inet (addr:)?([0-9]*\\.){3}[0-9]*\' | grep -Eo \'([0-9]*\\.){3}[0-9]*\' | grep -v \'127.0.0.1\' | sed -n '2 p'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(IPAddress, 16, fp) != NULL)
  {
    //printf("%s", IPAddress);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the current Software Version
 *
 * @param SVersion (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSWVers(char SVersion[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /home/pi/rpidatv/scripts/installed_version.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SVersion, 16, fp) != NULL)
  {
    //printf("%s", SVersion);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the latest Software Version from file
 *
 * @param SVersion (str) IP Address to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetLatestVers(char LatestVersion[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /home/pi/rpidatv/scripts/latest_version.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(LatestVersion, 16, fp) != NULL)
  {
    //printf("%s", LatestVersion);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks whether a ping to google on 8.8.8.8 works
 *
 * @param nil
 *
 * @return 0 if it pings OK, 1 if it doesn't
*******************************************************************************/

int CheckGoogle()
{
  FILE *fp;
  char response[127];

  /* Open the command for reading. */
  fp = popen("ping 8.8.8.8 -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    printf("%s", response);
  }
  //  printf("%s", response);
  /* close */
  pclose(fp);
  if (strcmp (response, "1 received,") == 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

/***************************************************************************//**
 * @brief Displays a splash screen with update progress
 *
 * @param char Version (Latest or Developement), char Step (centre message)
 *
 * @return void
*******************************************************************************/

void DisplayUpdateMsg(char* Version, char* Step)
{
  char LinuxCommand[511];

  // Delete any old image
  strcpy(LinuxCommand, "rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Build and run the convert command for the image
  strcpy(LinuxCommand, "convert -size 720x576 xc:white ");

  strcat(LinuxCommand, "-gravity North -pointsize 40 -annotate 0 ");
  strcat(LinuxCommand, "\"\\nUpdating Portsdown Software\\nTo ");
  strcat(LinuxCommand, Version);
  strcat(LinuxCommand, " Version\" ");
 
  strcat(LinuxCommand, "-gravity Center -pointsize 50 -annotate 0 \"");
  strcat(LinuxCommand, Step);
  strcat(LinuxCommand, "\\n\\nPlease wait\" ");

  strcat(LinuxCommand, "-gravity South -pointsize 50 -annotate 0 ");
  strcat(LinuxCommand, "\"DO NOT TURN POWER OFF\" ");

  strcat(LinuxCommand, "/home/pi/tmp/update.jpg");

  system(LinuxCommand);

  // Display the image on the desktop
  strcpy(LinuxCommand, "sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg ");
  strcat(LinuxCommand, ">/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Kill fbi
  strcpy(LinuxCommand, "(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
  system(LinuxCommand);
}

/***************************************************************************//**
 * @brief checks whether software update available
 *
 * @param 
 *
 * @return void
*******************************************************************************/

void PrepSWUpdate()
{
  char CurrentVersion[256];
  char LatestVersion[256];
  char CurrentVersion9[15];
  char LatestVersion9[15];
  char LinuxCommand[256];

  strcpy(UpdateStatus, "NotAvailable");

  // delete old latest version file
  system("rm /home/pi/rpidatv/scripts/latest_version.txt  >/dev/null 2>/dev/null");

  // Download new latest version file
  strcpy(LinuxCommand, "wget --timeout=2 https://raw.githubusercontent.com/BritishAmateurTelevisionClub/");
  if (GetLinuxVer() == 8)  // Jessie, so rpidatv repo
  {
    strcat(LinuxCommand, "rpidatv/master/scripts/latest_version.txt ");
  }
  else                     // Stretch, so portsdown repo
  {
    strcat(LinuxCommand, "portsdown/master/scripts/latest_version.txt ");
  }
  strcat(LinuxCommand, "-O /home/pi/rpidatv/scripts/latest_version.txt  >/dev/null 2>/dev/null");
  system(LinuxCommand);

  // Fetch the current and latest versions and make sure we have 9 characters
  GetSWVers(CurrentVersion);
  snprintf(CurrentVersion9, 10, "%s", CurrentVersion);
  GetLatestVers(LatestVersion);
  snprintf(LatestVersion9, 10, "%s", LatestVersion);
  snprintf(MenuText[0], 40, "Current Software Version: %s", CurrentVersion);
  strcpy(MenuText[1], " ");
  strcpy(MenuText[2], " ");

  // Check latest version starts with 20*
  if ( !((LatestVersion[0] == 50) && (LatestVersion[1] == 48)) )
  {
    // Invalid response from GitHub.  Check Google ping
    if (CheckGoogle() == 0)
    {
      strcpy(MenuText[2], "Unable to contact GitHub for update");
      strcpy(MenuText[3], "Internet connection to Google seems OK");
      strcpy(MenuText[4], "Please check BATC Wiki FAQ for advice");
    }
    else
    {
      strcpy(MenuText[2], "Unable to contact GitHub for update");
      strcpy(MenuText[3], "There appears to be no Internet connection");
      strcpy(MenuText[4], "Please check your connection and try again");
    }
  }
  else
  {
    snprintf(MenuText[1], 40, "Latest Software Version:   %s", LatestVersion);

    // Compare versions
    if (atoi(LatestVersion9) > atoi(CurrentVersion9))
    {
      strcpy(MenuText[3], "A software update is available");
      strcpy(MenuText[4], "Do you want to update now?");
      strcpy(UpdateStatus, "NormalUpdate");
    }
    if (atoi(LatestVersion9) == atoi(CurrentVersion9))
    {
      strcpy(MenuText[2], "Your software is the latest version");
      strcpy(MenuText[3], "There is no need to update");
      strcpy(MenuText[4], "Do you want to force an update now?");
      strcpy(UpdateStatus, "ForceUpdate");
    }
    if (atoi(LatestVersion9) < atoi(CurrentVersion9))
    {
      strcpy(MenuText[2], " ");
      strcpy(MenuText[3], "Your software is newer than the latest version.");
      strcpy(MenuText[4], "Do you want to force an update now?");
      strcpy(UpdateStatus, "ForceUpdate");
    }
  }
}

/***************************************************************************//**
 * @brief Acts on Software Update buttons
 *
 * @param int NoButton, which is 5, 6 or 7
 *
 * @return void
*******************************************************************************/

void ExecuteUpdate(int NoButton)
{
  char Step[255];
  char LinuxCommand[256];

  switch(NoButton)
  {
  case 5:
    if ((strcmp(UpdateStatus, "NormalUpdate") == 0) || (strcmp(UpdateStatus, "ForceUpdate") == 0))

    {
      // code for normal update

      // Display the updating message
      finish();
      strcpy(Step, "Step 1 of 10\\nDownloading Update\\n\\nX---------");
      DisplayUpdateMsg("Latest" , Step);

      // Delete any old update
      strcpy(LinuxCommand, "rm /home/pi/update.sh >/dev/null 2>/dev/null");
      system(LinuxCommand);

      if (GetLinuxVer() == 8)  // Jessie, so rpidatv repo
      {
        printf("Downloading Normal Update Jessie Version\n");
        strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/rpidatv/master/update.sh");
        strcat(LinuxCommand, " -O /home/pi/update.sh");
        system(LinuxCommand);

        strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
        DisplayUpdateMsg("Latest Jessie", Step);
      }
      else
      {
        printf("Downloading Normal Update Stretch Version\n");
        strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/portsdown/master/update.sh");
        strcat(LinuxCommand, " -O /home/pi/update.sh");
        system(LinuxCommand);

        strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
        DisplayUpdateMsg("Latest Stretch", Step);
      }
      strcpy(LinuxCommand, "chmod +x /home/pi/update.sh");   
      system(LinuxCommand);
      system("reset");
      exit(132);  // Pass control back to scheduler
    }
    break;
  case 6:
    if (strcmp(UpdateStatus, "ForceUpdate") == 0)
    {
        strcpy(UpdateStatus, "DevUpdate");
    }
    break;
  case 7:
    if (strcmp(UpdateStatus, "DevUpdate") == 0)
    {
      // Code for Dev Update
      // Display the updating message
      finish();
      strcpy(Step, "Step 1 of 10\\nDownloading Update\\n\\nX---------");
      DisplayUpdateMsg("Development", Step);

      // Delete any old update
      strcpy(LinuxCommand, "rm /home/pi/update.sh >/dev/null 2>/dev/null");
      system(LinuxCommand);

      if (GetLinuxVer() == 8)  // Jessie, so rpidatv repo
      {
        printf("Downloading Development Update for Jessie\n");
        strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/davecrump/rpidatv/master/update.sh");
        strcat(LinuxCommand, " -O /home/pi/update.sh");
        system(LinuxCommand);

        strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
        DisplayUpdateMsg("Development Jessie", Step);
      }
      else
      {
        printf("Downloading Development Update Stretch Version\n");
        strcpy(LinuxCommand, "wget https://raw.githubusercontent.com/davecrump/portsdown/master/update.sh");
        strcat(LinuxCommand, " -O /home/pi/update.sh");
        system(LinuxCommand);

        strcpy(Step, "Step 2 of 10\\nLoading Update Script\\n\\nXX--------");
        DisplayUpdateMsg("Development Stretch", Step);
      }
      strcpy(LinuxCommand, "chmod +x /home/pi/update.sh");   
      system(LinuxCommand);
      system("reset");
      exit(133);  // Pass control back to scheduler for Dev Load
    }
    break;
  default:
    break;
  }
}


/***************************************************************************//**
 * @brief Looks up the GPU Temp
 *
 * @param GPUTemp (str) GPU Temp to be passed as a string max 20 char
 *
 * @return void
*******************************************************************************/

void GetGPUTemp(char GPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("/opt/vc/bin/vcgencmd measure_temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(GPUTemp, 20, fp) != NULL)
  {
    printf("%s", GPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the CPU Temp
 *
 * @param CPUTemp (str) CPU Temp to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetCPUTemp(char CPUTemp[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/class/thermal/thermal_zone0/temp", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(CPUTemp, 20, fp) != NULL)
  {
    printf("%s", CPUTemp);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Checks the CPU Throttling Status
 *
 * @param Throttled (str) Throttle status to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetThrottled(char Throttled[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("vcgencmd get_throttled", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(Throttled, 20, fp) != NULL)
  {
    printf("%s", Throttled);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Reads the input source from portsdown_config.txt
 *        and determines coding and video source
 * @param sets strings: coding, source
 *
 * @return void
*******************************************************************************/

void ReadModeInput(char coding[256], char vsource[256])
{
  char ModeInput[256];
  GetConfigParam(PATH_PCONFIG,"modeinput", ModeInput);

  strcpy(coding, "notset");
  strcpy(vsource, "notset");

  if (strcmp(ModeInput, "CAMH264") == 0) 
  {
    strcpy(coding, "H264");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  } 
  else if (strcmp(ModeInput, "ANALOGCAM") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Static Test Card F");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "PATERNAUDIO") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Test Card");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[2]); // TCAnim
  }
  else if (strcmp(ModeInput, "CONTEST") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "DESKTOP") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Screen");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[4]); // Desktop
  }
  else if (strcmp(ModeInput, "CARRIER") == 0)
  {
    strcpy(coding, "DC");
    strcpy(vsource, "Plain Carrier");
    strcpy(CurrentTXMode, "Carrier");
  }
  else if (strcmp(ModeInput, "TESTMODE") == 0)
  {
    strcpy(coding, "Square Wave");
    strcpy(vsource, "Test");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
  }
  else if (strcmp(ModeInput, "FILETS") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "TS File");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentEncoding, "TS File");
  }
  else if (strcmp(ModeInput, "IPTSIN") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "IP Transport Stream");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "IPTS in");
  }
  else if (strcmp(ModeInput, "VNC") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "VNC");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
  }
  else if (strcmp(ModeInput, "CAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOGMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[3]); // Desktop
  }
  else if (strcmp(ModeInput, "CONTESTMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "CAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam 16:9");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "CAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam HD");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOG16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "WEBCAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARD16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "CARDHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "C920H264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[7]); // C920
  }
  else if (strcmp(ModeInput, "C920HDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[7]); // C920
  }
  else if (strcmp(ModeInput, "C920FHDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
    strcpy(CurrentTXMode, "DVB-S");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "1080p");
    strcpy(CurrentSource, TabSource[7]); // C920
  }
  else
  {
    strcpy(coding, "notset");
    strcpy(vsource, "notset");
  }
}

/***************************************************************************//**
 * @brief Reads the output mode from portsdown_config.txt
 *        and determines the user-friendly string for display
 * @param sets strings: Moutput
 *
 * @return void
*******************************************************************************/

void ReadModeOutput(char Moutput[256])
{
  char ModeOutput[256];
  GetConfigParam(PATH_PCONFIG,"modeoutput", ModeOutput);
  strcpy(CurrentModeOP, ModeOutput);
  strcpy(Moutput, "notset");

  if (strcmp(ModeOutput, "IQ") == 0) 
  {
    strcpy(Moutput, "Filter-modulator Board");
    strcpy(CurrentModeOPtext, TabModeOPtext[0]);
  } 
  else if (strcmp(ModeOutput, "QPSKRF") == 0) 
  {
    strcpy(Moutput, "Ugly mode for testing");
    strcpy(CurrentModeOPtext, TabModeOPtext[1]);
  } 
  else if (strcmp(ModeOutput, "DATVEXPRESS") == 0) 
  {
    strcpy(Moutput, "DATV Express by USB");
    strcpy(CurrentModeOPtext, TabModeOPtext[2]);
  } 
  else if (strcmp(ModeOutput, "BATC") == 0) 
  {
    strcpy(Moutput, "BATC Streaming");
    strcpy(CurrentModeOPtext, TabModeOPtext[3]);
  } 
  else if (strcmp(ModeOutput, "STREAMER") == 0) 
  {
    strcpy(Moutput, "Bespoke Streaming");
    strcpy(CurrentModeOPtext, TabModeOPtext[4]);
  } 
  else if (strcmp(ModeOutput, "COMPVID") == 0) 
  {
    strcpy(Moutput, "Composite Video");
    strcpy(CurrentModeOPtext, TabModeOPtext[5]);
  } 
  else if (strcmp(ModeOutput, "DTX1") == 0) 
  {
    strcpy(Moutput, "DTX-1 Modulator");
    strcpy(CurrentModeOPtext, TabModeOPtext[6]);
  } 
  else if (strcmp(ModeOutput, "IP") == 0) 
  {
    strcpy(Moutput, "IP Stream");
    strcpy(CurrentModeOPtext, TabModeOPtext[7]);
  } 
  else if (strcmp(ModeOutput, "DIGITHIN") == 0) 
  {
    strcpy(Moutput, "DigiThin Board");
  } 
  else
  {
    strcpy(Moutput, "notset");
  }
}

/***************************************************************************//**
 * @brief Reads the EasyCap modes from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadModeEasyCap()
{
  // char ModeOutput[256];
  GetConfigParam(PATH_PCONFIG,"analogcaminput", ModeVidIP);
  GetConfigParam(PATH_PCONFIG,"analogcamstandard", ModeSTD);
}

/***************************************************************************//**
 * @brief Reads the Caption State from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadCaptionState()
{
  GetConfigParam(PATH_PCONFIG,"caption", CurrentCaptionState);
}

/***************************************************************************//**
 * @brief Reads the Audio State from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadAudioState()
{
  GetConfigParam(PATH_PCONFIG,"audio", CurrentAudioState);
}

/***************************************************************************//**
 * @brief Reads the current Attenuator from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadAttenState()
{
  GetConfigParam(PATH_PCONFIG,"attenuator", CurrentAtten);
}

/***************************************************************************//**
 * @brief Reads the current band from portsdown_config.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadBand()
{
  char Param[15];
  char Value[15]="";
  float CurrentFreq;

  // Look up the current frequency
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  CurrentFreq = atof(Value);
  strcpy(Value,"");

  // Look up the current band
  strcpy(Param,"band");
  GetConfigParam(PATH_PCONFIG, Param, Value);
 
  if (strcmp(Value, "t1") == 0)
  {
    CurrentBand = 5;
  }
  if (strcmp(Value, "t2") == 0)
  {
    CurrentBand = 6;
  }
  if (strcmp(Value, "t3") == 0)
  {
    CurrentBand = 7;
  }
  if (strcmp(Value, "t4") == 0)
  {
    CurrentBand = 8;
  }

  if ((strcmp(Value, TabBand[0]) == 0) || (strcmp(Value, TabBand[1]) == 0)
   || (strcmp(Value, TabBand[2]) == 0) || (strcmp(Value, TabBand[3]) == 0)
   || (strcmp(Value, TabBand[4]) == 0))
  {
    // Not a transverter, so set band based on the current frequency

    if (CurrentFreq < 100)                            // 71 MHz
    {
       CurrentBand = 0;
       strcpy(Value, "d1");
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      CurrentBand = 1;
      strcpy(Value, "d2");
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      CurrentBand = 2;
      strcpy(Value, "d3");
    }
    if ((CurrentFreq >= 950) && (CurrentFreq <2000))  // 1255 MHz
    {
      CurrentBand = 3;
      strcpy(Value, "d4");
    }
    if (CurrentFreq >= 2000)                          // 2400 MHz
    {
      CurrentBand = 4;
      strcpy(Value, "d5");
    }

    // And set the band correctly
    strcpy(Param,"band");
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }
    printf("In ReadBand, CurrentFreq = %f, CurrentBand = %d and band desig = %s\n", CurrentFreq, CurrentBand, Value);
}

/***************************************************************************//**
 * @brief Reads the current band details from portsdown_presets.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadBandDetails()
{
  int i;
  char Param[31];
  char Value[31]="";
  for( i = 0; i < 9; i = i + 1)
  {
    strcpy(Param, TabBand[i]);
    strcat(Param, "label");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabBandLabel[i], Value);
    
    strcpy(Param, TabBand[i]);
    strcat(Param, "attenlevel");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandAttenLevel[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "explevel");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandExpLevel[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "expports");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandExpPorts[i] = atoi(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "lo");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    TabBandLO[i] = atof(Value);

    strcpy(Param, TabBand[i]);
    strcat(Param, "numbers");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabBandNumbers[i], Value);
  }
}

/***************************************************************************//**
 * @brief Reads the current Call, Locator and PIDs from portsdown_presets.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadCallLocPID()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "call");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(CallSign, Value);

  strcpy(Param, "locator");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(Locator, Value);

  strcpy(Param, "pidstart");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDstart, Value);

  strcpy(Param, "pidvideo");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDvideo, Value);

  strcpy(Param, "pidaudio");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDaudio, Value);

  strcpy(Param, "pidpmt");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(PIDpmt, Value);
}


/***************************************************************************//**
 * @brief Reads the current ADF Ref Freqs from portsdown_presets.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/
void ReadADFRef()
{
  char Param[31];
  char Value[255]="";

  strcpy(Param, "adfref1");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[0], Value);

  strcpy(Param, "adfref2");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[1], Value);

  strcpy(Param, "adfref3");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(ADFRef[2], Value);

  strcpy(Param, "adfref");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(CurrentADFRef, Value);

  //strcpy(Param, "adf5355ref");
  //GetConfigParam(PATH_SGCONFIG, Param, Value);
  //strcpy(ADF5355Ref, Value);
}

/***************************************************************************//**
 * @brief Looks up the SD Card Serial Number
 *
 * @param SerNo (str) Serial Number to be passed as a string
 *
 * @return void
*******************************************************************************/

void GetSerNo(char SerNo[256])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /sys/block/mmcblk0/device/serial", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(SerNo, 20, fp) != NULL)
  {
    printf("%s", SerNo);
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the Audio Input Devices
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetDevices(char DeviceName1[256], char DeviceName2[256])
{
  FILE *fp;
  char arecord_response_line[256];
  int card = 1;

  /* Open the command for reading. */
  fp = popen("arecord -l", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(arecord_response_line, 250, fp) != NULL)
  {
    if (arecord_response_line[0] == 'c')
    {
      if (card == 2)
      {
        strcpy(DeviceName2, arecord_response_line);
        card = card + 1;
      }
      if (card == 1)
      {
        strcpy(DeviceName1, arecord_response_line);
        card = card + 1;
      }
    }
    printf("%s", arecord_response_line);
  }
  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Looks up the USB Video Input Device address
 *
 * @param DeviceName1 and DeviceName2 (str) First 40 char of device names
 *
 * @return void
*******************************************************************************/

void GetUSBVidDev(char VidDevName[256])
{
  FILE *fp;
  char response_line[255];
  char WebcamName[255] = "none";

  // Read the Webcam address if it is present

  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/Webcam/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      strcpy(WebcamName, response_line);
    }
  }
  pclose(fp);

  if (strcmp(WebcamName, "none") == 0)  // not detected from "Webcam", so try "046d:0825" for C270
  {
    fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/046d:0825/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
       {
        strcpy(WebcamName, response_line);
      }
    }
    pclose(fp);
  }

  if (strcmp(WebcamName, "none") == 0)  // not detected from "Webcam", so try "046d:0821" for C910
  {
    fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/046d:0821/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
    if (fp == NULL)
    {
      printf("Failed to run command\n" );
      exit(1);
    }

    /* Read the output a line at a time - output it. */
    while (fgets(response_line, 250, fp) != NULL)
    {
      if (strlen(response_line) > 1)
       {
        strcpy(WebcamName, response_line);
      }
    }
    pclose(fp);
  }

  printf("Webcam device name is %s\n", WebcamName);

  // Now look for USB devices, but reject any lines that match the Webcam address
  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) <= 1)
    {
        strcpy(VidDevName, "nil");
    }
    else
    {
      // printf("This time response was %s\n", response_line);
      
      if (strcmp(WebcamName, response_line) != 0) // If they don't match
      {
        strcpy(VidDevName, response_line);
      }
    }
  }

  printf("Video capture device name is %s\n", VidDevName);

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Detects if a Logitech C 910, C525 or C270 webcam was connected since last restart
 *
 * @param None
 *
 * @return 0 = webcam not detected
 *         1 = webcam detected
 *         2 = shell returned unexpected exit status
*******************************************************************************/
 
int DetectLogitechWebcam()
{
  char shell_command[255];
  // Pattern for C270, C525 and C910
  char DMESG_PATTERN[255] = "046d:0825|Webcam C525|046d:0821";
  FILE * shell;
  sprintf(shell_command, "dmesg | grep -E -q \"%s\"", DMESG_PATTERN);
  shell = popen(shell_command, "r");
  int r = pclose(shell);
  if (WEXITSTATUS(r) == 0)
  {
    //printf("Logitech: webcam detected\n");
    return 1;
  }
  else if (WEXITSTATUS(r) == 1)
  {
    //printf("Logitech: webcam not detected\n");
    return 0;
  } 
  else 
  {
    //printf("Logitech: unexpected exit status %d\n", WEXITSTATUS(r));
    return 2;
  }
}


/***************************************************************************//**
 * @brief Detects if a Logitech C920 webcam is currently connected
 *
 * @param nil
 *
 * @return 1 if connected, 0 if not connected
*******************************************************************************/

int CheckC920()
{
  FILE *fp;
  char response_line[255];

  // Read the Webcam address if it is present

  fp = popen("v4l2-ctl --list-devices 2> /dev/null | sed -n '/C920/,/dev/p' | grep 'dev' | tr -d '\t'", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response_line, 250, fp) != NULL)
  {
    if (strlen(response_line) > 1)
    {
      return 1;
    }

  }
  pclose(fp);

  return 0;
}


/***************************************************************************//**
 * @brief Reads the Presets from portsdown_config.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadPresets()
{
  int n;
  char Param[255];
  char Value[255];
  char SRTag[9][255]={"psr1","psr2","psr3","psr4","psr5","psr6","psr7","psr8","psr9"};
  char FreqTag[9][255]={"pfreq1","pfreq2","pfreq3","pfreq4","pfreq5","pfreq6","pfreq7","pfreq8","pfreq9"};
  char SRValue[9][255];
  int len;

  // Read SRs
  for(n = 0; n < 9; n = n + 1)
  {
    strcpy(Param, SRTag[ n ]);
    GetConfigParam(PATH_PPRESETS,Param,Value);
    strcpy(SRValue[ n ], Value);
    TabSR[n] = atoi(SRValue[n]);
    if (TabSR[n] > 999)
    {
      strcpy(SRLabel[n], "SR");
      strcat(SRLabel[n], SRValue[n]);
    }
    else
    {
      strcpy(SRLabel[n], "SR ");
      strcat(SRLabel[n], SRValue[n]);
    }
  }

  // Read Frequencies
  for(n = 0; n < 9; n = n + 1)
  {
    strcpy(Param, FreqTag[ n ]);
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabFreq[ n ], Value);
    len  = strlen(TabFreq[n]);
    switch (len)
    {
      case 2:
        strcpy(FreqLabel[n], " ");
        strcat(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 3:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz ");
        break;
      case 4:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " MHz");
        break;
      case 5:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], "MHz");
        break;
      default:
        strcpy(FreqLabel[n], TabFreq[n]);
        strcat(FreqLabel[n], " M");
        break;
    }
  }

  // Read Preset Config button labels
  for(n = 0; n < 4; n = n + 1)
  {
    strcpy(Value, "");
    snprintf(Param, 10, "p%dlabel", n + 1);
    GetConfigParam(PATH_PPRESETS, Param, Value);
    strcpy(TabPresetLabel[n], Value);
  }
}

/***************************************************************************//**
 * @brief Checks for the presence of an RTL-SDR
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckRTL()
{
  char RTLStatus[256];
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_rtl.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  /* Read the output a line at a time - output it. */
  while (fgets(RTLStatus, sizeof(RTLStatus)-1, fp) != NULL)
  {
  }
  if (RTLStatus[0] == '0')
  {
    printf("RTL Detected\n" );
    return(0);
  }
  else
  {
    printf("No RTL Detected\n" );
    return(1);
  }

  /* close */
  pclose(fp);
}

void InitialiseGPIO()
{
  // Called on startup to initialise the GPIO so that it works correctly
  // for spi commands when first used (it didn't always before)
  pinMode(GPIO_PTT, OUTPUT);
  digitalWrite(GPIO_PTT, LOW);
  pinMode(GPIO_SPI_CLK, OUTPUT);
  digitalWrite(GPIO_SPI_CLK, LOW);
  pinMode(GPIO_SPI_DATA, OUTPUT);
  digitalWrite(GPIO_SPI_DATA, LOW);
  pinMode(GPIO_4351_LE, OUTPUT);
  digitalWrite(GPIO_4351_LE, HIGH);
  pinMode(GPIO_Atten_LE, OUTPUT);
  digitalWrite(GPIO_Atten_LE, HIGH);
  pinMode(GPIO_5355_LE, OUTPUT);
  digitalWrite(GPIO_5355_LE, HIGH);
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

void touchcal()
{
  VGfloat cross_x;                 // Position of white cross
  VGfloat cross_y;                 // Position of white cross
  int n;                           // Loop counter
  int rawX, rawY, rawPressure;     // Raw touch position
  int lowerY = 0, higherY = 0;     // Screen referenced touch average
  int leftX = 0, rightX = 0;       // Screen referenced touch average
  int touchposX[8];                // Screen referenced uncorrected touch position
  int touchposY[8];                // Screen referenced uncorrected touch position
  int correctedX;                  // Screen referenced corrected touch position
  int correctedY;                  // Screen referenced corrected touch position
  char Param[255];                 // Parameter name for writing to Calibration File
  char Value[255];                 // Value for writing to Calibration File

  MsgBox4("TOUCHSCREEN CALIBRATION", "Touch the screen on each cross"
    , "Screen will be recalibrated after 8 touches", "Touch screen to start");
  wait_touch();

  for (n = 1; n < 9; n = n + 1 )
  {
    BackgroundRGB(0,0,0,255);
    Fill(255, 255, 255, 1); 
    WindowClear();
    StrokeWidth(3);
    Stroke(255, 255, 255, 0.8);    // White lines

    // Draw Frame of 3 points deep around screen inner
    Line(0,         hscreen, wscreen-1, hscreen);
    Line(0,         1,       wscreen-1, 1      );
    Line(0,         1,       0,         hscreen);
    Line(wscreen-1, 1,       wscreen-1, hscreen);

    // Calculate cross centres for 10% and 90% crosses
    if ((n == 1) || (n ==5) || (n == 4) || (n == 8))
    {
      cross_y = (hscreen * 9) / 10;
    }
    else
    {
      cross_y = hscreen / 10;
    }
    if ((n == 3) || (n ==4) || (n == 7) || (n == 8))
    {
      cross_x = (wscreen * 9) / 10;
    }
    else
    {
      cross_x = wscreen / 10;
    }

    // Draw cross
    Line(cross_x-wscreen/20, cross_y, cross_x+wscreen/20, cross_y);
    Line(cross_x, cross_y-hscreen/20, cross_x, cross_y+hscreen/20);

    //Send to Screen
    End();

    // Wait here until screen touched
    while(getTouchSample(&rawX, &rawY, &rawPressure)==0)
    {
      usleep(100000);
    }

    // Transform touch to display coordinate system
    // Results returned in scaledX and scaledY (globals)
    TransformTouchMap(rawX, rawY);

    printf("x=%d y=%d scaledX=%d scaledY=%d\n ",rawX,rawY,scaledX,scaledY);

    if ((n == 1) || (n ==5) || (n == 4) || (n == 8))
    {
      if (scaledY < hscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      higherY = higherY + scaledY;
    }
    else
    {
     if (scaledY > hscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      lowerY = lowerY + scaledY;
    }
    if ((n == 3) || (n ==4) || (n == 7) || (n == 8))
    {
     if (scaledX < wscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      rightX = rightX + scaledX;
    }
    else
    {
     if (scaledX > wscreen/2)  // gross error
      {
        StrokeWidth(0);
        MsgBox4("Last touch was too far", "from the cross", "Touch Screen to continue", "and then try again");
        wait_touch();
        return;
      }
      leftX = leftX + scaledX;
    }

    // Save touch position for display
    touchposX[n] = scaledX;
    touchposY[n] = scaledY;
  }
    
  // Average out touches
  higherY = higherY/4; // higherY is the height of the upper horixontal line
  lowerY = lowerY/4;   // lowerY is the height of the lower horizontal line
  rightX = rightX/4;   // rightX is the left-right pos of the right hand vertical line
  leftX = leftX/4;     // leftX is the left-right pos of the left hand vertical line

  // Now calculate the global calibration factors
  CalFactorX = (0.8 * wscreen)/(rightX-leftX);
  CalFactorY = (0.8 * hscreen)/(higherY-lowerY);
  CalShiftX= wscreen/10 - leftX * CalFactorX;
  CalShiftY= hscreen/10 - lowerY * CalFactorY;

  // Save them to the calibration file

  // Save CalFactors
  snprintf(Value, 10, "%.4f", CalFactorX);
  strcpy(Param, "CalFactorX");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);
  snprintf(Value, 10, "%.4f", CalFactorY);
  strcpy(Param, "CalFactorY");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);

  // Save CalShifts
  snprintf(Value, 10, "%.1f", CalShiftX);
  strcpy(Param, "CalShiftX");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);
  snprintf(Value, 10, "%.1f", CalShiftY);
  strcpy(Param, "CalShiftY");
  SetConfigParam(PATH_TOUCHCAL,Param,Value);

  //printf("Left = %d, right = %d \n", leftX, rightX);
  //printf("Lower  = %d, upper = %d \n", lowerY, higherY);
  //printf("CalFactorX = %lf \n", CalFactorX);
  //printf("CalFactorY = %lf \n", CalFactorY);
  //printf("CalShiftX = %lf \n", CalShiftX);
  //printf("CalShiftY = %lf \n", CalShiftY);

  // Draw crosses in red where the touches were
  // and in green where the corrected touches are
  
  BackgroundRGB(0,0,0,255);
  Fill(255, 255, 255, 1); 
  WindowClear();
  StrokeWidth(3);
  Stroke(255, 255, 255, 0.8);

  VGfloat th = TextHeight(SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2+th, "Red crosses are before calibration,", SansTypeface, 25);
  TextMid(wscreen/2, hscreen/2-th, "Green crosses after calibration", SansTypeface, 25);
  TextMid(wscreen/2, hscreen/24, "Touch Screen to Continue", SansTypeface, 25);

  // Draw Frame
  Line(0,         hscreen, wscreen-1, hscreen);
  Line(0,         1,       wscreen-1, 1      );
  Line(0,         1,       0,         hscreen);
  Line(wscreen-1, 1,       wscreen-1, hscreen);

  // Draw the crosses
  for (n = 1; n < 9; n = n + 1 )
  {
    // Calculate cross centres
    if ((n == 1) || (n == 4) || (n == 5) || (n == 8))
    {
      cross_y = (hscreen * 9) / 10;
    }
    else
    {
      cross_y = hscreen / 10;
    }
    if ((n == 3) || (n == 4) || (n == 7) || (n == 8))
    {
      cross_x = (wscreen * 9) / 10;
    }
    else
    {
      cross_x = wscreen / 10;
    }

    // Draw reference cross in white
    Stroke(255, 255, 255, 0.8);
    Line(cross_x-wscreen/20, cross_y, cross_x+wscreen/20, cross_y);
    Line(cross_x, cross_y-hscreen/20, cross_x, cross_y+hscreen/20);

    // Draw uncorrected touch cross in red
    Stroke(255, 0, 0, 0.8);
    Line(touchposX[n]-wscreen/40, touchposY[n]-hscreen/40, touchposX[n]+wscreen/40, touchposY[n]+hscreen/40);
    Line(touchposX[n]+wscreen/40, touchposY[n]-hscreen/40, touchposX[n]-wscreen/40, touchposY[n]+hscreen/40);

    // Draw corrected touch cross in green
    correctedX = touchposX[n] * CalFactorX;
    correctedX = correctedX + CalShiftX;
    correctedY = touchposY[n] * CalFactorY;
    correctedY = correctedY + CalShiftY;
    Stroke(0, 255, 0, 0.8);
    Line(correctedX-wscreen/40, correctedY-hscreen/40, correctedX+wscreen/40, correctedY+hscreen/40);
    Line(correctedX+wscreen/40, correctedY-hscreen/40, correctedX-wscreen/40, correctedY+hscreen/40);
  }

  //Send to Screen
  End();
  wait_touch();

  // Set screen back to normal
  BackgroundRGB(0,0,0,255);
  StrokeWidth(0);
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
  //int  scaledX, scaledY;

  TransformTouchMap(x,y);  // Sorts out orientation and approx scaling of the touch map

  CorrectTouchMap();       // Calibrates each individual screen

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  int margin=10;  // was 20

  if((scaledX<=(ButtonArray[NbButton].x+ButtonArray[NbButton].w-margin))&&(scaledX>=ButtonArray[NbButton].x+margin) &&
    (scaledY<=(ButtonArray[NbButton].y+ButtonArray[NbButton].h-margin))&&(scaledY>=ButtonArray[NbButton].y+margin))
  {
    // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
    return 1;
  }
  else
  {
    return 0;
  }
}

int IsMenuButtonPushed(int x,int y)
{
  int  i, NbButton, cmo, cmsize;
  NbButton = -1;
  cmo = ButtonNumber(CurrentMenu, 0); // Current Menu Button number Offset
  cmsize = ButtonNumber(CurrentMenu + 1, 0) - ButtonNumber(CurrentMenu, 0);
  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map
  CorrectTouchMap();            // Calibrates each individual screen

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  int margin=10;  // was 20

  // For each button in the current Menu, check if it has been pushed.
  // If it has been pushed, return the button number.  If nothing valid has been pushed return -1
  // If it has been pushed, do something with the last event time

  for (i = 0; i <cmsize; i++)
  {
    if (ButtonArray[i + cmo].IndexStatus > 0)  // If button has been defined
    {
      if  ((scaledX <= (ButtonArray[i + cmo].x + ButtonArray[i + cmo].w - margin))
        && (scaledX >= ButtonArray[i + cmo].x + margin)
        && (scaledY <= (ButtonArray[i + cmo].y + ButtonArray[i + cmo].h - margin))
        && (scaledY >= ButtonArray[i + cmo].y + margin))  // and touched
      {
        // ButtonArray[NbButton].LastEventTime=mymillis(); No longer used
        NbButton = i;          // Set the button number to return
        break;                 // Break out of loop as button has been found
      }
    }
  }
  return NbButton;
}

int InitialiseButtons()
{
  // Writes 0 to IndexStatus of each button to signify that it should not
  // be displayed.  As soon as a status (text and color) is added, IndexStatus > 0
  int i;
  for (i = 0; i <= MAX_BUTTON; i = i + 1)
  {
    ButtonArray[i].IndexStatus = 0;
  }
  return 1;
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
	// NewButton->LastEventTime=mymillis();  No longer used
	return IndexButtonInArray++;
}

int ButtonNumber(int MenuIndex, int Button)
{
  // Returns the Button Number (0 - 599) from the Menu number and the button position
  int ButtonNumb = 0;

  if (MenuIndex <= 10)
  {
    ButtonNumb = (MenuIndex - 1) * 25 + Button;
  }
  if ((MenuIndex >= 11) && (MenuIndex <= 40))
  {
    ButtonNumb = 250 + (MenuIndex - 11) * 10 + Button;
  }
  if ((MenuIndex >= 41) && (MenuIndex <= 41))
  {
    ButtonNumb = 550 + (MenuIndex - 41) * 50 + Button;
  }
  if (MenuIndex > 41)
  {
    ButtonNumb = 600;
  }
  return ButtonNumb;
}

int CreateButton(int MenuIndex, int ButtonPosition)
{
  // Provide Menu number (int 1 - 41), Button Position (0 bottom left, 23 top right)
  // return button number

  // Menus 1 - 10 are classic 25-button menus
  // Menus 11 - 40 are 10-button menus
  // Menu 41 is a keyboard

  int ButtonIndex;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  ButtonIndex = ButtonNumber(MenuIndex, ButtonPosition);

  if (MenuIndex != 41)
  {
    if (ButtonPosition < 20)  // Bottom 4 rows
    {
      x = (ButtonPosition % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 0.9;
    }
    else if ((ButtonPosition == 20) || (ButtonPosition == 21))  // TX and RX buttons
    {
      x = (ButtonPosition % 5) * wbuttonsize *1.7 + 20;    // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 1.2;
      h = hbuttonsize * 1.2;
    }
    else if ((ButtonPosition == 22) || (ButtonPosition == 23)) //Menu Up and Menu down buttons
    {
      x = ((ButtonPosition + 1) % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 1.2;
    }
  }
  else  // Keyboard
  {
    w = wscreen/12;
    h = hscreen/8;

    if (ButtonPosition <= 9)  // Space bar and < > - Enter, Bkspc
    {
      switch (ButtonPosition)
      {
      case 0:                   // Space Bar
          y = 0;
          x = wscreen * 5 / 24;
          w = wscreen * 11 /24;
          break;
      case 1:                  // Not used
          y = 0;
          x = wscreen * 8 / 12;
          break;
      case 2:                  // <
          y = 0;
          x = wscreen * 9 / 12;
          break;
      case 3:                  // >
          y = 0;
          x = wscreen * 10 / 12;
          break;
      case 4:                  // -
          y = 0;
          x = wscreen * 11 / 12;
          break;
      case 5:                  // Not used
          y = 0;
          x = wscreen * 8 / 12;
          break;
      case 6:                 // Left Shift
          y = hscreen/8;
          x = 0;
          break;
      case 7:                 // Right Shift
          y = hscreen/8;
          x = wscreen * 11 / 12;
          break;
      case 8:                 // Enter
          y = 2 * hscreen/8;
          x = wscreen * 10 / 12;
          w = 2 * wscreen/12;
          h = 2 * hscreen/8;
          break;
      case 9:
          y = 4 * hscreen / 8;
          x = wscreen * 21 / 24;
          w = 3 * wscreen/24;
          break;
      }
    }
    if ((ButtonPosition >= 10) && (ButtonPosition <= 19))  // ZXCVBNM,./
    {
      y = hscreen/8;
      x = (ButtonPosition - 9) * wscreen/12;
    }
    if ((ButtonPosition >= 20) && (ButtonPosition <= 29))  // ASDFGHJKL
    {
      y = 2 * hscreen / 8;
      x = (ButtonPosition - 19.5) * wscreen/12;
    }
    if ((ButtonPosition >= 30) && (ButtonPosition <= 39))  // QWERTYUIOP
    {
      y = 3 * hscreen / 8;
      x = (ButtonPosition - 30) * wscreen/12;
    }
    if ((ButtonPosition >= 40) && (ButtonPosition <= 49))  // 1234567890
    {
      y = 4 * hscreen / 8;
      x = (ButtonPosition - 39.5) * wscreen/12;
    }
  }

  button_t *NewButton=&(ButtonArray[ButtonIndex]);
  NewButton->x=x;
  NewButton->y=y;
  NewButton->w=w;
  NewButton->h=h;
  NewButton->NoStatus=0;
  NewButton->IndexStatus=0;

  return (ButtonIndex);
}

int AddButtonStatus(int ButtonIndex,char *Text,color_t *Color)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  strcpy(Button->Status[Button->IndexStatus].Text,Text);
  Button->Status[Button->IndexStatus].Color=*Color;
  return Button->IndexStatus++;
}

void AmendButtonStatus(int ButtonIndex, int ButtonStatusIndex, char *Text, color_t *Color)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  strcpy(Button->Status[ButtonStatusIndex].Text, Text);
  Button->Status[ButtonStatusIndex].Color=*Color;
}

void DrawButton(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);

  if (CurrentMenu == 41)
  {
    StrokeWidth(2);
    Stroke(150, 150, 200, 0.8);
  }
  else
  {
    StrokeWidth(0);
  }

  Fill(Button->Status[Button->NoStatus].Color.r, Button->Status[Button->NoStatus].Color.g, Button->Status[Button->NoStatus].Color.b, 1);
  Roundrect(Button->x,Button->y,Button->w,Button->h, Button->w/10, Button->w/10);
  Fill(255, 255, 255, 1);				   // White text

  char label[256];
  strcpy(label, Button->Status[Button->NoStatus].Text);
  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(label, find);            // pointer to ^ in string
  if((ptr) && (CurrentMenu != 41))                                           // if ^ found then 2 lines
  {
    int index = ptr - label;                        // Position of ^ in string
    char line1[15];
    char line2[15];
    snprintf(line1, index+1, label);                // get text before ^
    snprintf(line2, strlen(label) - index, label + index + 1);  // and after ^
    TextMid(Button->x+Button->w/2, Button->y+Button->h*11/16, line1, SansTypeface, 18);	
    TextMid(Button->x+Button->w/2, Button->y+Button->h* 3/16, line2, SansTypeface, 18);
  
    // Draw overlay button.  Menu 1, 2 lines and Button status = 0 only
    if ((CurrentMenu == 1) && (Button->NoStatus == 0))
    {
      Fill(Button->Status[1].Color.r, Button->Status[1].Color.g, Button->Status[1].Color.b, 1);
      Roundrect(Button->x,Button->y,Button->w,Button->h/2, Button->w/10, Button->w/10);
      Fill(255, 255, 255, 1);				   // White text
      TextMid(Button->x+Button->w/2, Button->y+Button->h* 3/16, line2, SansTypeface, 18);
    }
  }
  else                                              // One line only
  {
    if (CurrentMenu <= 10)
    {
      TextMid(Button->x+Button->w/2, Button->y+Button->h/2, label, SansTypeface, Button->w/strlen(label));
    }
    else if (CurrentMenu == 41)
    {
       TextMid(Button->x+Button->w/2, Button->y+Button->h/2 - hscreen / 64, label, SansTypeface, 24);
    }
    else // fix text size at 18
    {
      TextMid(Button->x+Button->w/2, Button->y+Button->h/2, label, SansTypeface, 18);
    }
  }
}

void SetButtonStatus(int ButtonIndex,int Status)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  Button->NoStatus=Status;
}

int GetButtonStatus(int ButtonIndex)
{
  button_t *Button=&(ButtonArray[ButtonIndex]);
  return Button->NoStatus;
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
    return 0;
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
	//unsigned short id[4];
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
                if (test_bit(i, bit[0])) {
                        printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
                        if (!i) continue;
                        ioctl(fd, EVIOCGBIT(i, KEY_MAX), bit[i]);
                        for (j = 0; j < KEY_MAX; j++){
                                if (test_bit(j, bit[i])) {
                                        printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
	if(j==330) IsAtouchDevice=1;
                                        if (i == EV_ABS) {
                                                ioctl(fd, EVIOCGABS(j), abs);
                                                for (k = 0; k < 5; k++)
                                                        if ((k < 3) || abs[k]){
                                                                printf("     %s %6d\n", absval[k], abs[k]);
                                                                if (j == 0){
                                                                        if ((strcmp(absval[k],"Min  ")==0)) *screenXmin =  abs[k];
                                                                        if ((strcmp(absval[k],"Max  ")==0)) *screenXmax =  abs[k];
                                                                }
                                                                if (j == 1){
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

void ShowMenuText()
{
  // Initialise and calculate the text display
  int line;
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  Fill(255, 255, 255, 1);    // White text

  // Display Text
  for (line = 0; line < 5; line = line + 1)
  {
    //tw = TextWidth(MenuText[line], font, pointsize);
    Text(wscreen / 12, hscreen - (4 + line) * linepitch, MenuText[line], font, pointsize);
  }
}

void ShowTitle()
{
  // Initialise and calculate the text display
  //BackgroundRGB(0,0,0,255);  // Black background
  if (CurrentMenu == 1)
  {
    Fill(0, 0, 0, 1);    // Black text
  }
  else
  {
    Fill(255, 255, 255, 1);    // White text
  }
  Fontinfo font = SansTypeface;
  int pointsize = 20;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.1 * (txtht + txtdp);
  VGfloat linenumber = 1.0;
  VGfloat tw;

  // Display Text
  tw = TextWidth(MenuTitle[CurrentMenu], font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, MenuTitle[CurrentMenu], font, pointsize);
}

void UpdateWindow()
// Paint each defined button and the title on the current Menu
{
  //int ButtonNumber(int MenuIndex, int Button)
  int i;
  int first;
  int last;

  first = ButtonNumber(CurrentMenu, 0);
  last = ButtonNumber(CurrentMenu + 1 , 0) - 1;

  if ((CurrentMenu >= 11) && (CurrentMenu <= 40))  // 10-button menus
  {
    Fill(127, 127, 127, 1);
    Roundrect(10, 10, wscreen-18, hscreen*2/6+10, 10, 10);
  }

  for(i=first; i<=last; i++)
  {
    // printf("Looking at button %d\n", i);
    if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drwan
    {
      DrawButton(i);                     // Draw the button
    }
  }
  ShowTitle();
  if (CurrentMenu == 33)
  {
    ShowMenuText();
  }

  End();                      // Write the drawn buttons to the screen
}

void ApplyTXConfig()
{
  // Called after any top row button changes to work out config required for a.sh
  // Based on decision tree
  if (strcmp(CurrentTXMode, "Carrier") == 0)
  {
    strcpy(ModeInput, "CARRIER");
  }
  else
  {
    if (strcmp(CurrentEncoding, "IPTS in") == 0)
    {
      strcpy(ModeInput, "IPTSIN");
    }
    else if (strcmp(CurrentEncoding, "TS File") == 0)
    {
      strcpy(ModeInput, "FILETS");
    }
    else if (strcmp(CurrentEncoding, "H264") == 0)
    {
      if (strcmp(CurrentFormat, "1080p") == 0)
      {
        if (CheckC920() == 1)
        {
          if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "C920FHDH264");
          }
          else
          {
            MsgBox2("1080p only available with C920 Webcam"
              , "Switching to C920");
            wait_touch();
            strcpy(ModeInput, "C920FHDH264");
          }
        }
        else
        {
        MsgBox2("1080p only available with C920 Webcam"
          , "Please select another mode");
        wait_touch();
        }
      }
      if (strcmp(CurrentFormat, "720p") == 0)
      {
        if (CheckC920() == 1)
        {
          if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "C920HDH264");
          }
          else
          {
            MsgBox2("720p H264 only available with C920 Webcam"
              , "Switching to C920");
            wait_touch();
           strcpy(ModeInput, "C920HDH264");
          }
        }
        else
        {
        MsgBox2("720p H264 only available with C920 Webcam"
          , "Selecting Pi Cam 720p MPEG-2");
          strcpy(ModeInput, "CAMHDMPEG-2");
        wait_touch();
        }
      }
      if (strcmp(CurrentFormat, "16:9") == 0)
      {
        if (CheckC920() == 1)
        {
          if (strcmp(CurrentSource, "C920") == 0)
          {
            MsgBox2("16:9 not available with C920"
              , "Selecting C920 720p");
            strcpy(ModeInput, "C920HDH264");
            wait_touch();
          }
          else
          {
            MsgBox2("16:9 only available with MPEG-2"
              , "Selecting Pi Cam 16:9 MPEG-2");
            strcpy(ModeInput, "CAM16MPEG-2");
            wait_touch();
          }
        }
      }
      if (strcmp(CurrentFormat, "4:3") == 0)
      {
        if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAMH264");
        }
        else if (strcmp(CurrentSource, "CompVid") == 0)
        {
          strcpy(ModeInput, "ANALOGCAM");
        }
        else if (strcmp(CurrentSource, "TCAnim") == 0)
        {
          strcpy(ModeInput, "PATERNAUDIO");
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARDH264");
        }
        else if (strcmp(CurrentSource, "PiScreen") == 0)
        {
          strcpy(ModeInput, "DESKTOP");
        }
        else if (strcmp(CurrentSource, "Contest") == 0)
        {
          strcpy(ModeInput, "CONTEST");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAMH264");
        }
        else if (strcmp(CurrentSource, "C920") == 0)
        {
          strcpy(ModeInput, "C920H264");
        }
        else // Shouldn't happen
        {
          strcpy(ModeInput, "DESKTOP");
        }
      }
    }
    else  // MPEG-2
    {
      if (strcmp(CurrentFormat, "1080p") == 0)
      {
        if (CheckC920() == 1)
        {
          if (strcmp(CurrentSource, "C920") == 0)
          {
            strcpy(ModeInput, "C920FHDH264");
          }
          else
          {
            MsgBox2("1080p only available with H264"
              , "Switching to H264");
            wait_touch();
            strcpy(ModeInput, "C920FHDH264");
          }
        }
        else
        {
        MsgBox2("1080p only available with C920 Webcam"
          , "Please select another mode");
        wait_touch();
        }
      }
      if (strcmp(CurrentFormat, "720p") == 0)
      {
        if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAMHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "CompVid") == 0)
        {
          MsgBox2("720p not available with Comp Vid", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "TCAnim") == 0)
        {
          MsgBox2("720p not available with TCAnim", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "PiScreen") == 0)
        {
          MsgBox2("720p not available with PiScreen", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "Contest") == 0)
        {
          MsgBox2("720p not available with Contest", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAMHDMPEG-2");
        }
        else if (strcmp(CurrentSource, "C920") == 0)
        {
          strcpy(ModeInput, "WEBCAMHDMPEG-2");
        }
        else  // shouldn't happen
        {
          strcpy(ModeInput, "CARDHDMPEG-2");
        }
      }

      if (strcmp(CurrentFormat, "16:9") == 0)
      {
        if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAM16MPEG-2");
        }
        else if (strcmp(CurrentSource, "CompVid") == 0)
        {
          strcpy(ModeInput, "ANALOG16MPEG-2");
        }
        else if (strcmp(CurrentSource, "TCAnim") == 0)
        {
          MsgBox2("TCAnim not available with MPEG-2", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARD16MPEG-2");
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARD16MPEG-2");
        }
        else if (strcmp(CurrentSource, "PiScreen") == 0)
        {
          MsgBox2("16:9 not available with PiScreen", "Selecting the test card");
          wait_touch();
          strcpy(ModeInput, "CARD16MPEG-2");
        }
        else if (strcmp(CurrentSource, "Contest") == 0)
        {
          MsgBox2("16:9 not available with Contest", "Selecting 4:3");
          wait_touch();
          strcpy(ModeInput, "CONTESTMPEG-2");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAM16MPEG-2");
        }
        else if (strcmp(CurrentSource, "C920") == 0)
        {
          strcpy(ModeInput, "WEBCAM16MPEG-2");
        }
        else  // shouldn't happen
        {
          strcpy(ModeInput, "CARD16MPEG-2");
        }
      }

      if (strcmp(CurrentFormat, "4:3") == 0)
      {
        if (strcmp(CurrentSource, "Pi Cam") == 0)
        {
          strcpy(ModeInput, "CAMMPEG-2");
        }
        else if (strcmp(CurrentSource, "CompVid") == 0)
        {
          strcpy(ModeInput, "ANALOGMPEG-2");
        }
        else if (strcmp(CurrentSource, "TCAnim") == 0)
        {
          strcpy(ModeInput, "CARDMPEG-2");
          MsgBox2("TCAnim not available with MPEG-2", "Selecting Test Card F instead");
          wait_touch();
        }
        else if (strcmp(CurrentSource, "TestCard") == 0)
        {
          strcpy(ModeInput, "CARDMPEG-2");
        }
        else if (strcmp(CurrentSource, "PiScreen") == 0)
        {
          strcpy(ModeInput, "CARDMPEG-2");
          MsgBox2("PiScreen not available with MPEG-2", "Selecting Test Card F instead");
          wait_touch();
        }
        else if (strcmp(CurrentSource, "Contest") == 0)
        {
          strcpy(ModeInput, "CONTESTMPEG-2");
        }
        else if (strcmp(CurrentSource, "Webcam") == 0)
        {
          strcpy(ModeInput, "WEBCAMMPEG-2");
        }
        else if (strcmp(CurrentSource, "C920") == 0)
        {
          strcpy(ModeInput, "WEBCAMMPEG-2");
        }
        else  // Shouldn't happen but give them Test Card F
        {
          strcpy(ModeInput, "CARDMPEG-2");
        }
      }
    }
  }
  // Now save the change and make sure that all the config is correct
  char Param[15] = "modeinput";
  SetConfigParam(PATH_PCONFIG, Param, ModeInput);
  printf("a.sh will be called with %s\n", ModeInput);

  // Load the Pi Cam driver for CAMMPEG-2 modes
  if ((strcmp(ModeInput,"CAMMPEG-2")==0)
    ||(strcmp(ModeInput,"CAM16MPEG-2")==0)
    ||(strcmp(ModeInput,"CAMHDMPEG-2")==0))
  {
    system("sudo modprobe bcm2835_v4l2");
  }
  // Replace Contest Numbers or Test Card with BATC Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void GreyOut1()
{
  // Called at the top of any StartHighlight1 to grey out inappropriate selections
  if (CurrentMenu == 1)
  {
    if (strcmp(CurrentTXMode, "Carrier") == 0)
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 16), 2); // Encoder
      SetButtonStatus(ButtonNumber(CurrentMenu, 18), 2); // Format
      SetButtonStatus(ButtonNumber(CurrentMenu, 19), 2); // Source
      SetButtonStatus(ButtonNumber(CurrentMenu, 11), 2); // SR
      SetButtonStatus(ButtonNumber(CurrentMenu, 12), 2); // FEC
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2);  // Audio
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2);  // Caption
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2);  // EasyCap
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 16), 0); // Encoder
      SetButtonStatus(ButtonNumber(CurrentMenu, 11), 0); // SR
      SetButtonStatus(ButtonNumber(CurrentMenu, 12), 0); // FEC
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Caption
      SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // EasyCap

      if ((strcmp(CurrentEncoding, "IPTS in") == 0) || (strcmp(CurrentEncoding, "TS File") == 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 18), 2); // Format
        SetButtonStatus(ButtonNumber(CurrentMenu, 19), 2); // Source
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2);  // Audio
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // Caption
        SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // EasyCap
      }
      else
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 18), 0); // Format
        SetButtonStatus(ButtonNumber(CurrentMenu, 19), 0); // Source
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // Caption
        SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // EasyCap
      }
      if (strcmp(CurrentEncoding, "MPEG-2") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
      }
      else if (strcmp(CurrentSource, "C920") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // Audio
      }
      else  // H264 (but not C920) or IPTS in or TS File
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // Audio
      }
    }
    if ((strcmp(CurrentModeOP, "BATC") == 0) || (strcmp(CurrentModeOP, "STREAMER") == 0)\
      || (strcmp(CurrentModeOP, "COMPVID") == 0) || (strcmp(CurrentModeOP, "DTX1") == 0)\
      || (strcmp(CurrentModeOP, "IP") == 0))
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 10), 2); // Frequency
      SetButtonStatus(ButtonNumber(CurrentMenu, 13), 2); // Band
      SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2); // Attenuator Level
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2);  // Attenuator Type
    }
    else
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 10), 0); // Frequency
      SetButtonStatus(ButtonNumber(CurrentMenu, 13), 0); // Band
      SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0);  // Attenuator Type
      if ((strcmp(CurrentAtten, "NONE") == 0) && (strcmp(CurrentModeOP, "DATVEXPRESS") != 0))
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 14), 2); // Attenuator Level
      }
      else
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 14), 0); // Attenuator Level
      }
    }
  }
}

void GreyOut15()
{
  if (strcmp(CurrentFormat, "1080p") == 0)
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // Pi Cam
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // CompVid
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // TCAnim
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // TestCard
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // Webcam
  }
  else
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // Pi Cam
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // TCAnim
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // TestCard
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // PiScreen
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // Contest
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0); // Webcam

    if (strcmp(CurrentEncoding, "H264") == 0)
    {
      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // TCAnim
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // PiScreen
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid
    }
    else //MPEG-2
    {

      SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // TCAnim
      SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // PiScreen
      SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // CompVid

      if (strcmp(CurrentFormat, "720p") == 0)
      {
        SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // CompVid
      }
    }
  }
}

void SelectInGroup(int StartButton,int StopButton,int NoButton,int Status)
{
  int i;
  for(i = StartButton ; i <= StopButton ; i++)
  {
    if(i == NoButton)
      SetButtonStatus(i, Status);
    else
      SetButtonStatus(i, 0);
  }
}

void SelectInGroupOnMenu(int Menu, int StartButton, int StopButton, int NumberButton, int Status)
{
  int i;
  int StartButtonAll;
  int StopButtonAll;
  int NumberButtonAll;

  StartButtonAll = ButtonNumber(Menu, StartButton); 
  StopButtonAll = ButtonNumber(Menu, StopButton); 
  NumberButtonAll = ButtonNumber(Menu, NumberButton); 
  for(i = StartButtonAll ; i <= StopButtonAll ; i++)
  {
    if(i == NumberButtonAll)
    {
      SetButtonStatus(i, Status);
    }
    else
    {
      SetButtonStatus(i, 0);
    }
  }
}

void SelectTX(int NoButton)  // TX RF Output Mode
{
  SelectInGroupOnMenu(CurrentMenu, 5, 6, NoButton, 1);
  strcpy(CurrentTXMode, TabTXMode[NoButton - 5]);
  ApplyTXConfig();
}

void SelectEncoding(int NoButton)  // Encoding
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  strcpy(CurrentEncoding, TabEncoding[NoButton - 5]);
  ApplyTXConfig();
}

void SelectOP(int NoButton)      // Output device
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(ModeOP, TabModeOP[NoButton - 5]);
  printf("************** Set Output Mode = %s\n",ModeOP);
  char Param[]="modeoutput";
  SetConfigParam(PATH_PCONFIG, Param, ModeOP);

  // Set the Current Mode Output variable
  strcpy(CurrentModeOP, TabModeOP[NoButton - 5]);
  strcpy(CurrentModeOPtext, TabModeOPtext[NoButton - 5]);
}

void SelectFormat(int NoButton)  // Video Format
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  strcpy(CurrentFormat, TabFormat[NoButton - 5]);
  ApplyTXConfig();
}

void SelectSource(int NoButton)  // Video Source
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 2, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(CurrentSource, TabSource[NoButton - 5]);
  ApplyTXConfig();
}

void SelectFreq(int NoButton)  //Frequency
{
  char freqtxt[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4)
  {
    NoButton = NoButton + 10;
  }
  strcpy(freqtxt, TabFreq[NoButton - 5]);
  char Param[] = "freqoutput";
  printf("************** Set Frequency = %s\n",freqtxt);
  SetConfigParam(PATH_PCONFIG, Param, freqtxt);

  DoFreqChange();

  // **** Not required - now in DoFreqChange() ******
  // Set the Band (and filter) Switching
  // system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // And wait for it to finish using portsdown_config.txt
  // usleep(100000);
}

void SelectSR(int NoButton)  // Symbol Rate
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4)
  {
    NoButton = NoButton + 10;
  }
  SR = TabSR[NoButton - 5];
  char Param[] = "symbolrate";
  char Value[255];
  sprintf(Value, "%d", SR);
  printf("************** Set SR = %s\n",Value);
  SetConfigParam(PATH_PCONFIG, Param, Value);
}

void SelectFec(int NoButton)  // FEC
{
//  SelectInGroup(10, 14 ,NoButton ,1);
//  fec = TabFec[NoButton - 10];
//  SelectInGroup(10, 14 ,NoButton ,1);
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  fec = TabFec[NoButton - 5];
  char Param[]="fec";
  char Value[255];
  sprintf(Value, "%d", fec);
  printf("************** Set FEC = %s\n",Value);
  SetConfigParam(PATH_PCONFIG, Param, Value);
}

void SelectPTT(int NoButton,int Status)  // TX/RX
{
  SelectInGroup(20, 21, NoButton, Status);
}

void SelectCaption(int NoButton)  // Caption on or off
{
  char Param[10]="caption";
  if(NoButton == 5)
  {
    strcpy(CurrentCaptionState, "off");
    SetConfigParam(PATH_PCONFIG,Param,"off");
  }
  if(NoButton == 6)
  {
    strcpy(CurrentCaptionState, "on");
    SetConfigParam(PATH_PCONFIG,Param,"on");
  }
  SelectInGroupOnMenu(CurrentMenu, 5, 6, NoButton, 1);
  printf("************** Set Caption %s \n", CurrentCaptionState);
}

void SelectSTD(int NoButton)  // PAL or NTSC
{
  char USBVidDevice[255];
  char Param[255];
  char SetStandard[255];
  SelectInGroupOnMenu(CurrentMenu, 8, 9, NoButton, 1);
  strcpy(ModeSTD, TabModeSTD[NoButton - 8]);
  printf("************** Set Input Standard = %s\n", ModeSTD);
  strcpy(Param, "analogcamstandard");
  SetConfigParam(PATH_PCONFIG,Param,ModeSTD);

  // Now Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, ModeSTD);
    printf("Video Standard set with command %s\n", SetStandard);
    system(SetStandard);
  }
}

void ChangeBandDetails(int NoButton)
{
  char Param[31];
  char Value[31];
  char Prompt[63];
  float AttenLevel = 1;
  int ExpLevel = -1;
  int ExpPorts = -1;
  float LO = 1000001;
  char Numbers[10] ="";
  //char PromptBand[15];
  int band;

  // Convert button number to band number
  band = NoButton + 5;  // this is correct only for lower line of buttons
  if (NoButton > 4)     // Upper line of buttons
  {
    band = NoButton - 5;
  }

  // Label
  strcpy(Param, TabBand[band]);
  strcat(Param, "label");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  snprintf(Prompt, 63, "Enter the title for Band %d (no spaces!):", band + 1);
  Keyboard(Prompt, Value, 15);
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);
  strcpy(TabBandLabel[band], KeyboardReturn);

  // Attenuator Level
  strcpy(Param, TabBand[band]);
  strcat(Param, "attenlevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  while ((AttenLevel > 0) || (AttenLevel < -31.75))
  {
    snprintf(Prompt, 63, "Set the Attenuator Level for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 6);
    AttenLevel = atof(KeyboardReturn);
  }
  TabBandAttenLevel[band] = AttenLevel;
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Express Level
  strcpy(Param, TabBand[band]);
  strcat(Param, "explevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  while ((ExpLevel < 0) || (ExpLevel > 44))
  {
    snprintf(Prompt, 63, "Set the Express Level for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 2);
    ExpLevel = atoi(KeyboardReturn);
  }
  TabBandExpLevel[band] = ExpLevel;
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Express ports
  strcpy(Param, TabBand[band]);
  strcat(Param, "expports");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  while ((ExpPorts < 0) || (ExpPorts > 15))
  {
    snprintf(Prompt, 63, "Enter Express Port Settings for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 2);
    ExpPorts = atoi(KeyboardReturn);
  }
  TabBandExpPorts[band] = ExpPorts;
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // LO frequency
  strcpy(Param, TabBand[band]);
  strcat(Param, "lo");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  while (LO > 1000000)
  {
    snprintf(Prompt, 63, "Enter LO frequency in MHz for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 10);
    LO = atof(KeyboardReturn);
  }
  TabBandLO[band] = LO;
  SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);

  // Contest Numbers
  strcpy(Param, TabBand[band]);
  strcat(Param, "numbers");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  while (strlen(Numbers) < 1)
  {
    snprintf(Prompt, 63, "Enter Contest Numbers for the %s Band:", TabBandLabel[band]);
    Keyboard(Prompt, Value, 10);
    strcpy(Numbers, KeyboardReturn);
  }
  strcpy(TabBandNumbers[band], Numbers);
  SetConfigParam(PATH_PPRESETS ,Param, Numbers);

  // Then, if it is the current band, call DoFreqChange
  if (band == CurrentBand)
  {
    DoFreqChange();
  }
}

void DoFreqChange()
{
  // Called after any frequency or band change or band parameter change
  // to set the correct band levels and numbers in portsdown_config.txt
  // Transverter band must be correct on entry.
  // However, this changes direct bands based on the new frequency

  char Param[15];
  char Value[15];
  //char Freqtext[255];
  float CurrentFreq;

  // Look up the current band
  strcpy(Param,"band");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  if ((strcmp(Value, TabBand[0]) == 0) || (strcmp(Value, TabBand[1]) == 0)
   || (strcmp(Value, TabBand[2]) == 0) || (strcmp(Value, TabBand[3]) == 0)
   || (strcmp(Value, TabBand[4]) == 0))   // Not a transverter
  {
    // So look up the current frequency
    strcpy(Param,"freqoutput");
    GetConfigParam(PATH_PCONFIG, Param, Value);
    CurrentFreq = atof(Value);

    printf("DoFreqChange thinks freq is %f\n", CurrentFreq);

    if (CurrentFreq < 100)                            // 71 MHz
    {
       CurrentBand = 0;
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      CurrentBand = 1;
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      CurrentBand = 2;
    }
    if ((CurrentFreq >= 950) && (CurrentFreq <2000))  // 1255 MHz
    {
      CurrentBand = 3;
    }
    if (CurrentFreq >= 2000)                          // 2400 MHz
    {
      CurrentBand = 4;
    }
  }
  // CurrentBand now holds the in-use band 

  // Set the correct band in portsdown_config.txt
  strcpy(Param,"band");    
  SetConfigParam(PATH_PCONFIG, Param, TabBand[CurrentBand]);

  // Read each Band value in turn from PPresets
  // Then set Current variables and
  // write correct values to portsdown_config.txt

  // Band Label
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "label");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  strcpy(Param, "labelofband");
  SetConfigParam(PATH_PCONFIG, Param, Value);
  
  // Attenuator Level
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "attenlevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  TabBandAttenLevel[CurrentBand] = atof(Value);

  strcpy(Param, "attenlevel");
  SetConfigParam(PATH_PCONFIG ,Param, Value);
  
  // Express Level
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "explevel");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandExpLevel[CurrentBand] = atoi(Value);

  strcpy(Param, "explevel");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Express Ports
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "expports");
  GetConfigParam(PATH_PPRESETS, Param, Value);
 
  TabBandExpPorts[CurrentBand] = atoi(Value);

  strcpy(Param, "expports");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // LO frequency
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "lo");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  TabBandLO[CurrentBand] = atoi(Value);

  strcpy(Param, "lo");
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Contest Numbers
  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "numbers");
  GetConfigParam(PATH_PPRESETS, Param, Value);

  strcpy(TabBandNumbers[CurrentBand], Value);

  strcpy(Param, "numbers");
  SetConfigParam(PATH_PCONFIG ,Param, Value);

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // And wait for it to finish using portsdown_config.txt
  usleep(100000);
}

void SelectBand(int NoButton)  // Set the Band
{
  char Param[15];
  char Value[15];
  float CurrentFreq;
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  CurrentFreq = atof(Value);

  SelectInGroupOnMenu(CurrentMenu, 0, 9, NoButton, 1);

  // Translate Button number to band index
  if (NoButton == 5)
  {
    if (CurrentFreq < 100)                            // 71 MHz
    {
       CurrentBand = 0;
    }
    if ((CurrentFreq >= 100) && (CurrentFreq < 250))  // 146 MHz
    {
      CurrentBand = 1;
    }
    if ((CurrentFreq >= 250) && (CurrentFreq < 950))  // 437 MHz
    {
      CurrentBand = 2;
    }
    if ((CurrentFreq >= 950) && (CurrentFreq <2000))  // 1255 MHz
    {
      CurrentBand = 3;
    }
    if (CurrentFreq >= 2000)                          // 2400 MHz
    {
      CurrentBand = 4;
    }
  }
  else
  {
    CurrentBand = NoButton + 5;
  }

  // Look up the name of the new Band
  strcpy(Value, TabBand[CurrentBand]);

  // Store the new band
  printf("************** Set Band = %s\n", Value);
  strcpy(Param,"band");
  SetConfigParam(PATH_PCONFIG, Param, Value);


  // Make all the changes required after a band change
  DoFreqChange();

  // *** Not required, now in DoFreqChange() ***
  // Set the Band (and filter) Switching
  // system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using portsdown_config.txt
  // usleep(100000);
}

void SelectVidIP(int NoButton)  // Comp Vid or S-Video
{
  char USBVidDevice[255];
  char Param[255];
  char SetVidIP[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 6, NoButton, 1);
  strcpy(ModeVidIP, TabModeVidIP[NoButton - 5]);
  printf("************** Set Input socket= %s\n", ModeVidIP);
  strcpy(Param, "analogcaminput");
  SetConfigParam(PATH_PCONFIG,Param,ModeVidIP);

  // Now Set the Analog Capture (input) Socket
  // command format: v4l2-ctl -d $ANALOGCAMNAME --set-input=$ANALOGCAMINPUT

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetVidIP, "v4l2-ctl -d ");
    strcat(SetVidIP, USBVidDevice);
    strcat(SetVidIP, " --set-input=");
    strcat(SetVidIP, ModeVidIP);
    printf(SetVidIP);
    system(SetVidIP);
  }
}

void SelectAudio(int NoButton)  // Audio Input
{
  int AudioIndex;
  if (NoButton < 5)
  {
    AudioIndex = NoButton + 5;
  }
  else
  {
    AudioIndex = NoButton - 5;
  }

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 0, NoButton, 1);
  strcpy(ModeAudio, TabModeAudio[AudioIndex]);
  printf("************** Set Audio Input = %s\n",ModeAudio);
  char Param[]="audio";
  SetConfigParam(PATH_PCONFIG,Param,ModeAudio);
}

void SelectAtten(int NoButton)  // Attenuator Type
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  strcpy(CurrentAtten, TabAtten[NoButton - 5]);
  printf("************** Set Attenuator = %s\n", CurrentAtten);
  char Param[]="attenuator";
  SetConfigParam(PATH_PCONFIG, Param, CurrentAtten);
}

void SetAttenLevel()
{
  char Prompt[63];
  char Value[31];
  char Param[15];
  int ExpLevel = -1;
  float AttenLevel = 1;

  if (strcmp(CurrentModeOP, "DATVEXPRESS") == 0)
  {
    while ((ExpLevel < 0) || (ExpLevel > 44))
    {
      snprintf(Prompt, 62, "Set the Express Output Level for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 3, "%d", TabBandExpLevel[CurrentBand]);
      Keyboard(Prompt, Value, 2);
      ExpLevel = atoi(KeyboardReturn);
    }
    TabBandExpLevel[CurrentBand] = ExpLevel;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "explevel");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "explevel");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);
  }
  else
  {
    while ((AttenLevel > 0) || (AttenLevel < -31.75))
    {
      snprintf(Prompt, 62, "Set the Attenuator Level for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 7, "%f", TabBandAttenLevel[CurrentBand]);
      Keyboard(Prompt, Value, 6);
      AttenLevel = atof(KeyboardReturn);
    }
    TabBandAttenLevel[CurrentBand] = AttenLevel;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "attenlevel");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "attenlevel");
    SetConfigParam(PATH_PCONFIG, Param, KeyboardReturn);
  }
}

void SavePreset(int PresetButton)
{
  char Param[255];
  char Value[255];
  char Prompt[63];

  // Read the Preset Label and ask for a new value
  snprintf(Prompt, 62, "Enter the new label for Preset %d:", PresetButton + 1);
  strcpy(Value, "");
  snprintf(Param, 10, "p%dlabel", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  Keyboard(Prompt, Value, 10);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
  strcpy(TabPresetLabel[PresetButton], KeyboardReturn);

  // Save the current frequency to Preset
  strcpy(Value, "");
  strcpy(Param, "freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dfreq", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current band to Preset
  strcpy(Value, "");
  strcpy(Param, "band");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dband", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current mode to Preset
  strcpy(Value, CurrentTXMode);
  snprintf(Param, 10, "p%dmode", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current encoder to Preset
  strcpy(Value, CurrentEncoding);
  snprintf(Param, 10, "p%dencoder", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current format to Preset
  strcpy(Value, CurrentFormat);
  snprintf(Param, 10, "p%dformat", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current source to Preset
  strcpy(Value, CurrentSource);
  snprintf(Param, 10, "p%dsource", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current output device to Preset
  strcpy(Value, "");
  strcpy(Param, "modeoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%doutput", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current SR to Preset
  strcpy(Value, "");
  strcpy(Param, "symbolrate");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dsr", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current FEC to Preset
  strcpy(Value, "");
  strcpy(Param, "fec");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%dfec", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);

  // Save the current audio setting to Preset
  strcpy(Value, "");
  strcpy(Param, "audio");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  snprintf(Param, 10, "p%daudio", PresetButton + 1);
  SetConfigParam(PATH_PPRESETS, Param, Value);
}

void RecallPreset(int PresetButton)
{
  char Param[255];
  char Value[255];

  // Read Preset Frequency and store in portsdown_config (not in memory)
  strcpy(Value, "");
  snprintf(Param, 10, "p%dfreq", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "freqoutput", Value);

  // Read Preset Band and store in portsdown_config and in memory
  strcpy(Value, "");
  snprintf(Param, 10, "p%dband", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "band", Value);
  ReadBand();   //Sets CurrentBand consistently from saved freqoutput and band

  // Read modulation, encoding, format and source

  strcpy(Value, "");
  snprintf(Param, 10, "p%dmode", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentTXMode, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dencoder", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentEncoding, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dformat", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentFormat, Value);

  strcpy(Value, "");
  snprintf(Param, 15, "p%dsource", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  strcpy(CurrentSource, Value);

  // Decode them and set modeinput in memory and portsdown_config
  ApplyTXConfig();

  // Read Output Device
  strcpy(Value, "");
  snprintf(Param, 15, "p%doutput", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "modeoutput", Value);
  strcpy(Value, "");
  ReadModeOutput(Value);  // Set CurrentModeOP and CurrentModeOPtest

  // Read SR
  strcpy(Value, "");
  snprintf(Param, 15, "p%dsr", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "symbolrate", Value);
  SR = atoi(Value);

  // Read FEC
  strcpy(Value, "");
  snprintf(Param, 15, "p%dfec", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "fec", Value);
  fec = atoi(Value);
  
  // Read Audio setting
  strcpy(Value, "");
  snprintf(Param, 15, "p%daudio", PresetButton + 1);
  GetConfigParam(PATH_PPRESETS, Param, Value);
  SetConfigParam(PATH_PCONFIG, "audio", Value);
  strcpy(ModeAudio, Value);

  // Make sure that changes are applied
  DoFreqChange();
}

void TransmitStart()
{
  // printf("Transmit Start\n");

  char Param[255];
  char Value[255];
//  #define PATH_SCRIPT_A "sudo /home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"
  #define PATH_SCRIPT_A "/home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"

  strcpy(Param,"modeinput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  strcpy(ModeInput,Value);

  // Check if MPEG-2 camera mode selected 
  if ((strcmp(ModeInput, "CAMMPEG-2")==0)
    ||(strcmp(ModeInput, "CAM16MPEG-2")==0)
    ||(strcmp(ModeInput, "CAMHDMPEG-2")==0))
  {
    // Start the viewfinder
    finish();
    system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if H264 Camera selected
  if(strcmp(ModeInput,"CAMH264") == 0)
  {
    // Start the viewfinder 
    finish();
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if a desktop mode is selected; if so, display desktop
  if  ((strcmp(ModeInput,"CARDH264")==0)
    || (strcmp(ModeInput,"PATERNAUDIO") == 0)
    || (strcmp(ModeInput,"CONTEST") == 0) 
    || (strcmp(ModeInput,"DESKTOP") == 0)
    || (strcmp(ModeInput,"CARDMPEG-2")==0)
    || (strcmp(ModeInput,"CONTESTMPEG-2")==0)
    || (strcmp(ModeInput,"CARD16MPEG-2")==0)
    || (strcmp(ModeInput,"CARDHDMPEG-2")==0))
  {
    finish();
    strcpy(ScreenState, "TXwithImage");
  }

  // Check if non-display input mode selected.  If so, turn off response to buttons.
  if ((strcmp(ModeInput,"ANALOGCAM") == 0)
    ||(strcmp(ModeInput,"WEBCAMH264") == 0)
    ||(strcmp(ModeInput,"ANALOGMPEG-2") == 0)
    ||(strcmp(ModeInput,"CARRIER") == 0)
    ||(strcmp(ModeInput,"TESTMODE") == 0)
    ||(strcmp(ModeInput,"IPTSIN") == 0)
    ||(strcmp(ModeInput,"FILETS") == 0)
    ||(strcmp(ModeInput,"WEBCAMMPEG-2") == 0)
    ||(strcmp(ModeInput,"ANALOG16MPEG-2") == 0)
    ||(strcmp(ModeInput,"WEBCAM16MPEG-2") == 0)
    ||(strcmp(ModeInput,"WEBCAMHDMPEG-2") == 0)
    ||(strcmp(ModeInput,"C920H264") == 0)
    ||(strcmp(ModeInput,"C920HDH264") == 0)
    ||(strcmp(ModeInput,"C920FHDH264") == 0))
  {
     strcpy(ScreenState, "TXwithMenu");
  }

  // Call a.sh to transmit
  system(PATH_SCRIPT_A);
}

void *Wait3Seconds(void * arg)
{
  system ("/home/pi/rpidatv/scripts/webcam_reset.sh");
  strcpy(ScreenState, "NormalMenu");
  return NULL;
}

void TransmitStop()
{
  char Param[255];
  char Value[255];
  int WebcamPresent = 0;

  // printf("Transmit Stop\n");

  // Turn the VCO off
  system("sudo /home/pi/rpidatv/bin/adf4351 off");

  // Check for C910, C525 or C270 webcam
  WebcamPresent = DetectLogitechWebcam();

  // Stop DATV Express transmitting
  char expressrx[50];
  strcpy(Param,"modeoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  strcpy(ModeOutput,Value);
  if(strcmp(ModeOutput,"DATVEXPRESS")==0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
  }

  // Kill the key processes as nicely as possible
  system("sudo killall rpidatv >/dev/null 2>/dev/null");
  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");

  // Turn the Viewfinder off
  system("v4l2-ctl --overlay=0 >/dev/null 2>/dev/null");

  // Stop the audio relay in CompVid mode
  system("sudo killall arecord >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts >/dev/null 2>/dev/null");

  // And make sure rpidatv has been stopped (required for brief transmit selections)
  system("sudo killall -9 rpidatv >/dev/null 2>/dev/null");

  // Ensure PTT off.  Required for carrier mode
  pinMode(GPIO_PTT, OUTPUT);
  digitalWrite(GPIO_PTT, LOW);

  // Wait a further 3 seconds and reset v42l-ctl if Logitech C910, C270 or C525 present
  if (WebcamPresent == 1)
  {
    // Check if Webcam was in use
    if ((strcmp(ModeInput,"WEBCAMH264") == 0)
      ||(strcmp(ModeInput,"WEBCAMMPEG-2") == 0)
      ||(strcmp(ModeInput,"WEBCAM16MPEG-2") == 0)
      ||(strcmp(ModeInput,"WEBCAMHDMPEG-2") == 0))
    {
      strcpy(ScreenState, "WebcamWait");
      // Create Wait 3 seconds timer thread
      pthread_create (&thwait3, NULL, &Wait3Seconds, NULL);
    }
  }
}

void coordpoint(VGfloat x, VGfloat y, VGfloat size, VGfloat pcolor[4]) {
  setfill(pcolor);
  Circle(x, y, size);
  setfill(pcolor);
}

fftwf_complex *fftout=NULL;
#define FFT_SIZE 256

int FinishedButton=0;

void *DisplayFFT(void * arg)
{
	FILE * pFileIQ = NULL;
	int fft_size=FFT_SIZE;
	fftwf_complex *fftin;
	fftin = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
	fftout = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
	fftwf_plan plan ;
	plan = fftwf_plan_dft_1d(fft_size, fftin, fftout, FFTW_FORWARD, FFTW_ESTIMATE );

	system("mkfifo fifo.iq >/dev/null 2>/dev/null");
	printf("Entering FFT thread\n");
	pFileIQ = fopen("fifo.iq", "r");

	while(FinishedButton==0)
	{
		//int Nbread; // value set later but not used
		//int log2_N=11; //FFT 1024 not used?
		//int ret; // not used?

		//Nbread=fread( fftin,sizeof(fftwf_complex),FFT_SIZE,pFileIQ);
		fread( fftin,sizeof(fftwf_complex),FFT_SIZE,pFileIQ);
		fftwf_execute( plan );

		//printf("NbRead %d %d\n",Nbread,sizeof(struct GPU_FFT_COMPLEX));

		fseek(pFileIQ,(1200000-FFT_SIZE)*sizeof(fftwf_complex),SEEK_CUR);
	}
	fftwf_free(fftin);
	fftwf_free(fftout);
  return NULL;
}

void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
}

void ProcessLeandvb()
{
  #define PATH_SCRIPT_LEAN "sudo /home/pi/rpidatv/scripts/leandvbgui.sh 2>&1"
  char *line=NULL;
  size_t len = 0;
  ssize_t read;
  FILE *fp;
  VGfloat shapecolor[4];
  RGBA(255, 255, 128,1, shapecolor);

  printf("Entering LeandProcess\n");
  FinishedButton=0;

  // create Thread FFT
  pthread_create (&thfft,NULL, &DisplayFFT, NULL);

  // Create Wait Button thread
  pthread_create (&thbutton,NULL, &WaitButtonEvent, NULL);

  fp=popen(PATH_SCRIPT_LEAN, "r");
  if(fp==NULL) printf("Process error\n");

  while (((read = getline(&line, &len, fp)) != -1)&&(FinishedButton==0))
  {

        char  strTag[20];
	int NbData;
	static int Decim=0;
	sscanf(line,"%s ",strTag);
	char * token;
	static int Lock=0;
	static float SignalStrength=0;
	static float MER=0;
	static float FREQ=0;
	if((strcmp(strTag,"SYMBOLS")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%d",&NbData);

		if(Decim%25==0)
		{
			//Start(wscreen,hscreen);
			Fill(255, 255, 255, 1);
			Roundrect(0,0,256,hscreen, 10, 10);
			BackgroundRGB(0,0,0,0);
			//Lock status
			char sLock[100];
			if(Lock==1)
			{
				strcpy(sLock,"Lock");
				Fill(0,255,0, 1);

			}
			else
			{
				strcpy(sLock,"----");
				Fill(255,0,0, 1);
			}
			Roundrect(200,0,100,50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(200, 20, sLock, SerifTypeface, 25);

			//Signal Strength
			char sSignalStrength[100];
			sprintf(sSignalStrength,"%3.0f",SignalStrength);

			Fill(255-SignalStrength,SignalStrength,0,1);
			Roundrect(350,0,20+SignalStrength/2,50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(350, 20, sSignalStrength, SerifTypeface, 25);

			//MER 2-30
			char sMER[100];
			sprintf(sMER,"%2.1fdB",MER);
			Fill(255-MER*8,(MER*8),0,1);
			Roundrect(500,0,(MER*8),50, 10, 10);
			Fill(255, 255, 255, 1);				   // White text
			Text(500,20, sMER, SerifTypeface, 25);
		}

		if(Decim%25==0)
		{
			static VGfloat PowerFFTx[FFT_SIZE];
			static VGfloat PowerFFTy[FFT_SIZE];
			StrokeWidth(2);

			Stroke(150, 150, 200, 0.8);
			int i;
			if(fftout!=NULL)
			{
			for(i=0;i<FFT_SIZE;i+=2)
			{

				PowerFFTx[i]=(i<FFT_SIZE/2)?(FFT_SIZE+i)/2:i/2;
				PowerFFTy[i]=log10f(sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])/FFT_SIZE)*100;	
			Line(PowerFFTx[i],0,PowerFFTx[i],PowerFFTy[i]);
			//Polyline(PowerFFTx,PowerFFTy,FFT_SIZE);

			//Line(0, (i<1024/2)?(1024/2+i)/2:(i-1024/2)/2,  (int)sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])*100/1024,(i<1024/2)?(1024/2+i)/2:(i-1024/2)/2);

			}
			//Polyline(PowerFFTx,PowerFFTy,FFT_SIZE);
			}
			//FREQ
			Stroke(0, 0, 255, 0.8);
			//Line(FFT_SIZE/2+FREQ/2/1024000.0,0,FFT_SIZE/2+FREQ/2/1024000.0,hscreen/2);
			Line(FFT_SIZE/2,0,FFT_SIZE/2,10);
			Stroke(0, 0, 255, 0.8);
			Line(0,hscreen-300,256,hscreen-300);
			StrokeWidth(10);
			Line(128+(FREQ/40000.0)*256.0,hscreen-300-20,128+(FREQ/40000.0)*256.0,hscreen-300+20);

			char sFreq[100];
			sprintf(sFreq,"%2.1fkHz",FREQ/1000.0);
			Text(0,hscreen-300+25, sFreq, SerifTypeface, 20);

		}
		if((Decim%25)==0)
		{
			int x,y;
			Decim++;
			int i;
			StrokeWidth(2);
			Stroke(255, 255, 128, 0.8);
			for(i=0;i<NbData;i++)
			{
				token=strtok(NULL," ");
				sscanf(token,"%d,%d",&x,&y);
				coordpoint(x+128, hscreen-(y+128), 5, shapecolor);

				Stroke(0, 255, 255, 0.8);
				Line(0,hscreen-128,256,hscreen-128);
				Line(128,hscreen,128,hscreen-256);

			}


			End();
			//usleep(40000);

		}
		else
			Decim++;
		/*if(Decim%1000==0)
		{
			char FileSave[255];
			FILE *File;
			sprintf(FileSave,"Snap%d_%dx%d.png",Decim,wscreen,hscreen);
			File=fopen(FileSave,"w");

			dumpscreen(wscreen,hscreen,File);
			fclose(File);
		}*/
		/*if(Decim>200)
		{
			Decim=0;
			Start(wscreen,hscreen);

		}*/

	}

	if((strcmp(strTag,"SS")==0))
	{
		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&SignalStrength);
		//printf("Signal %f\n",SignalStrength);
	}

	if((strcmp(strTag,"MER")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&MER);
		//printf("MER %f\n",MER);
	}

	if((strcmp(strTag,"FREQ")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%f",&FREQ);
		//printf("FREQ %f\n",FREQ);
	}

	if((strcmp(strTag,"LOCK")==0))
	{

		token = strtok(line," ");
		token = strtok(NULL," ");
		sscanf(token,"%d",&Lock);
	}

	free(line);
	line=NULL;
    }
  printf("End Lean - Clean\n");
  system("sudo killall rtl_sdr >/dev/null 2>/dev/null");
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png");  // Add logo image

  MsgBox4("Switching back to Main Menu", "", "Please wait for the receiver", "to clean up files");
  usleep(5000000); // Time to FFT end reading samples
  pthread_join(thfft, NULL);
	//pclose(fp);
	pthread_join(thbutton, NULL);
	printf("End Lean\n");
 
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  system("sudo killall fbi >/dev/null 2>/dev/null");
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall ts2es >/dev/null 2>/dev/null");
  finish();
  strcpy(ScreenState, "RXwithImage");            //  Signal to display touch menu without further touch
}

void ReceiveStart()
{
  strcpy(ScreenState, "RXwithImage");
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  ProcessLeandvb();
}

void ReceiveStop()
{
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  printf("Receive Stop\n");
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
  printf("MsgBox called and waiting for touch\n");
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
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue", SansTypeface, 25);
  End();
  printf("MsgBox2 called and waiting for touch\n");
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
  printf("MsgBox4 called\n");
}


void InfoScreen()
{
  char result[256];
  char result2[256] = " ";

  // Look up and format all the parameters to be displayed

  char swversion[256] = "Software Version: ";
  if (GetLinuxVer() == 8)
  {
    strcat(swversion, "Jessie ");
  }
  else if (GetLinuxVer() == 9)
  {
    strcat(swversion, "Stretch ");
  }
  GetSWVers(result);
  strcat(swversion, result);

  char ipaddress[256] = "IP: ";
  strcpy(result, "Not connected");
  GetIPAddr(result);
  strcat(ipaddress, result);
  strcat(ipaddress, "    ");
  GetIPAddr2(result2);
  strcat(ipaddress, result2);

  char CPUTemp[256];
  GetCPUTemp(result);
  sprintf(CPUTemp, "CPU temp=%.1f\'C      GPU ", atoi(result)/1000.0);
  GetGPUTemp(result);
  strcat(CPUTemp, result);

  char PowerText[256] = "Temperature has been or is too high";
  GetThrottled(result);
  result[strlen(result) - 1]  = '\0';
  if(strcmp(result,"throttled=0x0")==0)
  {
    strcpy(PowerText,"Temperatures and Supply voltage OK");
  }
  if(strcmp(result,"throttled=0x50000")==0)
  {
    strcpy(PowerText,"Low supply voltage event since start-up");
  }
  if(strcmp(result,"throttled=0x50005")==0)
  {
    strcpy(PowerText,"Low supply voltage now");
  }
  //strcpy(PowerText,result);
  //strcat(PowerText,"End");

  char TXParams1[256] = "TX ";
  GetConfigParam(PATH_PCONFIG,"freqoutput",result);
  strcat(TXParams1, result);
  strcat(TXParams1, " MHz  SR ");
  GetConfigParam(PATH_PCONFIG,"symbolrate",result);
  strcat(TXParams1, result);
  strcat(TXParams1, "  FEC ");
  GetConfigParam(PATH_PCONFIG,"fec",result);
  strcat(TXParams1, result);
  strcat(TXParams1, "/");
  sprintf(result, "%d", atoi(result)+1);
  strcat(TXParams1, result);

  char TXParams2[256];
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);
  strcpy(TXParams2, vcoding);
  strcat(TXParams2, " coding from ");
  strcat(TXParams2, vsource);
  
  char TXParams3[256];
  char ModeOutput[256];
  ReadModeOutput(ModeOutput);
  strcpy(TXParams3, "Output to ");
  strcat(TXParams3, ModeOutput);

  char SerNo[256];
  char CardSerial[256] = "SD Card Serial: ";
  GetSerNo(SerNo);
  strcat(CardSerial, SerNo);

  char DeviceTitle[256] = "Audio Devices:";

  char Device1[256]=" ";
  char Device2[256]=" ";
  GetDevices(Device1, Device2);

  // Initialise and calculate the text display
  init(&wscreen, &hscreen);  // Restart the gui
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
  tw = TextWidth("BATC Portsdown Information Screen", font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), hscreen - linenumber * linepitch, "BATC Portsdown Information Screen", font, pointsize);
  linenumber = linenumber + 2.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, swversion, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, ipaddress, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, CPUTemp, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, PowerText, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams1, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams2, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, TXParams3, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, CardSerial, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, DeviceTitle, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, Device1, font, pointsize);
  linenumber = linenumber + 1.0;

  Text(wscreen/12.0, hscreen - linenumber * linepitch, Device2, font, pointsize);
  linenumber = linenumber + 1.0;

    tw = TextWidth("Touch Screen to Continue",  font, pointsize);
  Text(wscreen / 2.0 - (tw / 2.0), 20, "Touch Screen to Continue",  font, pointsize);

  // Push to screen
  End();

  printf("Info Screen called and waiting for touch\n");
  wait_touch();
}

void rtlradio1()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M wbfm -f 92.9M | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r32) &");
    system(rtlcall);

    MsgBox("Radio 4 92.9 FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio2()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M wbfm -f 106.0M | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r32) &");
    system(rtlcall);

    MsgBox("SAM FM 106.0");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio3()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 144.75M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("ATV Calling Channel 144.75 MHz FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}
void rtlradio4()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 145.7875M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("GB3BF Bedford 145.7875");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtlradio5()
{
  if(CheckRTL()==0)
  {
    char rtlcall[256];
    char card[256];
    GetPiAudioCard(card);
    strcpy(rtlcall, "(rtl_fm -M fm -f 145.8M -s 20k -g 50 -l 0 -E pad | aplay -D plughw:");
    strcat(rtlcall, card);
    strcat(rtlcall, ",0 -f S16_LE -r20 -t raw) &");
    system(rtlcall);

    MsgBox("ISS Downlink 145.8 MHz FM");
    wait_touch();

    system("sudo killall rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall aplay >/dev/null 2>/dev/null");
    usleep(1000);
    system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
    system("sudo killall -9 aplay >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void rtl_tcp()
{
  if(CheckRTL()==0)
  {
    char rtl_tcp_start[256];
    char current_IP[256];
    char message1[256];
    char message2[256];
    char message3[256];
    char message4[256];
    GetIPAddr(current_IP);
    strcpy(rtl_tcp_start,"(rtl_tcp -a ");
    strcat(rtl_tcp_start, current_IP);
    strcat(rtl_tcp_start, ") &");
    system(rtl_tcp_start);

    strcpy(message1, "RTL-TCP server running on");
    strcpy(message2, current_IP);
    strcat(message2, ":1234");
    strcpy(message3, "Touch screen again to");
    strcpy(message4, "stop the RTL-TCP Server");
    MsgBox4(message1, message2, message3, message4);
    wait_touch();

    system("sudo killall rtl_tcp >/dev/null 2>/dev/null");
    usleep(500);
    system("sudo killall -9 rtl_tcp >/dev/null 2>/dev/null");
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

void do_snap()
{
  char USBVidDevice[255];

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) != 12)  // /dev/video* with a new line
  {
    MsgBox("No EasyCap Found");
    wait_touch();
    UpdateWindow();
    BackgroundRGB(0,0,0,255);
  }
  else
  {
    finish();
    printf("do_snap\n");
    system("/home/pi/rpidatv/scripts/snap.sh >/dev/null 2>/dev/null");
    wait_touch();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
    system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
    init(&wscreen, &hscreen);
    Start(wscreen,hscreen);
    BackgroundRGB(0,0,0,255);
    UpdateWindow();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill fbi now
  }
}

void do_videoview()
{
  printf("videoview called\n");
  char Param[255];
  char Value[255];
  char USBVidDevice[255];
  char ffmpegCMD[255];

  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) != 12)  // /dev/video* with a new line
  {
    MsgBox("No EasyCap Found");
    wait_touch();
    UpdateWindow();
    BackgroundRGB(0,0,0,255);
  }
  else
  {
    // Make the display ready
    finish();

    // Create a thread to listen for display touches
    pthread_create (&thview,NULL, &WaitButtonEvent,NULL);

    strcpy(Param,"display");
    GetConfigParam(PATH_PCONFIG,Param,Value);
    if ((strcmp(Value,"Waveshare")==0) || (strcmp(Value,"Waveshare4")==0))
    // Write directly to the touchscreen framebuffer for Waveshare displays
    {
      USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
      strcpy(ffmpegCMD, "/home/pi/rpidatv/bin/ffmpeg -hide_banner -loglevel panic -f v4l2 -i ");
      strcat(ffmpegCMD, USBVidDevice);
      strcat(ffmpegCMD, " -vf \"yadif=0:1:0,scale=480:320\" -f rawvideo -pix_fmt rgb565 -vframes 3 /home/pi/tmp/frame.raw");
      system("sudo killall fbcp");
      // Refresh image until display touched
      while ( FinishedButton == 0 )
      {
        system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
        system(ffmpegCMD);
        system("split -b 307200 -d -a 1 /home/pi/tmp/frame.raw /home/pi/tmp/frame");
        system("cat /home/pi/tmp/frame2>/dev/fb1");
      }
      // Screen has been touched so stop and tidy up
      system("fbcp &");
      system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
    }
    else  // not a waveshare display so write to the main framebuffer
    {
      while ( FinishedButton == 0 )
      {
        system("/home/pi/rpidatv/scripts/view.sh");
        usleep(100000);
      }
    }
    // Screen has been touched
    printf("videoview exit\n");

    // Tidy up and display touch menu
    FinishedButton = 0;
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
    system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
    init(&wscreen, &hscreen);
    Start(wscreen,hscreen);
    BackgroundRGB(0,0,0,255);
    UpdateWindow();
    system("sudo killall fbi >/dev/null 2>/dev/null");  // kill fbi now
  }
}

void do_snapcheck()
{
  FILE *fp;
  char SnapIndex[256];
  int SnapNumber;
  int Snap;
  char fbicmd[256];

  // Fetch the Next Snap serial number
  fp = popen("cat /home/pi/snaps/snap_index.txt", "r");
  if (fp == NULL) 
  {
    printf("Failed to run command\n" );
    exit(1);
  }
  /* Read the output a line at a time - output it. */
  while (fgets(SnapIndex, 20, fp) != NULL)
  {
    printf("%s", SnapIndex);
  }
  /* close */
  pclose(fp);

  // Make the display ready
  finish();

  SnapNumber=atoi(SnapIndex);

  // Show the last 5 snaps
  for( Snap = SnapNumber - 1; Snap > SnapNumber - 6 && Snap >= 0; Snap = Snap - 1 )
  {
    sprintf(SnapIndex, "%d", Snap);
    strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
    strcat(fbicmd, SnapIndex);
    strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
    system(fbicmd);
    wait_touch();
  }

  // Tidy up and display touch menu
  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
  init(&wscreen, &hscreen);
  Start(wscreen,hscreen);
  BackgroundRGB(0,0,0,255);
  UpdateWindow();
}

static void cleanexit(int exit_code)
{
  strcpy(ModeInput, "DESKTOP"); // Set input so webcam reset script is not called
  TransmitStop();
  ReceiveStop();
  finish();
  printf("Clean Exit Code %d\n", exit_code);
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(exit_code);
}

void Keyboard(char RequestText[64], char InitText[64], int MaxLength)
{
  char EditText[64];
  strcpy (EditText, InitText);
  int token;
  int PreviousMenu;
  int i;
  int CursorPos;
  char thischar[2];
  int KeyboardShift = 1;
  int ShiftStatus = 0;
  char KeyPressed[2];
  char PreCuttext[63];
  char PostCuttext[63];
  
  printf("Entering Keyboard\n");

  // Store away currentMenu
  PreviousMenu = CurrentMenu;

  // Trim EditText to MaxLength
  if (strlen(EditText) > MaxLength)
  {
    strncpy(EditText, &EditText[0], MaxLength);
    EditText[MaxLength] = '\0';
  }

  // Set cursor position to next character after EditText
  CursorPos = strlen(EditText);

  // On initial call set Menu to 41
  CurrentMenu = 41;
  Fontinfo font = SansTypeface;
  int pointsize;
  int rawX, rawY, rawPressure;
  Fill(255, 255, 255, 1);    // White text

  for (;;)
  {
    BackgroundRGB(0,0,0,255);

    // Display Instruction Text
    pointsize = 20;
    Text(wscreen / 24, 7 * hscreen/8 , RequestText, font, pointsize);

    //Draw the cursor
    Fill(0, 0, 0, 1);    // Black Box
    StrokeWidth(3);
    Stroke(255, 255, 255, 0.8);  // White boundary
    Rect((2*CursorPos+1) * wscreen/48, 10.75 * hscreen / 16, wscreen/288, hscreen/12);
    StrokeWidth(0);

    // Display Text for Editing
    pointsize = 30;
    Fill(255, 255, 255, 1);    // White text
    for (i = 1; i <= strlen(EditText); i = i + 1)
    {
      strncpy(thischar, &EditText[i-1], 1);
      thischar[1] = '\0';
      if (i > MaxLength)
      {
        Fill(255, 127, 127, 1);    // Red text
      }
      TextMid(i * wscreen / 24.0, 11 * hscreen / 16, thischar, font, pointsize);
    }
    Fill(255, 255, 255, 1);    // White text

    // Sort Shift changes
    ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
    for (i = ButtonNumber(CurrentMenu, 0); i <= ButtonNumber(CurrentMenu, 49); i = i + 1)
    {
      if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
      {
        SetButtonStatus(i, ShiftStatus);
      }
    }  

    // Display the completed keyboard page
    UpdateWindow();

    // Wait for key press
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    token = IsMenuButtonPushed(rawX, rawY);
    printf("Keyboard Token %d\n", token);

    // Highlight special keys when touched
    if ((token == 8) || (token == 9) || (token == 2) || (token == 3)) // Enter, backspace, L and R arrows
    {
        SetButtonStatus(ButtonNumber(41, token), 1);
        DrawButton(ButtonNumber(41, token));
        UpdateWindow();
        usleep(300000);
        SetButtonStatus(ButtonNumber(41, token), 0);
        DrawButton(ButtonNumber(41, token));
        UpdateWindow();
    }

    if (token == 8)  // Enter pressed
    {
      if (strlen(EditText) > MaxLength) 
      {
        strncpy(KeyboardReturn, &EditText[0], MaxLength);
        KeyboardReturn[MaxLength] = '\0';
      }
      else
      {
        strcpy(KeyboardReturn, EditText);
      }
      CurrentMenu = PreviousMenu;
      printf("Exiting KeyBoard with String \"%s\"\n", KeyboardReturn);
      break;
    }
    else
    {    
      if (KeyboardShift == 1)     // Upper Case
      {
        switch (token)
        {
        case 0:  strcpy(KeyPressed, " "); break;
        case 4:  strcpy(KeyPressed, "-"); break;
        case 10: strcpy(KeyPressed, "Z"); break;
        case 11: strcpy(KeyPressed, "X"); break;
        case 12: strcpy(KeyPressed, "C"); break;
        case 13: strcpy(KeyPressed, "V"); break;
        case 14: strcpy(KeyPressed, "B"); break;
        case 15: strcpy(KeyPressed, "N"); break;
        case 16: strcpy(KeyPressed, "M"); break;
        case 17: strcpy(KeyPressed, ","); break;
        case 18: strcpy(KeyPressed, "."); break;
        case 19: strcpy(KeyPressed, "/"); break;
        case 20: strcpy(KeyPressed, "A"); break;
        case 21: strcpy(KeyPressed, "S"); break;
        case 22: strcpy(KeyPressed, "D"); break;
        case 23: strcpy(KeyPressed, "F"); break;
        case 24: strcpy(KeyPressed, "G"); break;
        case 25: strcpy(KeyPressed, "H"); break;
        case 26: strcpy(KeyPressed, "J"); break;
        case 27: strcpy(KeyPressed, "K"); break;
        case 28: strcpy(KeyPressed, "L"); break;
        case 30: strcpy(KeyPressed, "Q"); break;
        case 31: strcpy(KeyPressed, "W"); break;
        case 32: strcpy(KeyPressed, "E"); break;
        case 33: strcpy(KeyPressed, "R"); break;
        case 34: strcpy(KeyPressed, "T"); break;
        case 35: strcpy(KeyPressed, "Y"); break;
        case 36: strcpy(KeyPressed, "U"); break;
        case 37: strcpy(KeyPressed, "I"); break;
        case 38: strcpy(KeyPressed, "O"); break;
        case 39: strcpy(KeyPressed, "P"); break;
        case 40: strcpy(KeyPressed, "1"); break;
        case 41: strcpy(KeyPressed, "2"); break;
        case 42: strcpy(KeyPressed, "3"); break;
        case 43: strcpy(KeyPressed, "4"); break;
        case 44: strcpy(KeyPressed, "5"); break;
        case 45: strcpy(KeyPressed, "6"); break;
        case 46: strcpy(KeyPressed, "7"); break;
        case 47: strcpy(KeyPressed, "8"); break;
        case 48: strcpy(KeyPressed, "9"); break;
        case 49: strcpy(KeyPressed, "0"); break;
        }
      }
      else                          // Lower Case
      {
        switch (token)
        {
        case 0:  strcpy(KeyPressed, " "); break;
        case 4:  strcpy(KeyPressed, "_"); break;
        case 10: strcpy(KeyPressed, "z"); break;
        case 11: strcpy(KeyPressed, "x"); break;
        case 12: strcpy(KeyPressed, "c"); break;
        case 13: strcpy(KeyPressed, "v"); break;
        case 14: strcpy(KeyPressed, "b"); break;
        case 15: strcpy(KeyPressed, "n"); break;
        case 16: strcpy(KeyPressed, "m"); break;
        case 17: strcpy(KeyPressed, "!"); break;
        case 18: strcpy(KeyPressed, "."); break;
        case 19: strcpy(KeyPressed, "?"); break;
        case 20: strcpy(KeyPressed, "a"); break;
        case 21: strcpy(KeyPressed, "s"); break;
        case 22: strcpy(KeyPressed, "d"); break;
        case 23: strcpy(KeyPressed, "f"); break;
        case 24: strcpy(KeyPressed, "g"); break;
        case 25: strcpy(KeyPressed, "h"); break;
        case 26: strcpy(KeyPressed, "j"); break;
        case 27: strcpy(KeyPressed, "k"); break;
        case 28: strcpy(KeyPressed, "l"); break;
        case 30: strcpy(KeyPressed, "q"); break;
        case 31: strcpy(KeyPressed, "w"); break;
        case 32: strcpy(KeyPressed, "e"); break;
        case 33: strcpy(KeyPressed, "r"); break;
        case 34: strcpy(KeyPressed, "t"); break;
        case 35: strcpy(KeyPressed, "y"); break;
        case 36: strcpy(KeyPressed, "u"); break;
        case 37: strcpy(KeyPressed, "i"); break;
        case 38: strcpy(KeyPressed, "o"); break;
        case 39: strcpy(KeyPressed, "p"); break;
        case 40: strcpy(KeyPressed, "1"); break;
        case 41: strcpy(KeyPressed, "2"); break;
        case 42: strcpy(KeyPressed, "3"); break;
        case 43: strcpy(KeyPressed, "4"); break;
        case 44: strcpy(KeyPressed, "5"); break;
        case 45: strcpy(KeyPressed, "6"); break;
        case 46: strcpy(KeyPressed, "7"); break;
        case 47: strcpy(KeyPressed, "8"); break;
        case 48: strcpy(KeyPressed, "9"); break;
        case 49: strcpy(KeyPressed, "0"); break;
        }
      }

      // If shift key has been pressed Change Case
      if ((token == 6) || (token == 7))
      {
        if (KeyboardShift == 1)
        {
          KeyboardShift = 0;
        }
        else
        {
          KeyboardShift = 1;
        }
      }
      else if (token == 9)   // backspace
      {
        if (CursorPos == 1)
        {
           // Copy the text to the right of the deleted character
          strncpy(PostCuttext, &EditText[1], strlen(EditText) - 1);
          PostCuttext[strlen(EditText) - 1] = '\0';
          strcpy (EditText, PostCuttext);
          CursorPos = 0;
        }
        if (CursorPos > 1)
        {
          // Copy the text to the left of the deleted character
          strncpy(PreCuttext, &EditText[0], CursorPos - 1);
          PreCuttext[CursorPos-1] = '\0';

          if (CursorPos == strlen(EditText))
          {
            strcpy (EditText, PreCuttext);
          }
          else
          {
            // Copy the text to the right of the deleted character
            strncpy(PostCuttext, &EditText[CursorPos], strlen(EditText));
            PostCuttext[strlen(EditText)-CursorPos] = '\0';

            // Now combine them
            strcat(PreCuttext, PostCuttext);
            strcpy (EditText, PreCuttext);
          }
          CursorPos = CursorPos - 1;
        }
      }
      else if (token == 2)   // left arrow
      {
        CursorPos = CursorPos - 1;
        if (CursorPos < 0)
        {
          CursorPos = 0;
        }
      }
      else if (token == 3)   // right arrow
      {
        CursorPos = CursorPos + 1;
        if (CursorPos > strlen(EditText))
        {
          CursorPos = strlen(EditText);
        }
      }
      else if ((token == 0) || (token == 4) || ((token >=10) && (token <= 49)))
      {
        // character Key has been touched, so highlight it for 300 ms
 
        ShiftStatus = 3 - (2 * KeyboardShift); // 1 = Upper, 3 = lower
        SetButtonStatus(ButtonNumber(41, token), ShiftStatus);
        DrawButton(ButtonNumber(41, token));
        UpdateWindow();
        usleep(300000);
        ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
        SetButtonStatus(ButtonNumber(41, token), ShiftStatus);
        DrawButton(ButtonNumber(41, token));
        UpdateWindow();

        if (strlen(EditText) < 23) // Don't let it overflow
        {
        // Copy the text to the left of the insert point
        strncpy(PreCuttext, &EditText[0], CursorPos);
        PreCuttext[CursorPos] = '\0';
          
        // Append the new character to the pre-insert string
        strcat(PreCuttext, KeyPressed);

        // Append any text to the right if it exists
        if (CursorPos == strlen(EditText))
        {
          strcpy (EditText, PreCuttext);
        }
        else
        {
          // Copy the text to the right of the inserted character
          strncpy(PostCuttext, &EditText[CursorPos], strlen(EditText));
          PostCuttext[strlen(EditText)-CursorPos] = '\0';

          // Now combine them
          strcat(PreCuttext, PostCuttext);
          strcpy (EditText, PreCuttext);
        }
        CursorPos = CursorPos+1;
        }
      }
    }
  }
}

void ChangePresetFreq(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char PresetNo[2];
  int FreqIndex;
  float TvtrFreq;
  float PDfreq;
  char Param[15] = "pfreq";

  //Convert button number to frequency array index
  if (NoButton < 4)
  {
    FreqIndex = NoButton + 5;
  }
  else
  {
    FreqIndex = NoButton - 5;
  }

  //Define request string depending on transverter or not
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    strcpy(RequestText, "Enter new frequency for Button ");
  }
  else
  {
    strcpy(RequestText, "Enter new transmit frequency for Button ");
  }
  snprintf(PresetNo, 2, "%d", FreqIndex + 1);
  strcat(RequestText, PresetNo);
  strcat(RequestText, " in MHz:");

  // Calulate initial value
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    snprintf(InitText, 10, "%s", TabFreq[FreqIndex]);
  }
  else
  {
    TvtrFreq = atof(TabFreq[FreqIndex]) + TabBandLO[CurrentBand];
    if (TvtrFreq < 0)
    {
      TvtrFreq = TvtrFreq * -1;
    }
    snprintf(InitText, 10, "%.1f", TvtrFreq);
  }
  
  Keyboard(RequestText, InitText, 10);

  // Correct freq for transverter offset
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    ;
  }
  else
  {
    PDfreq = atof(KeyboardReturn) - TabBandLO[CurrentBand];
    if (TabBandLO[CurrentBand] < 0)
    {
      PDfreq = TabBandLO[CurrentBand] - atof(KeyboardReturn);
    }
    snprintf(KeyboardReturn, 10, "%.1f", PDfreq);
  }
  
  // Write freq to tabfreq
  strcpy(TabFreq[FreqIndex], KeyboardReturn);

  // write freq to Presets file
  strcat(Param, PresetNo); 
  printf("Store Preset %s %s\n", Param, KeyboardReturn);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);

  // Compose Button Text
  switch (strlen(TabFreq[FreqIndex]))
  {
  case 2:
    strcpy(FreqLabel[FreqIndex], " ");
    strcat(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz ");
    break;
  case 3:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz ");
    break;
  case 4:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " MHz");
    break;
  case 5:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], "MHz");
    break;
  default:
    strcpy(FreqLabel[FreqIndex], TabFreq[FreqIndex]);
    strcat(FreqLabel[FreqIndex], " M");
    break;
  }
}

void ChangePresetSR(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char PresetNo[2];
  int SRIndex;
  int SRCheck = 0;
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  if (NoButton < 4)
  {
    SRIndex = NoButton + 5;
  }
  else
  {
    SRIndex = NoButton - 5;
  }

  while ((SRCheck < 50) || (SRCheck > 9999))
  {
    strcpy(RequestText, "Enter new Symbol Rate for Button ");
    snprintf(PresetNo, 2, "%d", SRIndex + 1);
    strcat(RequestText, PresetNo);
    strcat(RequestText, " in KS/s:");
    snprintf(InitText, 5, "%d", TabSR[SRIndex]);
    Keyboard(RequestText, InitText, 4);
  
    // Check valid value
    SRCheck = atoi(KeyboardReturn);
  }

  // Update stored value
  TabSR[SRIndex] = SRCheck;

  // write SR to Presets file
  char Param[8] = "psr";
  strcat(Param, PresetNo); 
  printf("Store Preset %s %s\n", Param, KeyboardReturn);
  SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);

  // Compose and update Button Text
  switch (strlen(KeyboardReturn))
  {
  case 2:
  case 3:
    strcpy(SRLabel[SRIndex], "SR ");
    strcat(SRLabel[SRIndex], KeyboardReturn);
    break;
  default:
    strcpy(SRLabel[SRIndex], "SR");
    strcat(SRLabel[SRIndex], KeyboardReturn);
    break;
  }

  // refresh Menu 28 buttons

  AmendButtonStatus(ButtonNumber(28, NoButton), 0, SRLabel[SRIndex], &Blue);
  AmendButtonStatus(ButtonNumber(28, NoButton), 1, SRLabel[SRIndex], &Green);

  // refresh Menu 17 buttons
  AmendButtonStatus(ButtonNumber(17, NoButton), 0, SRLabel[SRIndex], &Blue);
  AmendButtonStatus(ButtonNumber(17, NoButton), 1, SRLabel[SRIndex], &Green);

  // Undo button highlight
  // SetButtonStatus(ButtonNumber(28, NoButton), 0);
}

void ChangeCall()
{
  char RequestText[64];
  char InitText[64];
  int Spaces = 1;
  int j;

  while (Spaces >= 1)
  {
    strcpy(RequestText, "Enter new Callsign (NO SPACES ALLOWED)");
    snprintf(InitText, 24, "%s", CallSign);
    Keyboard(RequestText, InitText, 23);
  
    // Check that there are no spaces
    Spaces = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if (isspace(KeyboardReturn[j]))
      {
        Spaces = Spaces +1;
      }
    }
  }
  strcpy(CallSign, KeyboardReturn);
  printf("Callsign set to: %s\n", CallSign);
  SetConfigParam(PATH_PCONFIG, "call", KeyboardReturn);
}

void ChangeLocator()
{
  char RequestText[64];
  char InitText[64];
  int Spaces = 1;
  int j;

  while (Spaces >= 1)
  {
    strcpy(RequestText, "Enter new Locator (6, 8 or 10 char, NO SPACES)");
    snprintf(InitText, 11, "%s", Locator);
    Keyboard(RequestText, InitText, 10);
  
    // Check that there are no spaces
    Spaces = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if (isspace(KeyboardReturn[j]))
      {
        Spaces = Spaces +1;
      }
    }
  }
  strcpy(Locator, KeyboardReturn);
  printf("Locator set to: %s\n", Locator);
  SetConfigParam(PATH_PCONFIG, "locator", KeyboardReturn);
}

void ChangeADFRef(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char param[64];
  int Spaces = 1;
  int j;

  switch(NoButton)
  {
  case 5:
    strcpy(CurrentADFRef, ADFRef[0]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 6:
    strcpy(CurrentADFRef, ADFRef[1]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 7:
    strcpy(CurrentADFRef, ADFRef[2]);
    SetConfigParam(PATH_PCONFIG, "adfref", CurrentADFRef);
    break;
  case 0:
  case 1:
  case 2:
    snprintf(RequestText, 45, "Enter new ADF Reference Frequncy %d in Hz", NoButton + 1);
    snprintf(InitText, 10, "%s", ADFRef[NoButton]);
    while (Spaces >= 1)
    {
      Keyboard(RequestText, InitText, 8);
  
      // Check that there are no spaces or other characters
      Spaces = 0;
      for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
      {
        if ( !(isdigit(KeyboardReturn[j])) )
        {
          Spaces = Spaces + 1;
        }
      }
    }
    strcpy(ADFRef[NoButton], KeyboardReturn);
    snprintf(param, 8, "adfref%d", NoButton + 1);
    SetConfigParam(PATH_PPRESETS, param, KeyboardReturn);
    strcpy(CurrentADFRef, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "adfref", KeyboardReturn);
    break;
  default:
    break;
  }
  printf("ADFRef set to: %s\n", CurrentADFRef);
}

void ChangePID(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  int PIDValid = 1;  // 0 represents valid
  int j;

  while (PIDValid >= 1)
  {
    switch (NoButton)
    {
    case 0:
      strcpy(RequestText, "Enter new Video PID (Range 16 - 8190, Rec: 256)");
      snprintf(InitText, 5, "%s", PIDvideo);
      break;
    case 1:
      strcpy(RequestText, "Enter new Audio PID (Range 16 - 8190 Rec: 257)");
      snprintf(InitText, 5, "%s", PIDaudio);
      break;
    case 2:
      strcpy(RequestText, "Enter new PMT PID (Range 16 - 8190 Rec: 4095)");
      snprintf(InitText, 5, "%s", PIDpmt);
      break;
    default:
      return;
    }

    Keyboard(RequestText, InitText, 4);
  
    // Check that there are no spaces and it is valid
    PIDValid = 0;
    for (j = 0; j < strlen(KeyboardReturn); j = j + 1)
    {
      if (isspace(KeyboardReturn[j]))
      {
        PIDValid = PIDValid + 1;
      }
    }
    if ( PIDValid == 0)
    {
      if ((atoi(KeyboardReturn) < 16 ) || (atoi(KeyboardReturn) > 8190 ))
      {
        PIDValid = 1;   // Out of bounds
      }
    }
  }
  switch (NoButton)
  {
  case 0:
    strcpy(PIDvideo, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidvideo", KeyboardReturn);
    strcpy(PIDstart, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidstart", KeyboardReturn);
    break;
  case 1:
    strcpy(PIDaudio, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidaudio", KeyboardReturn);
    break;
  case 2:
    strcpy(PIDpmt, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "pidpmt", KeyboardReturn);
    break;
  }
  printf("PID set to: %s\n", KeyboardReturn);
}


void waituntil(int w,int h)
{
  // Wait for a screen touch and act on its position

  int rawX, rawY, rawPressure, i;

  // printf("Entering WaitUntil\n");
  // Start the main loop for the Touchscreen
  for (;;)
  {
    if (strcmp(ScreenState, "RXwithImage") != 0) // Don't wait for touch if returning from recieve
    {
      // Wait here until screen touched
      if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
    }

    // Screen has been touched or returning from recieve
    printf("x=%d y=%d\n", rawX, rawY);

    // React differently depending on context: char ScreenState[255]

      // Menu (normal)                              NormalMenu  (implemented)
      // Menu (Specials)                            SpecialMenu (not implemented yet)
      // Transmitting
        // with image displayed                     TXwithImage (implemented)
        // with menu displayed but not active       TXwithMenu  (implemented)
      // Receiving                                  RXwithImage (implemented)
      // Video Output                               VideoOut    (not implemented yet)
      // Snap View                                  SnapView    (not implemented yet)
      // VideoView                                  VideoView   (not implemented yet)
      // Snap                                       Snap        (not implemented yet)
      // SigGen?                                    SigGen      (not implemented yet)
      // WebcamWait                                 Waiting for Webcam reset. Touch listens but does not respond

      //printf("Screenstate is %s \n", ScreenState);

     // Sort TXwithImage first:
    if (strcmp(ScreenState, "TXwithImage") == 0)
    {
      TransmitStop();
      ReceiveStop();
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(255,255,255,255);
      SelectPTT(20,0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;  // All reset, and Menu displayed so go back and wait for next touch
     }

    // Now Sort TXwithMenu:
    if (strcmp(ScreenState, "TXwithMenu") == 0)
    {
      TransmitStop();
      SelectPTT(20, 0);
      UpdateWindow();
      if (strcmp(ScreenState, "WebcamWait") != 0)
      {
        strcpy(ScreenState, "NormalMenu");
      }
      continue;
    }

    if (strcmp(ScreenState, "WebcamWait") == 0)
    {
      // Do nothing
      printf("In WebcamWait \n");
      continue;
    }

    // Now deal with return from receiving
    if (strcmp(ScreenState, "RXwithImage") == 0)
    {
      ReceiveStop();
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(255,255,255,255);
      SelectPTT(21,0);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    // Not transmitting or receiving, so sort NormalMenu
    if (strcmp(ScreenState, "NormalMenu") == 0)
    {
      // For Menu (normal), check which button has been pressed (Returns 0 - 23)

      i = IsMenuButtonPushed(rawX, rawY);
      if (i == -1)
      {
        continue;  //Pressed, but not on a button so wait for the next touch
      }

      // Now do the reponses for each Menu in turn
      if (CurrentMenu == 1)  // Main Menu
      {
        printf("Button Event %d, Entering Menu 1 Case Statement\n",i);

        // Clear Preset store trigger if not a preset
        if ((i > 4) && (PresetStoreTrigger == 1))
        {
          PresetStoreTrigger = 0;
          SetButtonStatus(4,0);
          UpdateWindow();
          continue;
        }

        switch (i)
        {

        case 0:
        case 1:
        case 2:
        case 3:
          if (PresetStoreTrigger == 0)
          {
            RecallPreset(i);  // Recall preset
            // and make sure that everything has been refreshed?
          }
          else
          {
            SavePreset(i);  // Set preset
            PresetStoreTrigger = 0;
            SetButtonStatus(4,0);
            BackgroundRGB(255,255,255,255);
          }
          SetButtonStatus(i, 1);
          Start_Highlights_Menu1();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(i, 0); 
          UpdateWindow();
          break;
        case 4:                        // Set up to store preset
          if (PresetStoreTrigger == 0)
          {
            PresetStoreTrigger = 1;
            SetButtonStatus(4,2);
          }
          else
          {
            PresetStoreTrigger = 0;
            SetButtonStatus(4,0);
          }
          UpdateWindow();
          break;
        case 5:
          printf("MENU 21 \n");       // EasyCap
          CurrentMenu=21;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu21();
          UpdateWindow();
          break;
        case 6:
          printf("MENU 22 \n");       // Caption
          CurrentMenu=22;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu22();
          UpdateWindow();
          break;
        case 7:
          printf("MENU 23 \n");       // Audio
          CurrentMenu=23;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu23();
          UpdateWindow();
          break;
        case 8:
          printf("MENU 24 \n");       // Attenuator
          CurrentMenu=24;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu24();
          UpdateWindow();
          break;
        case 9:                       // Spare
          UpdateWindow();
          break;
        case 10:
          printf("MENU 16 \n");        // Frequency
          CurrentMenu=16;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu16();
          UpdateWindow();
          break;
        case 11:
          printf("MENU 17 \n");       // SR
          CurrentMenu=17;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu17();
          UpdateWindow();
          break;
        case 12:
          printf("MENU 18 \n");       // FEC
          CurrentMenu=18;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu18();
          UpdateWindow();
          break;
        case 13:                      // Transverter
          printf("MENU 19 \n");
          CurrentMenu=19;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu19();
          UpdateWindow();
          break;
        case 14:                         // Level
          printf("Set Attenuator Level \n");
          SetAttenLevel();
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 15:
          printf("MENU 11 \n");        // Modulation
          CurrentMenu=11;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu11();
          UpdateWindow();
          break;
        case 16:
          printf("MENU 12 \n");        // Encoding
          CurrentMenu=12;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu12();
          UpdateWindow();
          break;
        case 17:
          printf("MENU 13 \n");        // Output Device
          CurrentMenu=13;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu13();
          UpdateWindow();
          break;
        case 18:
          printf("MENU 14 \n");        // Format
          CurrentMenu=14;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu14();
          UpdateWindow();
          break;
        case 19:
          printf("MENU 15 \n");        // Source
          CurrentMenu=15;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu15();
          UpdateWindow();
          break;
        case 20:                       // TX PTT
          //usleep(500000);
          SelectPTT(i,1);
          UpdateWindow();
          TransmitStart();
          break;
        case 21:                       // RX
          if(CheckRTL()==0)
          {
            BackgroundRGB(0,0,0,255);
            Start(wscreen,hscreen);
            ReceiveStart();
            break;
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
            BackgroundRGB(255,255,255,255);
            UpdateWindow();
          }
          break;
        case 22:                      // Not shown
          break;
        case 23:                      // Select Menu 2
          printf("MENU 2 \n");
          CurrentMenu=2;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        default:
          printf("Menu 1 Error\n");
        }
        continue;  // Completed Menu 1 action, go and wait for touch
      }

      if (CurrentMenu == 2)  // Menu 2
      {
        printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
        switch (i)
        {
        case 0:                               // Shutdown
          MsgBox4("", "Shutting down now", "", "");
          usleep(1000000);
          finish();
          system("sudo shutdown now");
          break;
        case 1:                               // Reboot
          MsgBox4("", "Rebooting now", "", "");
          usleep(1000000);
          finish();
          system("sudo reboot now");
          break;
        case 2:                               // Display Info Page
          InfoScreen();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 3:                              // Calibrate Touch
          touchcal();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 4:                               //  
          UpdateWindow();
          break;
        case 5:                             // RTL Radio 144.75
          rtlradio3();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 6:                             // RTL Radio 145.8
          rtlradio5();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 7:                               // 
          UpdateWindow();
          break;
        case 8:                               // 
          UpdateWindow();
          break;
        case 9:                               // 
          UpdateWindow();
          break;
        case 10:                              // Take Snap from EasyCap Input
          do_snap();
          UpdateWindow();
          break;
        case 11:                              // View EasyCap Input
          do_videoview();
          UpdateWindow();
          break;
        case 12:                              // Check Snaps
          do_snapcheck();
          UpdateWindow();
          break;
        case 13:                              //
          UpdateWindow();
          break;
        case 14:                              //
          UpdateWindow();
          break;
        case 15:                               // Select FreqShow
          if(CheckRTL()==0)
          {
            cleanexit(131);
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
          }
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 16:                               // Start Sig Gen and Exit
          cleanexit(130);
          break;
        case 17:                               // Start RTL-TCP server
          rtl_tcp();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 18:                              // (SpyServer?)
          UpdateWindow();
          break;
        case 19:                              // (Locator Calc?)
          UpdateWindow();
          break;
        case 20:                              // Not shown
          ;
          break;
        case 21:                              // Not shown
          ;
          break;
        case 22:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                              // Menu 3
          printf("MENU 3 \n");
          CurrentMenu=3;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu3();
          UpdateWindow();
          break;
        default:
          printf("Menu 2 Error\n");
        }
        continue;   // Completed Menu 2 action, go and wait for touch
      }

      if (CurrentMenu == 3)  // Menu 3
      {
        printf("Button Event %d, Entering Menu 3 Case Statement\n",i);
        switch (i)
        {
        case 0:                              // Check for Update
          printf("MENU 33 \n"); 
          CurrentMenu=33;
          BackgroundRGB(0,0,0,255);
          PrepSWUpdate();
          Start_Highlights_Menu33();
          UpdateWindow();
          break;
        case 1:                               // 
          break;
        case 2:                               // 
          break;
        case 3:                               // 
          break;
        case 4:                               // 
          break;
         case 5:                              // 
          break;
        case 6:                              // 
          break;
        case 7:                              // 
          break;
        case 8:                              // 
          break;
        case 9:                              // 
          break;
        case 10:                               // Blank
          break;
        case 11:                               // Blank
          break;
        case 12:                               // Blank
          break;
        case 13:                               // Blank
          break;
        case 14:                               // Blank
          break;
        case 15:                              // Set Band Details
          printf("MENU 26 \n"); 
          CurrentMenu=26;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu26();
          UpdateWindow();
          break;
        case 16:                              // Set Preset Frequencies
          printf("MENU 27 \n"); 
          CurrentMenu=27;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu27();
          UpdateWindow();
          break;
        case 17:                              // Set Preset SRs
          printf("MENU 28 \n"); 
          CurrentMenu=28;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu28();
          UpdateWindow();
          break;
        case 18:                              // Set Call, Loc and PIDs
          printf("MENU 29 \n"); 
          CurrentMenu=29;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 19:                               // Set ADF Reference Frequency
          printf("MENU 32 \n"); 
          CurrentMenu=32;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu32();
          UpdateWindow();
          break;
        case 20:                              // Not shown
          break;
        case 21:                              // Not shown
          break;
        case 22:                              // Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                              // Not Shown
          break;
        default:
          printf("Menu 3 Error\n");
        }
        continue;   // Completed Menu 3 action, go and wait for touch
      }
      if (CurrentMenu == 11)  // Menu 11 TX RF Output Mode
      {
        printf("Button Event %d, Entering Menu 11 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 5:                               // DVB-S
          SelectTX(i);
          printf("DVB-S\n");
          break;
        case 6:                               // Carrier
          SelectTX(i);
          printf("Carrier\n");
          break;
        default:
          printf("Menu 11 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 11\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 11 action, go and wait for touch
      }
      if (CurrentMenu == 12)  // Menu 12 Encoding
      {
        printf("Button Event %d, Entering Menu 12 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Encoding Cancel\n");
          break;
        case 5:                               // H264
          SelectEncoding(i);
          printf("H264\n");
          break;
        case 6:                               // MPEG-2
          SelectEncoding(i);
          printf("MPEG-2\n");
          break;
        case 7:                               // IPTS in
          SelectEncoding(i);
          printf("IPTS in\n");
          break;
        case 8:                               // TS File
          SelectEncoding(i);
          printf("TS File\n");
          break;
        default:
          printf("Menu 12 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 12\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 12 action, go and wait for touch
      }
      if (CurrentMenu == 13)  // Menu 13 Output Device
      {
        printf("Button Event %d, Entering Menu 13 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Encoding Cancel\n");
          break;
        case 5:                               // IQ
          SelectOP(i);
          printf("IQ\n");
          break;
        case 6:                               // QPSKRF
          SelectOP(i);
          printf("QPSKRF\n");
          break;
        case 7:                               // EXPRESS
          SelectOP(i);
          printf("EXPRESS\n");
          break;
        case 8:                               // BATC
          SelectOP(i);
          printf("BATC\n");
          break;
        case 9:                               // STREAMER
          SelectOP(i);
          printf("STREAMER\n");
          break;
        case 0:                               // COMPVID
          SelectOP(i);
          printf("COMPVID\n");
          break;
        case 1:                               // DTX-1
          SelectOP(i);
          printf("DTX-1\n");
          break;
        case 2:                               // IPTS
          SelectOP(i);
          printf("IPTS\n");
          break;
        default:
          printf("Menu 13 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 13\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 13 action, go and wait for touch
      }
      if (CurrentMenu == 14)  // Menu 14 Video Format
      {
        printf("Button Event %d, Entering Menu 14 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Vid Format Cancel\n");
          break;
        case 5:                               // 4:3
          SelectFormat(i);
          printf("4:3\n");
          break;
        case 6:                               // 16:9
          SelectFormat(i);
          printf("16:9\n");
          break;
        case 7:                               // 720p
          SelectFormat(i);
          printf("720p\n");
          break;
        case 8:                               // 1080p
          SelectFormat(i);
          printf("1080p\n");
          break;
        default:
          printf("Menu 14 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 14\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 14 action, go and wait for touch
      }
      if (CurrentMenu == 15)  // Menu 15 Video Source
      {
        printf("Button Event %d, Entering Menu 15 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Vid Format Cancel\n");
          break;
        case 5:                               // PiCam
          SelectSource(i);
          printf("PiCam\n");
          break;
        case 6:                               // CompVid
          SelectSource(i);
          printf("CompVid\n");
          break;
        case 7:                               // TCAnim
          SelectSource(i);
          printf("TCAnim\n");
          break;
        case 8:                               // TestCard
          SelectSource(i);
          printf("TestCard\n");
          break;
        case 9:                               // PiScreen
          SelectSource(i);
          printf("PiScreen\n");
          break;
        case 0:                               // Contest
          SelectSource(i);
          printf("Contest\n");
          break;
        case 1:                               // Webcam
          SelectSource(i);
          printf("Webcam\n");
          break;
        case 2:                               // C920
          SelectSource(i);
          printf("C920\n");
          break;
        default:
          printf("Menu 15 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 15\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 15 action, go and wait for touch
      }
      if (CurrentMenu == 16)  // Menu 16 Frequency
      {
        printf("Button Event %d, Entering Menu 16 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 0:                               // Freq 6
        case 1:                               // Freq 7
        case 2:                               // Freq 8
        case 3:                               // Freq 9
        case 5:                               // Freq 1
        case 6:                               // Freq 2
        case 7:                               // Freq 3
        case 8:                               // Freq 4
        case 9:                               // Freq 5
          SelectFreq(i);
          printf("Frequency Button %d\n", i);
          break;
        default:
          printf("Menu 16 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 16\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 16 action, go and wait for touch
      }
      if (CurrentMenu == 17)  // Menu 17 Symbol Rate
      {
        printf("Button Event %d, Entering Menu 17 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("SR Cancel\n");
          break;
        case 5:                               // SR 1
          SelectSR(i);
          printf("SR 1\n");
          break;
        case 6:                               // SR 2
          SelectSR(i);
          printf("SR 2\n");
          break;
        case 7:                               // SR 3
          SelectSR(i);
          printf("SR 3\n");
          break;
        case 8:                               // SR 4
          SelectSR(i);
          printf("SR 4\n");
          break;
        case 9:                               // SR 5
          SelectSR(i);
          printf("SR 5\n");
          break;
        case 0:                               // SR 6
          SelectSR(i);
          printf("SR 6\n");
          break;
        case 1:                               // SR 7
          SelectSR(i);
          printf("SR 7\n");
          break;
        case 2:                               // SR 8
          SelectSR(i);
          printf("SR 8\n");
          break;
        case 3:                               // SR 9
          SelectSR(i);
          printf("SR 8\n");
          break;
        default:
          printf("Menu 17 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 17\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 17 action, go and wait for touch
      }
      if (CurrentMenu == 18)  // Menu 18 FEC
      {
        printf("Button Event %d, Entering Menu 18 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // FEC 1/2
          SelectFec(i);
          printf("FEC 1/2\n");
          break;
        case 6:                               // FEC 2/3
          SelectFec(i);
          printf("FEC 2/3\n");
          break;
        case 7:                               // FEC 3/4
          SelectFec(i);
          printf("FEC 3/4\n");
          break;
        case 8:                               // FEC 5/6
          SelectFec(i);
          printf("FEC 5/6\n");
          break;
        case 9:                               // FEC 7/8
          SelectFec(i);
          printf("FEC 7/8\n");
          break;
        default:
          printf("Menu 18 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 18\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 18 action, go and wait for touch
      }
      if (CurrentMenu == 19)  // Menu 19 Transverter
      {
        printf("Button Event %d, Entering Menu 19 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // Band Direct
          SelectBand(i);
          printf("Band Direct\n");
          break;
        case 0:                               // Band t1
          SelectBand(i);
          printf("Band t1\n");
          break;
        case 1:                               // Band t2
          SelectBand(i);
          printf("Band t2\n");
          break;
        case 2:                               // Band t3
          SelectBand(i);
          printf("Band t3\n");
          break;
        case 3:                               // Band t4
          SelectBand(i);
          printf("Band t4\n");
          break;
        default:
          printf("Menu 19 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 19\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 19 action, go and wait for touch
      }
      // Menu 20 Level
      if (CurrentMenu == 21)  // Menu 21 EasyCap
      {
        printf("Button Event %d, Entering Menu 21 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("EasyCap Cancel\n");
          break;
        case 5:                               // Comp Vid
          SelectVidIP(i);
          printf("EasyCap Comp Vid\n");
          break;
        case 6:                               // S-Video
          SelectVidIP(i);
          printf("EasyCap S-Video\n");
          break;
         case 8:                               // PAL
          SelectSTD(i);
          printf("EasyCap PAL\n");
          break;
        case 9:                               // NTSC
          SelectSTD(i);
          printf("EasyCap NTSC\n");
          break;
        default:
          printf("Menu 21 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 21\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 21 action, go and wait for touch
      }
      if (CurrentMenu == 22)  // Menu 22 Caption
      {
        printf("Button Event %d, Entering Menu 22 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Caption Cancel\n");
          break;
        case 5:                               // Caption Off
          SelectCaption(i);
          printf("Caption Off\n");
          break;
        case 6:                               // Caption On
          SelectCaption(i);
          printf("Caption On\n");
          break;
        default:
          printf("Menu 22 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 22\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 22 action, go and wait for touch
      }
      if (CurrentMenu == 23)  // Menu 23 Audio
      {
        printf("Button Event %d, Entering Menu 23 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Audio Cancel\n");
          break;
        case 5:                               // Audio Auto
          SelectAudio(i);
          printf("Audio Auto\n");
          break;
        case 6:                               // Audio Mic
          SelectAudio(i);
          printf("Audio Mic\n");
          break;
        case 7:                               // Audio EasyCap
          SelectAudio(i);
          printf("Audio EasyCap\n");
          break;
        case 8:                               // Audio Bleeps
          SelectAudio(i);
          printf("Audio Bleeps\n");
          break;
        case 9:                               // Audio Off
          SelectAudio(i);
          printf("Audio Off\n");
          break;
        case 0:                               // Webcam
          SelectAudio(i);
          printf("Audio Webcam\n");
          break;
        default:
          printf("Menu 23 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 23\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 23 action, go and wait for touch
      }
      if (CurrentMenu == 24)  // Menu 24 Audio
      {
        printf("Button Event %d, Entering Menu 24 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Attenuator Cancel\n");
          break;
        case 5:                               // Attenuator NONE
          SelectAtten(i);
          printf("Attenuator None\n");
          break;
        case 6:                               // Attenuator PE4312
          SelectAtten(i);
          printf("Attenuator PE4312\n");
          break;
        case 7:                               // Attenuator PE43713
          SelectAtten(i);
          printf("Attenuator PE43713\n");
          break;
        case 8:                               // Attenuator HMC1119
          SelectAtten(i);
          printf("Attenuator HMC1119\n");
          break;
         default:
          printf("Menu 24 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 24\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 24 action, go and wait for touch
      }
      if (CurrentMenu == 26)  // Menu 26 Band Details
      {
        printf("Button Event %d, Entering Menu 26 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Band Details Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 26\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          printf("Changing Band Details %d\n", i);
          ChangeBandDetails(i);
          CurrentMenu=26;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu26();
          printf("Calling UpdateWindow\n");
          UpdateWindow();
          break;
        default:
          printf("Menu 26 Error\n");
        }
        // stay in Menu 26 to set another band
        continue;   // Completed Menu 26 action, go and wait for touch
      }
      if (CurrentMenu == 27)  // Menu 27 Preset Frequencies
      {
        printf("Button Event %d, Entering Menu 27 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Preset Frequency Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 27\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          printf("Changing Preset Freq Button %d\n", i);
          ChangePresetFreq(i);
          CurrentMenu=27;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu27();
          UpdateWindow();
          break;
        default:
          printf("Menu 27 Error\n");
        }
        // stay in Menu 27 if freq changed
        continue;   // Completed Menu 27 action, go and wait for touch
      }
      if (CurrentMenu == 28)  // Menu 28 Preset SRs
      {
        printf("Button Event %d, Entering Menu 28 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Preset SR Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 28\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
          printf("Changing Preset SR Button %d\n", i);
          ChangePresetSR(i);
          CurrentMenu=28;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu28();
          UpdateWindow();
          break;
        default:
          printf("Menu 28 Error\n");
        }
        // stay in Menu 28 if SR changed
        continue;   // Completed Menu 28 action, go and wait for touch
      }
      if (CurrentMenu == 29)  // Menu 29 Call, Locator and PIDs
      {
        printf("Button Event %d, Entering Menu 29 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Call/locator/PID Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 29\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
          printf("Changing PID\n");
          ChangePID(i);
          CurrentMenu=29;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 3:
          break;  // PCR PID can't be changed
        case 5:
          printf("Changing Call\n");
          ChangeCall();
          CurrentMenu=29;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        case 6:
          printf("Changing Locator\n");
          ChangeLocator();
          CurrentMenu=29;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu29();
          UpdateWindow();
          break;
        default:
          printf("Menu 29 Error\n");
        }
        // stay in Menu 29 if parameter changed
        continue;   // Completed Menu 29 action, go and wait for touch
      }

      if (CurrentMenu == 32)  // Menu 32 Select and Change ADF Reference Frequency
      {
        printf("Button Event %d, Entering Menu 32 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set or change ADFRef\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 32\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:
        case 1:
        case 2:
        //case 4:
        case 5:
        case 6:
        case 7:
          printf("Changing ADFRef\n");
          ChangeADFRef(i);
          CurrentMenu=32;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu32();
          UpdateWindow();
          break;
        default:
          printf("Menu 32 Error\n");
        }
        // stay in Menu 32 if parameter changed
        continue;   // Completed Menu 32 action, go and wait for touch
      }
      if (CurrentMenu == 33)  // Menu 33 Check for Update
      {
        printf("Button Event %d, Entering Menu 33 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Cancelling Check for Update\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 33\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:
        case 6:
        case 7:
          printf("Checking for Update\n");
          ExecuteUpdate(i);
          CurrentMenu=33;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu33();
          UpdateWindow();
          break;
        default:
          printf("Menu 33 Error\n");
        }
        // stay in Menu 33 if parameter changed
        continue;   // Completed Menu 33 action, go and wait for touch
      }
      if (CurrentMenu == 41)  // Menu 41 Keyboard (should not get here)
      {
        //break;
      }
    }   
  }
}

void Define_Menu1()
{
  int button = 0;
  color_t Green;
  color_t Blue;
  color_t Red;
  color_t Grey;
  strcpy(MenuTitle[1], "BATC Portsdown Transmitter Main Menu"); 

  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=255; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;

  // Frequency - Bottom Row, Menu 1

  button = CreateButton(1, 0);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 1);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 2);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 3);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(1, 4);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // 2nd Row, Menu 1.  EasyCap, Caption, Audio and Attenuator

  button = CreateButton(1, 5);                        // EasyCap
  AddButtonStatus(button, "EasyCap^not set", &Blue);
  AddButtonStatus(button, "EasyCap^not set", &Green);
  AddButtonStatus(button, "EasyCap^not set", &Grey);

  button = CreateButton(1, 6);                        // Caption
  AddButtonStatus(button, "Caption^not set", &Blue);
  AddButtonStatus(button, "Caption^not set", &Green);
  AddButtonStatus(button, "Caption^not set", &Grey);

  button = CreateButton(1, 7);                        // Audio
  AddButtonStatus(button, "Audio^not set", &Blue);
  AddButtonStatus(button, "Audio^not set", &Green);
  AddButtonStatus(button, "Audio^not set", &Grey);

  button = CreateButton(1, 8);                       // Attenuator
  AddButtonStatus(button, "Atten^not set", &Blue);
  AddButtonStatus(button, "Atten^not set", &Green);
  AddButtonStatus(button, "Atten^not set", &Grey);

  //button = CreateButton(1, 9);                       // Spare!
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  // Freq, SR, FEC, Transverter and Level - 3rd line up Menu 1

  button = CreateButton(1, 10);                        // Freq
  AddButtonStatus(button, " Freq ^not set", &Blue);
  AddButtonStatus(button, " Freq ^not set", &Green);
  AddButtonStatus(button, " Freq ^not set", &Grey);

  button = CreateButton(1, 11);                        // SR
  AddButtonStatus(button, "Sym Rate^not set", &Blue);
  AddButtonStatus(button, "Sym Rate^not set", &Green);
  AddButtonStatus(button, "Sym Rate^not set", &Grey);

  button = CreateButton(1, 12);                        // FEC
  AddButtonStatus(button, "  FEC  ^not set", &Blue);
  AddButtonStatus(button, "  FEC  ^not set", &Green);
  AddButtonStatus(button, "  FEC  ^not set", &Grey);

  button = CreateButton(1, 13);                        // Transverter
  AddButtonStatus(button,"Band/Tvtr^None",&Blue);
  AddButtonStatus(button,"Band/Tvtr^None",&Green);
  AddButtonStatus(button,"Band/Tvtr^None",&Grey);

  button = CreateButton(1, 14);                        // Level
  AddButtonStatus(button," Level^-",&Blue);
  AddButtonStatus(button," Level^-",&Green);
  AddButtonStatus(button," Level^-",&Grey);

  // Modulation, Vid Encoder, Output Device, Format and Source. 4th line up Menu 1

  button = CreateButton(1, 15);
  AddButtonStatus(button, "TX Mode ^not set", &Blue);
  AddButtonStatus(button, "TX Mode ^not set", &Green);
  AddButtonStatus(button, "TX Mode ^not set", &Grey);

  button = CreateButton(1, 16);
  AddButtonStatus(button, "Encoding^not set", &Blue);
  AddButtonStatus(button, "Encoding^not set", &Green);
  AddButtonStatus(button, "Encoder^not set", &Grey);

  button = CreateButton(1, 17);
  AddButtonStatus(button, "Output to^not set", &Blue);
  AddButtonStatus(button, "Output to^not set", &Green);
  AddButtonStatus(button, "Output to^not set", &Grey);

  button = CreateButton(1, 18);
  AddButtonStatus(button, "Format^not set", &Blue);
  AddButtonStatus(button, "Format^not set", &Green);
  AddButtonStatus(button, "Format^not set", &Grey);

  button = CreateButton(1, 19);
  AddButtonStatus(button, "Source^not set", &Blue);
  AddButtonStatus(button, "Source^not set", &Green);
  AddButtonStatus(button, "Source^not set", &Grey);

  //TRANSMIT RECEIVE BLANK MENU2 - Top of Menu 1

  button = CreateButton(1, 20);
  AddButtonStatus(button," TX  ",&Blue);
  AddButtonStatus(button,"TX ON",&Red);

  button = CreateButton(1, 21);
  AddButtonStatus(button," RX  ",&Blue);
  AddButtonStatus(button,"RX ON",&Green);

  // Button 22 not used

  button = CreateButton(1, 23);
  AddButtonStatus(button," M2  ",&Blue);
  AddButtonStatus(button," M2  ",&Green);
}

void Start_Highlights_Menu1()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight and text
{
  char Param[255];
  char Value[255];
  color_t Green;
  color_t Blue;
  color_t Red;
  color_t Grey;
  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=255; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;

  // Read the Config from file
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  printf("Entering Start_Highlights_Menu1\n");

  // Call to Check Grey buttons
  GreyOut1();

  // Presets Buttons 0 - 3
  
  char Presettext[63];

  strcpy(Presettext, "Preset 1^");
  strcat(Presettext, TabPresetLabel[0]);
  AmendButtonStatus(0, 0, Presettext, &Blue);
  AmendButtonStatus(0, 1, Presettext, &Green);
  AmendButtonStatus(0, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 2^");
  strcat(Presettext, TabPresetLabel[1]);
  AmendButtonStatus(1, 0, Presettext, &Blue);
  AmendButtonStatus(1, 1, Presettext, &Green);
  AmendButtonStatus(1, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 3^");
  strcat(Presettext, TabPresetLabel[2]);
  AmendButtonStatus(2, 0, Presettext, &Blue);
  AmendButtonStatus(2, 1, Presettext, &Green);
  AmendButtonStatus(2, 2, Presettext, &Grey);

  strcpy(Presettext, "Preset 4^");
  strcat(Presettext, TabPresetLabel[3]);
  AmendButtonStatus(3, 0, Presettext, &Blue);
  AmendButtonStatus(3, 1, Presettext, &Green);
  AmendButtonStatus(3, 2, Presettext, &Grey);

  // Set Preset Button 4

  strcpy(Presettext, "Store^Preset");
  AmendButtonStatus(4, 0, Presettext, &Blue);
  AmendButtonStatus(4, 1, Presettext, &Blue);
  AmendButtonStatus(4, 2, Presettext, &Red);
  AmendButtonStatus(4, 3, Presettext, &Red);

  // EasyCap Button 5

  char EasyCaptext[255];
  strcpy(Param,"analogcaminput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value," EasyCap Input");
  strcpy(EasyCaptext, "EasyCap^");
  if (strcmp(Value, "0") == 0)
  {
    strcat(EasyCaptext, "Comp Vid");
  }
  else
  {
    strcat(EasyCaptext, "S-Video");
  }
  AmendButtonStatus(5, 0, EasyCaptext, &Blue);
  AmendButtonStatus(5, 1, EasyCaptext, &Green);
  AmendButtonStatus(5, 2, EasyCaptext, &Grey);

  // Caption Button 6

  char Captiontext[255];
  strcpy(Param,"caption");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value," Caption State");
  strcpy(Captiontext, "Caption^");
  if (strcmp(Value, "off") == 0)
  {
    strcat(Captiontext, "Off");
  }
  else
  {
    strcat(Captiontext, "On");
  }
  AmendButtonStatus(6, 0, Captiontext, &Blue);
  AmendButtonStatus(6, 1, Captiontext, &Green);
  AmendButtonStatus(6, 2, Captiontext, &Grey);

  // Audio Button 7

  char Audiotext[255];
  strcpy(Param,"audio");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value," Audio Selection");
  if (strcmp(Value, "auto") == 0)
  {
    strcpy(Audiotext, "Audio^Auto");
  }
  else if (strcmp(Value, "mic") == 0)
  {
    strcpy(Audiotext, "Audio^USB Mic");
  }
  else if (strcmp(Value, "video") == 0)
  {
    strcpy(Audiotext, "Audio^EasyCap");
  }
  else if (strcmp(Value, "bleeps") == 0)
  {
    strcpy(Audiotext, "Audio^Bleeps");
  }
  else if (strcmp(Value, "webcam") == 0)
  {
    strcpy(Audiotext, "Audio^Webcam");
  }
  else
  {
    strcpy(Audiotext, "Audio^Off");
  }
  AmendButtonStatus(7, 0, Audiotext, &Blue);
  AmendButtonStatus(7, 1, Audiotext, &Green);
  AmendButtonStatus(7, 2, Audiotext, &Grey);

  // Attenuator Button 8

  char Attentext[255];
  strcpy(Value, "None");
  strcpy(Param,"attenuator");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n", Value, " Attenuator Selection");
  strcpy (Attentext, "Atten^");
  strcat (Attentext, Value);

  AmendButtonStatus(8, 0, Attentext, &Blue);
  AmendButtonStatus(8, 1, Attentext, &Green);
  AmendButtonStatus(8, 2, Attentext, &Grey);

  // Spare Button 9

  // Frequency Button 10

  char Freqtext[255];
  float TvtrFreq;
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Freq");
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    strcpy(Freqtext, "Freq^");
    strcat(Freqtext, Value);
    strcat(Freqtext, " MHz");
  }
  else
  {
    strcpy(Freqtext, "F: ");
    strcat(Freqtext, Value);
    strcat(Freqtext, "^T:");
    TvtrFreq = atof(Value) + TabBandLO[CurrentBand];
    if (TvtrFreq < 0)
    {
      TvtrFreq = TvtrFreq * -1;
    }
    snprintf(Value, 10, "%.1f", TvtrFreq);
    strcat(Freqtext, Value);
  }
  AmendButtonStatus(10, 0, Freqtext, &Blue);
  AmendButtonStatus(10, 1, Freqtext, &Green);
  AmendButtonStatus(10, 2, Freqtext, &Grey);

  // Symbol Rate Button 11

  char SRtext[255];
  strcpy(Param,"symbolrate");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"SR");
  strcpy(SRtext, "Sym Rate^ ");
  strcat(SRtext, Value);
  strcat(SRtext, " ");
  AmendButtonStatus(11, 0, SRtext, &Blue);
  AmendButtonStatus(11, 1, SRtext, &Green);
  AmendButtonStatus(11, 2, SRtext, &Grey);

  // FEC Button 12

  char FECtext[255];
  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  switch(fec)
  {
    case 1:strcpy(FECtext, "  FEC  ^  1/2 ") ;break;
    case 2:strcpy(FECtext, "  FEC  ^  2/3 ") ;break;
    case 3:strcpy(FECtext, "  FEC  ^  3/4 ") ;break;
    case 5:strcpy(FECtext, "  FEC  ^  5/6 ") ;break;
    case 7:strcpy(FECtext, "  FEC  ^  7/8 ") ;break;
  }
  AmendButtonStatus(12, 0, FECtext, &Blue);
  AmendButtonStatus(12, 1, FECtext, &Green);
  AmendButtonStatus(12, 2, FECtext, &Grey);

  // Band, Button 13
  char Bandtext[31]="Band/Tvtr^";
  strcpy(Param,"band");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  printf("Value=%s %s\n",Value,"Band");
  if (strcmp(Value, "d1") == 0)
  {
    strcat(Bandtext, TabBandLabel[0]);
  }
  if (strcmp(Value, "d2") == 0)
  {
    strcat(Bandtext, TabBandLabel[1]);
  }
  if (strcmp(Value, "d3") == 0)
  {
    strcat(Bandtext, TabBandLabel[2]);
  }
  if (strcmp(Value, "d4") == 0)
  {
    strcat(Bandtext, TabBandLabel[3]);
  }
  if (strcmp(Value, "d5") == 0)
  {
    strcat(Bandtext, TabBandLabel[4]);
  }
  if (strcmp(Value, "t1") == 0)
  {
    strcat(Bandtext, TabBandLabel[5]);
  }
  if (strcmp(Value, "t2") == 0)
  {
    strcat(Bandtext, TabBandLabel[6]);
  }
  if (strcmp(Value, "t3") == 0)
  {
    strcat(Bandtext, TabBandLabel[7]);
  }
  if (strcmp(Value, "t4") == 0)
  {
    strcat(Bandtext, TabBandLabel[8]);
  }
  AmendButtonStatus(13, 0, Bandtext, &Blue);
  AmendButtonStatus(13, 1, Bandtext, &Green);
  AmendButtonStatus(13, 2, Bandtext, &Grey);

  // Level, Button 14
  char Leveltext[15];
  if (strcmp(CurrentModeOP, "DATVEXPRESS") != 0)
  {
    snprintf(Leveltext, 20, "Att Level^%.2f", TabBandAttenLevel[CurrentBand]);
  }
  else
  {
    snprintf(Leveltext, 20, "Exp Level^%d", TabBandExpLevel[CurrentBand]);
  }
  AmendButtonStatus(14, 0, Leveltext, &Blue);
  AmendButtonStatus(14, 1, Leveltext, &Green);
  AmendButtonStatus(14, 2, Leveltext, &Grey);

  // TX Modulation Button 15

  char TXModetext[255];
  strcpy(TXModetext, "Modulation^");
  strcat(TXModetext, CurrentTXMode);
  AmendButtonStatus(15, 0, TXModetext, &Blue);
  AmendButtonStatus(15, 1, TXModetext, &Green);
  AmendButtonStatus(15, 2, TXModetext, &Grey);

  // Encoding Button 16

  char Encodingtext[255];
  strcpy(Encodingtext, "Encoder^ ");
  strcat(Encodingtext, CurrentEncoding);
  strcat(Encodingtext, " ");
  AmendButtonStatus(16, 0, Encodingtext, &Blue);
  AmendButtonStatus(16, 1, Encodingtext, &Green);
  AmendButtonStatus(16, 2, Encodingtext, &Grey);

  // Output Device Button 17

  char Outputtext[255];
  strcpy(Outputtext, "Output to^");
  strcat(Outputtext, CurrentModeOPtext);
  AmendButtonStatus(17, 0, Outputtext, &Blue);
  AmendButtonStatus(17, 1, Outputtext, &Green);
  AmendButtonStatus(17, 2, Outputtext, &Grey);

  // Video Format Button 18

  char Formattext[255];
  strcpy(Formattext, "Format^ ");
  strcat(Formattext, CurrentFormat);
  strcat(Formattext, " ");
  AmendButtonStatus(18, 0, Formattext, &Blue);
  AmendButtonStatus(18, 1, Formattext, &Green);
  AmendButtonStatus(18, 2, Formattext, &Grey);

  // Video Source Button 19

  char Sourcetext[255];
  strcpy(Sourcetext, "Source^");
  strcat(Sourcetext, CurrentSource);
  //strcat(Sourcetext, " ");
  AmendButtonStatus(19, 0, Sourcetext, &Blue);
  AmendButtonStatus(19, 1, Sourcetext, &Green);
  AmendButtonStatus(19, 2, Sourcetext, &Grey);
}

void Define_Menu2()
{
  int button;
  color_t Green;
  color_t Blue;
  //color_t Black;

  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  //Black.r=0; Black.g=0; Black.b=0;

  strcpy(MenuTitle[2], "BATC Portsdown Transmitter Menu 2"); 

  // Bottom Row, Menu 2

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Shutdown^ ", &Blue);
  AddButtonStatus(button, "Shutdown^ ", &Green);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "Reboot^ ", &Blue);
  AddButtonStatus(button, "Reboot^ ", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "Info^ ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 3);
  AddButtonStatus(button, "Calibrate^Touch", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 4);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  // 2nd Row, Menu 2

  button = CreateButton(2, 5);
  AddButtonStatus(button, "Receive^144.75", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "Receive^145.8", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 7);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 8);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 9);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  // 3rd line up Menu 2

  button = CreateButton(2, 10);
  AddButtonStatus(button, "Video^Snap", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 11);
  AddButtonStatus(button, "Video^View", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 12);
  AddButtonStatus(button, "Snap^Check", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 13);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 14);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  // 4th line up Menu 2

  button = CreateButton(2, 15);
  AddButtonStatus(button, "Freq Show^Spectrum", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 16);
  AddButtonStatus(button, "Sig Gen^ ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(2, 17);
  AddButtonStatus(button, "RTL-TCP^Server", &Blue);
  //AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 18);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  //button = CreateButton(2, 19);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // Top of Menu 2

  //button = CreateButton(2, 20);
  //AddButtonStatus(button, "FreqShow^Spectrum", &Blue);

  //button = CreateButton(2, 21);
  //AddButtonStatus(button, "", &Blue);

  button = CreateButton(2, 22);
  AddButtonStatus(button," M1  ",&Blue);
  AddButtonStatus(button," M1  ",&Green);

  button = CreateButton(2, 23);
  AddButtonStatus(button," M3  ",&Blue);
  AddButtonStatus(button," M3  ",&Green);
}

void Start_Highlights_Menu2()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  /* char Param[255];
  char Value[255];

  // Audio Input

  strcpy(Param,"audio");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Audio");
  if(strcmp(Value,"mic")==0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 5, 1);
  }
  if(strcmp(Value, "auto") == 0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 6, 1);
  }
  if(strcmp(Value,"video") == 0)
  {
    SelectInGroup(25 + 5, 25 + 7, 25 + 7, 1);
  } */
}

void Define_Menu3()
{
  int button;
  color_t Green;
  color_t Blue;

  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  strcpy(MenuTitle[3], "Menu 3 Portsdown Configuration");

  // Bottom Line Menu 3: Check for Update

  button = CreateButton(3, 0);
  AddButtonStatus(button, "Check for^Update", &Blue);

  // 4th line up Menu 3: Band Details, Preset Freqs, Preset SRs

  button = CreateButton(3, 15);
  AddButtonStatus(button, "Set Band^Details", &Blue);
  AddButtonStatus(button, "Set Band^Details", &Green);

  button = CreateButton(3, 16);
  AddButtonStatus(button, "Set Preset^Freqs", &Blue);
  AddButtonStatus(button, "Set Preset^Freqs", &Green);

  button = CreateButton(3, 17);
  AddButtonStatus(button, "Set Preset^SRs", &Blue);
  AddButtonStatus(button, "Set Preset^SRs", &Green);

  button = CreateButton(3, 18);
  AddButtonStatus(button, "Set Call,^Loc & PIDs", &Blue);
  AddButtonStatus(button, "Set Call,^Loc & PIDs", &Green);

  button = CreateButton(3, 19);
  AddButtonStatus(button, "Set ADF^Ref Freq", &Blue);
  AddButtonStatus(button, "Set ADF^Ref Freq", &Green);

  // Top of Menu 3

  button = CreateButton(3, 22);
  AddButtonStatus(button," M1  ",&Blue);
}

void Start_Highlights_Menu3()
// Retrieves stored value for each group of buttons
// and then sets the correct highlight
{
  ;
  //char Param[255];
  //char Value[255];
  //int STD=1;
}

void Define_Menu11()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[11], "Modulation Selection Menu (11)"); 

  // Bottom Row, Menu 11

  button = CreateButton(11, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 11

  button = CreateButton(11, 5);
  AddButtonStatus(button, TabTXMode[0], &Blue);
  AddButtonStatus(button, TabTXMode[0], &Green);

  button = CreateButton(11, 6);
  AddButtonStatus(button, TabTXMode[1], &Blue);
  AddButtonStatus(button, TabTXMode[1], &Green);
}

void Start_Highlights_Menu11()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentTXMode, TabTXMode[0])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 5, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[1])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 6, 1);
  }
}

void Define_Menu12()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[12], "Encoding Selection Menu (12)"); 

  // Bottom Row, Menu 12

  button = CreateButton(12, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 12

  button = CreateButton(12, 5);
  AddButtonStatus(button, TabEncoding[0], &Blue);
  AddButtonStatus(button, TabEncoding[0], &Green);

  button = CreateButton(12, 6);
  AddButtonStatus(button, TabEncoding[1], &Blue);
  AddButtonStatus(button, TabEncoding[1], &Green);

  button = CreateButton(12, 7);
  AddButtonStatus(button, TabEncoding[2], &Blue);
  AddButtonStatus(button, TabEncoding[2], &Green);

  button = CreateButton(12, 8);
  AddButtonStatus(button, TabEncoding[3], &Blue);
  AddButtonStatus(button, TabEncoding[3], &Green);
}

void Start_Highlights_Menu12()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentEncoding, TabEncoding[0]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 8, 5, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[1]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 8, 6, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[2]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 8, 7, 1);
  }
  if(strcmp(CurrentEncoding, TabEncoding[3]) == 0)
  {
    SelectInGroupOnMenu(12, 5, 8, 8, 1);
  }
}

void Define_Menu13()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[13], "Output Device Menu (13)"); 

  // Bottom Row, Menu 13

  button = CreateButton(13, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(13, 0);
  AddButtonStatus(button, TabModeOPtext[5], &Blue);
  AddButtonStatus(button, TabModeOPtext[5], &Green);

  button = CreateButton(13, 1);
  AddButtonStatus(button, TabModeOPtext[6], &Blue);
  AddButtonStatus(button, TabModeOPtext[6], &Green);

  button = CreateButton(13, 2);
  AddButtonStatus(button, TabModeOPtext[7], &Blue);
  AddButtonStatus(button, TabModeOPtext[7], &Green);

  // 2nd Row, Menu 13

  button = CreateButton(13, 5);
  AddButtonStatus(button, TabModeOPtext[0], &Blue);
  AddButtonStatus(button, TabModeOPtext[0], &Green);

  button = CreateButton(13, 6);
  AddButtonStatus(button, TabModeOPtext[1], &Blue);
  AddButtonStatus(button, TabModeOPtext[1], &Green);

  button = CreateButton(13, 7);
  AddButtonStatus(button, TabModeOPtext[2], &Blue);
  AddButtonStatus(button, TabModeOPtext[2], &Green);

  button = CreateButton(13, 8);
  AddButtonStatus(button, TabModeOPtext[3], &Blue);
  AddButtonStatus(button, TabModeOPtext[3], &Green);

  button = CreateButton(13, 9);
  AddButtonStatus(button, TabModeOPtext[4], &Blue);
  AddButtonStatus(button, TabModeOPtext[4], &Green);
}

void Start_Highlights_Menu13()
{
  if(strcmp(CurrentModeOP, TabModeOP[0]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 5, 1);
    SelectInGroupOnMenu(13, 0, 2, 5, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[1]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 6, 1);
    SelectInGroupOnMenu(13, 0, 2, 6, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[2]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 7, 1);
    SelectInGroupOnMenu(13, 0, 2, 7, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[3]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 8, 1);
    SelectInGroupOnMenu(13, 0, 2, 8, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[4]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 9, 1);
    SelectInGroupOnMenu(13, 0, 2, 9, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[5]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 0, 1);
    SelectInGroupOnMenu(13, 0, 2, 0, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[6]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 1, 1);
    SelectInGroupOnMenu(13, 0, 2, 1, 1);
  }
  if(strcmp(CurrentModeOP, TabModeOP[7]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 2, 1);
    SelectInGroupOnMenu(13, 0, 2, 2, 1);
  }
}

void Define_Menu14()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[14], "Video Format Menu (14)"); 

  // Bottom Row, Menu 14

  button = CreateButton(14, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 14

  button = CreateButton(14, 5);
  AddButtonStatus(button, TabFormat[0], &Blue);
  AddButtonStatus(button, TabFormat[0], &Green);

  button = CreateButton(14, 6);
  AddButtonStatus(button, TabFormat[1], &Blue);
  AddButtonStatus(button, TabFormat[1], &Green);

  button = CreateButton(14, 7);
  AddButtonStatus(button, TabFormat[2], &Blue);
  AddButtonStatus(button, TabFormat[2], &Green);

  button = CreateButton(14, 8);
  AddButtonStatus(button, TabFormat[3], &Blue);
  AddButtonStatus(button, TabFormat[3], &Green);
}

void Start_Highlights_Menu14()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  if(strcmp(CurrentFormat, TabFormat[0]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 5, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[1]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 6, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[2]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 7, 1);
  }
  if(strcmp(CurrentFormat, TabFormat[3]) == 0)
  {
    SelectInGroupOnMenu(14, 5, 8, 8, 1);
  }
}

void Define_Menu15()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t Grey;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Grey.r=127; Grey.g=127; Grey.b=127;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[15], "Video Source Menu (15)"); 

  // Bottom Row, Menu 15
  button = CreateButton(15, 0);
  AddButtonStatus(button, TabSource[5], &Blue);
  AddButtonStatus(button, TabSource[5], &Green);
  AddButtonStatus(button, TabSource[5], &Grey);

  button = CreateButton(15, 1);
  AddButtonStatus(button, TabSource[6], &Blue);
  AddButtonStatus(button, TabSource[6], &Green);
  AddButtonStatus(button, TabSource[6], &Grey);

  button = CreateButton(15, 2);
  AddButtonStatus(button, TabSource[7], &Blue);
  AddButtonStatus(button, TabSource[7], &Green);
  AddButtonStatus(button, TabSource[7], &Grey);

  button = CreateButton(15, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 15

  button = CreateButton(15, 5);
  AddButtonStatus(button, TabSource[0], &Blue);
  AddButtonStatus(button, TabSource[0], &Green);
  AddButtonStatus(button, TabSource[0], &Grey);

  button = CreateButton(15, 6);
  AddButtonStatus(button, TabSource[1], &Blue);
  AddButtonStatus(button, TabSource[1], &Green);
  AddButtonStatus(button, TabSource[1], &Grey);

  button = CreateButton(15, 7);
  AddButtonStatus(button, TabSource[2], &Blue);
  AddButtonStatus(button, TabSource[2], &Green);
  AddButtonStatus(button, TabSource[2], &Grey);

  button = CreateButton(15, 8);
  AddButtonStatus(button, TabSource[3], &Blue);
  AddButtonStatus(button, TabSource[3], &Green);
  AddButtonStatus(button, TabSource[3], &Grey);

  button = CreateButton(15, 9);
  AddButtonStatus(button, TabSource[4], &Blue);
  AddButtonStatus(button, TabSource[4], &Green);
  AddButtonStatus(button, TabSource[4], &Grey);
}

void Start_Highlights_Menu15()
{
  char vcoding[256];
  char vsource[256];
  ReadModeInput(vcoding, vsource);

  // Call to GreyOut inappropriate buttons
  GreyOut15();

  if(strcmp(CurrentSource, TabSource[0]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 5, 1);
    SelectInGroupOnMenu(15, 0, 2, 5, 1);
  }
  if(strcmp(CurrentSource, TabSource[1]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 6, 1);
    SelectInGroupOnMenu(15, 0, 2, 6, 1);
  }
  if(strcmp(CurrentSource, TabSource[2]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 7, 1);
    SelectInGroupOnMenu(15, 0, 2, 7, 1);
  }
  if(strcmp(CurrentSource, TabSource[3]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 8, 1);
    SelectInGroupOnMenu(15, 0, 2, 8, 1);
  }
  if(strcmp(CurrentSource, TabSource[4]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 9, 1);
    SelectInGroupOnMenu(15, 0, 2, 9, 1);
  }
  if(strcmp(CurrentSource, TabSource[5]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 0, 1);
    SelectInGroupOnMenu(15, 0, 2, 0, 1);
  }
  if(strcmp(CurrentSource, TabSource[6]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 1, 1);
    SelectInGroupOnMenu(15, 0, 2, 1, 1);
  }
  if(strcmp(CurrentSource, TabSource[7]) == 0)
  {
    SelectInGroupOnMenu(15, 5, 9, 2, 1);
    SelectInGroupOnMenu(15, 0, 2, 2, 1);
  }
}

void Define_Menu16()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  float TvtrFreq;
  char Freqtext[31];
  char Value[31];

  strcpy(MenuTitle[16], "Frequency Selection Menu (16)"); 

  // Bottom Row, Menu 16


  if ((TabBandLO[5] < 0.1) && (TabBandLO[5] > -0.1))
  {
    strcpy(Freqtext, FreqLabel[5]);
  }
  else
  {
    strcpy(Freqtext, "F: ");
    strcat(Freqtext, TabFreq[5]);
    strcat(Freqtext, "^T:");
    TvtrFreq = atof(TabFreq[5]) + TabBandLO[CurrentBand];
    if (TvtrFreq < 0)
    {
      TvtrFreq = TvtrFreq * -1;
    }
    snprintf(Value, 10, "%.1f", TvtrFreq);
    strcat(Freqtext, Value);
  }
 
  button = CreateButton(16, 0);
  AddButtonStatus(button, "test", &Blue);
  AddButtonStatus(button, Freqtext, &Green);

  button = CreateButton(16, 1);
  AddButtonStatus(button, FreqLabel[6], &Blue);
  AddButtonStatus(button, FreqLabel[6], &Green);

  button = CreateButton(16, 2);
  AddButtonStatus(button, FreqLabel[7], &Blue);
  AddButtonStatus(button, FreqLabel[7], &Green);

  button = CreateButton(16, 3);
  AddButtonStatus(button, FreqLabel[8], &Blue);
  AddButtonStatus(button, FreqLabel[8], &Green);

  button = CreateButton(16, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 16

  button = CreateButton(16, 5);
  AddButtonStatus(button, FreqLabel[0], &Blue);
  AddButtonStatus(button, FreqLabel[0], &Green);

  button = CreateButton(16, 6);
  AddButtonStatus(button, FreqLabel[1], &Blue);
  AddButtonStatus(button, FreqLabel[1], &Green);

  button = CreateButton(16, 7);
  AddButtonStatus(button, FreqLabel[2], &Blue);
  AddButtonStatus(button, FreqLabel[2], &Green);

  button = CreateButton(16, 8);
  AddButtonStatus(button, FreqLabel[3], &Blue);
  AddButtonStatus(button, FreqLabel[3], &Green);

  button = CreateButton(16, 9);
  AddButtonStatus(button, FreqLabel[4], &Blue);
  AddButtonStatus(button, FreqLabel[4], &Green);
}

void MakeFreqText(int index)
{
  char Param[255];
  char Value[255];
  float TvtrFreq;

  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if ((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1))
  {
    strcpy(FreqBtext, FreqLabel[index]);
  }
  else
  {
    strcpy(FreqBtext, "F: ");
    strcat(FreqBtext, TabFreq[index]);
    strcat(FreqBtext, "^T:");
    TvtrFreq = atof(TabFreq[index]) + TabBandLO[CurrentBand];
    if (TvtrFreq < 0)
    {
      TvtrFreq = TvtrFreq * -1;
    }
    snprintf(Value, 10, "%.1f", TvtrFreq);
    strcat(FreqBtext, Value);
  }
}

void Start_Highlights_Menu16()
{
  // Frequency
  char Param[255];
  char Value[255];
  int index;
  int NoButton;
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  // Update info in memory
  ReadPresets();

  // Look up current frequency for highlighting
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG,Param,Value);

  for(index = 0; index < 9 ; index = index + 1)
  {
    // Define the button text
    MakeFreqText(index);
    NoButton = index + 5; // Valid for bottom row
    if (index > 4)          // Overwrite for top row
    {
      NoButton = index - 5;
    }
    AmendButtonStatus(ButtonNumber(16, NoButton), 0, FreqBtext, &Blue);
    AmendButtonStatus(ButtonNumber(16, NoButton), 1, FreqBtext, &Green);

    //Highlight the Current Button
    if(strcmp(Value, TabFreq[index]) == 0)
    {
      SelectInGroupOnMenu(16, 5, 9, NoButton, 1);
      SelectInGroupOnMenu(16, 0, 3, NoButton, 1);
    }
  }
}

void Define_Menu17()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[17], "Symbol Rate Selection Menu (17)"); 

  // Bottom Row, Menu 17

  button = CreateButton(17, 0);
  AddButtonStatus(button, SRLabel[5], &Blue);
  AddButtonStatus(button, SRLabel[5], &Green);

  button = CreateButton(17, 1);
  AddButtonStatus(button, SRLabel[6], &Blue);
  AddButtonStatus(button, SRLabel[6], &Green);

  button = CreateButton(17, 2);
  AddButtonStatus(button, SRLabel[7], &Blue);
  AddButtonStatus(button, SRLabel[7], &Green);

  button = CreateButton(17, 3);
  AddButtonStatus(button, SRLabel[8], &Blue);
  AddButtonStatus(button, SRLabel[8], &Green);

  button = CreateButton(17, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 17

  button = CreateButton(17, 5);
  AddButtonStatus(button, SRLabel[0], &Blue);
  AddButtonStatus(button, SRLabel[0], &Green);

  button = CreateButton(17, 6);
  AddButtonStatus(button, SRLabel[1], &Blue);
  AddButtonStatus(button, SRLabel[1], &Green);

  button = CreateButton(17, 7);
  AddButtonStatus(button, SRLabel[2], &Blue);
  AddButtonStatus(button, SRLabel[2], &Green);

  button = CreateButton(17, 8);
  AddButtonStatus(button, SRLabel[3], &Blue);
  AddButtonStatus(button, SRLabel[3], &Green);

  button = CreateButton(17, 9);
  AddButtonStatus(button, SRLabel[4], &Blue);
  AddButtonStatus(button, SRLabel[4], &Green);
}

void Start_Highlights_Menu17()
{
  // Symbol Rate
  char Param[255];
  char Value[255];
  int SR;

  strcpy(Param,"symbolrate");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  SR=atoi(Value);
  printf("Value=%s %s\n",Value,"SR");

  if ( SR == TabSR[0] )
  {
    SelectInGroupOnMenu(17, 0, 3, 5, 1);
    SelectInGroupOnMenu(17, 5, 9, 5, 1);
  }
  else if ( SR == TabSR[1] )
  {
    SelectInGroupOnMenu(17, 0, 3, 6, 1);
    SelectInGroupOnMenu(17, 5, 9, 6, 1);
  }
  else if ( SR == TabSR[2] )
  {
    SelectInGroupOnMenu(17, 0, 3, 7, 1);
    SelectInGroupOnMenu(17, 5, 9, 7, 1);
  }
  else if ( SR == TabSR[3] )
  {
    SelectInGroupOnMenu(17, 0, 3, 8, 1);
    SelectInGroupOnMenu(17, 5, 9, 8, 1);
  }
  else if ( SR == TabSR[4] )
  {
    SelectInGroupOnMenu(17, 0, 3, 9, 1);
    SelectInGroupOnMenu(17, 5, 9, 9, 1);
  }
  else if ( SR == TabSR[5] )
  {
    SelectInGroupOnMenu(17, 0, 3, 0, 1);
    SelectInGroupOnMenu(17, 5, 9, 0, 1);
  }
  else if ( SR == TabSR[6] )
  {
    SelectInGroupOnMenu(17, 0, 3, 1, 1);
    SelectInGroupOnMenu(17, 5, 9, 1, 1);
  }
  else if ( SR == TabSR[7] )
  {
    SelectInGroupOnMenu(17, 0, 3, 2, 1);
    SelectInGroupOnMenu(17, 5, 9, 2, 1);
  }
  else if ( SR == TabSR[8] )
  {
    SelectInGroupOnMenu(17, 0, 3, 3, 1);
    SelectInGroupOnMenu(17, 5, 9, 3, 1);
  }
}

void Define_Menu18()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[18], "FEC Selection Menu (18)"); 

  // Bottom Row, Menu 18

  button = CreateButton(18, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 18

  button = CreateButton(18, 5);
  AddButtonStatus(button,"FEC 1/2",&Blue);
  AddButtonStatus(button,"FEC 1/2",&Green);

  button = CreateButton(18, 6);
  AddButtonStatus(button,"FEC 2/3",&Blue);
  AddButtonStatus(button,"FEC 2/3",&Green);

  button = CreateButton(18, 7);
  AddButtonStatus(button,"FEC 3/4",&Blue);
  AddButtonStatus(button,"FEC 3/4",&Green);

  button = CreateButton(18, 8);
  AddButtonStatus(button,"FEC 5/6",&Blue);
  AddButtonStatus(button,"FEC 5/6",&Green);

  button = CreateButton(18, 9);
  AddButtonStatus(button,"FEC 7/8",&Blue);
  AddButtonStatus(button,"FEC 7/8",&Green);
}

void Start_Highlights_Menu18()
{
  // FEC
  char Param[255];
  char Value[255];
  int fec;

  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  switch(fec)
  {
    //void SelectInGroupOnMenu(int Menu, int StartButton, int StopButton, int NumberButton, int Status)

    case 1:SelectInGroupOnMenu(18, 5, 9, 5, 1);
    break;
    case 2:SelectInGroupOnMenu(18, 5, 9, 6, 1);
    break;
    case 3:SelectInGroupOnMenu(18, 5, 9, 7, 1);
    break;
    case 5:SelectInGroupOnMenu(18, 5, 9, 8, 1);
    break;
    case 7:SelectInGroupOnMenu(18, 5, 9, 9, 1);
    break;
  }
}

void Define_Menu19()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[19], "Transverter Selection Menu (19)"); 

  // Bottom Row, Menu 19

  button = CreateButton(19, 0);
  AddButtonStatus(button, TabBandLabel[5], &Blue);
  AddButtonStatus(button, TabBandLabel[5], &Green);

  button = CreateButton(19, 1);
  AddButtonStatus(button, TabBandLabel[6], &Blue);
  AddButtonStatus(button, TabBandLabel[6], &Green);

  button = CreateButton(19, 2);
  AddButtonStatus(button, TabBandLabel[7], &Blue);
  AddButtonStatus(button, TabBandLabel[7], &Green);

  button = CreateButton(19, 3);
  AddButtonStatus(button, TabBandLabel[8], &Blue);
  AddButtonStatus(button, TabBandLabel[8], &Green);

  button = CreateButton(19, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 19

  button = CreateButton(19, 5);
  AddButtonStatus(button,"Direct",&Blue);
  AddButtonStatus(button,"Direct",&Green);
}

void Start_Highlights_Menu19()
{
  // Band/Transverter Select
  char Param[255];
  char Value[255];
  int NoButton;
  char BandLabel[31];
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  strcpy(Param,"band");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Band");

  //Set default of Direct and then test for transverters
  // and overwrite if required
  SelectInGroupOnMenu(19, 5, 5, 5, 1);
  SelectInGroupOnMenu(19, 0, 3, 5, 1);

  if (strcmp(Value, "t1") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 0, 1);
    SelectInGroupOnMenu(19, 0, 3, 0, 1);
  }
  if (strcmp(Value, "t2") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 1, 1);
    SelectInGroupOnMenu(19, 0, 3, 1, 1);
  }
  if (strcmp(Value, "t3") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 2, 1);
    SelectInGroupOnMenu(19, 0, 3, 2, 1);
  }
  if (strcmp(Value, "t4") == 0)
  {
    SelectInGroupOnMenu(19, 5, 5, 3, 1);
    SelectInGroupOnMenu(19, 0, 3, 3, 1);
  }

  // Set transverter Labels

  for (NoButton = 0; NoButton < 4; NoButton = NoButton + 1)
  {
    strcpy(BandLabel, "Transvtr^");
    strcat(BandLabel, TabBandLabel[NoButton + 5]);
    AmendButtonStatus(ButtonNumber(19, NoButton), 0, BandLabel, &Blue);
    AmendButtonStatus(ButtonNumber(19, NoButton), 1, BandLabel, &Green);
  }
}

// Menu 20 Level

void Define_Menu21()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[21], "EasyCap Video Input Menu (21)"); 

  // Bottom Row, Menu 21

  button = CreateButton(21, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 21

  button = CreateButton(21, 5);
  AddButtonStatus(button,"Comp Vid",&Blue);
  AddButtonStatus(button,"Comp Vid",&Green);

  button = CreateButton(21, 6);
  AddButtonStatus(button,"S-Video",&Blue);
  AddButtonStatus(button,"S-Video",&Green);

  button = CreateButton(21, 8);
  AddButtonStatus(button," PAL ",&Blue);
  AddButtonStatus(button," PAL ",&Green);

  button = CreateButton(21, 9);
  AddButtonStatus(button," NTSC ",&Blue);
  AddButtonStatus(button," NTSC ",&Green);
}

void Start_Highlights_Menu21()
{
  // EasyCap
  ReadModeEasyCap();
  if (strcmp(ModeVidIP, "0") == 0)
  {
    SelectInGroupOnMenu(21, 5, 6, 5, 1);
  }
  else
  {
    SelectInGroupOnMenu(21, 5, 6, 6, 1);
  }
  if (strcmp(ModeSTD, "6") == 0)
  {
    SelectInGroupOnMenu(21, 8, 9, 8, 1);
  }
  else
  {
    SelectInGroupOnMenu(21, 8, 9, 9, 1);
  }
}

void Define_Menu22()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[22], "Caption Selection Menu (22)"); 

  // Bottom Row, Menu 22

  button = CreateButton(22, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 22

  button = CreateButton(22, 5);
  AddButtonStatus(button,"Caption^Off",&Blue);
  AddButtonStatus(button,"Caption^Off",&Green);

  button = CreateButton(22, 6);
  AddButtonStatus(button,"Caption^On",&Blue);
  AddButtonStatus(button,"Caption^On",&Green);
}

void Start_Highlights_Menu22()
{
  // Caption
  ReadCaptionState();
  if (strcmp(CurrentCaptionState, "off") == 0)
  {
    SelectInGroupOnMenu(22, 5, 6, 5, 1);
  }
  else
  {
    SelectInGroupOnMenu(22, 5, 6, 6, 1);
  }
}

void Define_Menu23()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[23], "Audio Input Selection Menu (23)"); 

  // Bottom Row, Menu 23

  button = CreateButton(23, 0);
  AddButtonStatus(button,"Webcam",&Blue);
  AddButtonStatus(button,"Webcam",&Green);

  button = CreateButton(23, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 23

  button = CreateButton(23, 5);
  AddButtonStatus(button,"Auto",&Blue);
  AddButtonStatus(button,"Auto",&Green);

  button = CreateButton(23, 6);
  AddButtonStatus(button,"USB Mic",&Blue);
  AddButtonStatus(button,"USB Mic",&Green);

  button = CreateButton(23, 7);
  AddButtonStatus(button,"EasyCap",&Blue);
  AddButtonStatus(button,"EasyCap",&Green);

  button = CreateButton(23, 8);
  AddButtonStatus(button,"Bleeps",&Blue);
  AddButtonStatus(button,"Bleeps",&Green);

  button = CreateButton(23, 9);
  AddButtonStatus(button,"Audio^Off",&Blue);
  AddButtonStatus(button,"Audio^Off",&Green);
}

void Start_Highlights_Menu23()
{
  // Audio
  ReadAudioState();
  if (strcmp(CurrentAudioState, "auto") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 5, 1);
    SelectInGroupOnMenu(23, 0, 0, 5, 1);
  }
  else if (strcmp(CurrentAudioState, "mic") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 6, 1);
    SelectInGroupOnMenu(23, 0, 0, 6, 1);
  }
  else if (strcmp(CurrentAudioState, "video") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 7, 1);
    SelectInGroupOnMenu(23, 0, 0, 7, 1);
  }
  else if (strcmp(CurrentAudioState, "bleeps") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 8, 1);
    SelectInGroupOnMenu(23, 0, 0, 8, 1);
  }
  else if (strcmp(CurrentAudioState, "webcam") == 0)
  {
    SelectInGroupOnMenu(23, 5, 9, 0, 1);
    SelectInGroupOnMenu(23, 0, 0, 0, 1);
  }
  else
  {
    SelectInGroupOnMenu(23, 5, 9, 9, 1);
    SelectInGroupOnMenu(23, 0, 0, 9, 1);
  }
}

void Define_Menu24()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[24], "Attenuator Selection Menu (24)"); 

  // Bottom Row, Menu 24

  button = CreateButton(24, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 24

  button = CreateButton(24, 5);
  AddButtonStatus(button,"None^Fitted",&Blue);
  AddButtonStatus(button,"None^Fitted",&Green);

  button = CreateButton(24, 6);
  AddButtonStatus(button,"PE4312",&Blue);
  AddButtonStatus(button,"PE4312",&Green);

  button = CreateButton(24, 7);
  AddButtonStatus(button,"PE43713",&Blue);
  AddButtonStatus(button,"PE43713",&Green);

  button = CreateButton(24, 8);
  AddButtonStatus(button,"HMC1119",&Blue);
  AddButtonStatus(button,"HMC1119",&Green);
}

void Start_Highlights_Menu24()
{
  // Audio
  ReadAttenState();
  if (strcmp(CurrentAtten, "NONE") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 5, 1);
  }
  else if (strcmp(CurrentAtten, "PE4312") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 6, 1);
  }
  else if (strcmp(CurrentAtten, "PE43713") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 7, 1);
  }
  else if (strcmp(CurrentAtten, "HMC1119") == 0)
  {
    SelectInGroupOnMenu(24, 5, 9, 8, 1);
  }
  else
  {
    SelectInGroupOnMenu(24, 5, 9, 5, 1);
  }
}

void Define_Menu26()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  char BandLabel[31];

  strcpy(MenuTitle[26], "Band Details Setting Menu (26)"); 

  // Bottom Row, Menu 26

  button = CreateButton(26, 0);
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[5]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 1);
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[6]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 2);
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[7]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 3);
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[8]);
  AddButtonStatus(button, BandLabel, &Blue);
  AddButtonStatus(button, BandLabel, &Green);

  button = CreateButton(26, 4);
  AddButtonStatus(button, "Exit", &Blue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 26

  button = CreateButton(26, 5);
  AddButtonStatus(button, TabBandLabel[0], &Blue);
  AddButtonStatus(button, TabBandLabel[0], &Green);

  button = CreateButton(26, 6);
  AddButtonStatus(button, TabBandLabel[1], &Blue);
  AddButtonStatus(button, TabBandLabel[1], &Green);

  button = CreateButton(26, 7);
  AddButtonStatus(button, TabBandLabel[2], &Blue);
  AddButtonStatus(button, TabBandLabel[2], &Green);

  button = CreateButton(26, 8);
  AddButtonStatus(button, TabBandLabel[3], &Blue);
  AddButtonStatus(button, TabBandLabel[3], &Green);

  button = CreateButton(26, 9);
  AddButtonStatus(button, TabBandLabel[4], &Blue);
  AddButtonStatus(button, TabBandLabel[4], &Green);
}

void Start_Highlights_Menu26()
{
  // Set Band Select Labels
  int NoButton;
  char BandLabel[31];
  color_t Green;
  color_t Blue;
  //color_t LBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  //LBlue.r=64; LBlue.g=64; LBlue.b=192;

  printf("Entering Start Highlights Menu26\n");

  for (NoButton = 0; NoButton < 4; NoButton = NoButton + 1)
  {
    strcpy(BandLabel, "Transvtr^");
    strcat(BandLabel, TabBandLabel[NoButton + 5]);
    AmendButtonStatus(ButtonNumber(26, NoButton), 0, BandLabel, &Blue);
    AmendButtonStatus(ButtonNumber(26, NoButton), 1, BandLabel, &Green);
  }
  for (NoButton = 5; NoButton < 10; NoButton = NoButton + 1)
  {
    AmendButtonStatus(ButtonNumber(26, NoButton), 0, TabBandLabel[NoButton - 5], &Blue);
    AmendButtonStatus(ButtonNumber(26, NoButton), 1, TabBandLabel[NoButton - 5], &Green);
  }
}

void Define_Menu27()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[27], "Frequency Preset Setting Menu (27)"); 

  // Bottom Row, Menu 27

  button = CreateButton(27, 0);
  AddButtonStatus(button, FreqLabel[5], &Blue);
  AddButtonStatus(button, FreqLabel[5], &Green);

  button = CreateButton(27, 1);
  AddButtonStatus(button, FreqLabel[6], &Blue);
  AddButtonStatus(button, FreqLabel[6], &Green);

  button = CreateButton(27, 2);
  AddButtonStatus(button, FreqLabel[7], &Blue);
  AddButtonStatus(button, FreqLabel[7], &Green);

  button = CreateButton(27, 3);
  AddButtonStatus(button, FreqLabel[8], &Blue);
  AddButtonStatus(button, FreqLabel[8], &Green);

  button = CreateButton(27, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 27

  button = CreateButton(27, 5);
  AddButtonStatus(button, FreqLabel[0], &Blue);
  AddButtonStatus(button, FreqLabel[0], &Green);

  button = CreateButton(27, 6);
  AddButtonStatus(button, FreqLabel[1], &Blue);
  AddButtonStatus(button, FreqLabel[1], &Green);

  button = CreateButton(27, 7);
  AddButtonStatus(button, FreqLabel[2], &Blue);
  AddButtonStatus(button, FreqLabel[2], &Green);

  button = CreateButton(27, 8);
  AddButtonStatus(button, FreqLabel[3], &Blue);
  AddButtonStatus(button, FreqLabel[3], &Green);

  button = CreateButton(27, 9);
  AddButtonStatus(button, FreqLabel[4], &Blue);
  AddButtonStatus(button, FreqLabel[4], &Green);
}

void Start_Highlights_Menu27()
{
  // Preset Frequency Change 
  int index;
  int NoButton;
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  for(index = 0; index < 9 ; index = index + 1)
  {
    // Define the button text
    MakeFreqText(index);
    NoButton = index + 5; // Valid for bottom row
    if (index > 4)          // Overwrite for top row
    {
      NoButton = index - 5;
    }
    AmendButtonStatus(ButtonNumber(27, NoButton), 0, FreqBtext, &Blue);
    AmendButtonStatus(ButtonNumber(27, NoButton), 1, FreqBtext, &Green);
  }
}

void Define_Menu28()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[28], "Symbol Rate Preset Setting Menu (28)"); 

  // Bottom Row, Menu 28

  button = CreateButton(28, 0);
  AddButtonStatus(button, SRLabel[5], &Blue);
  AddButtonStatus(button, SRLabel[5], &Green);

  button = CreateButton(28, 1);
  AddButtonStatus(button, SRLabel[6], &Blue);
  AddButtonStatus(button, SRLabel[6], &Green);

  button = CreateButton(28, 2);
  AddButtonStatus(button, SRLabel[7], &Blue);
  AddButtonStatus(button, SRLabel[7], &Green);

  button = CreateButton(28, 3);
  AddButtonStatus(button, SRLabel[8], &Blue);
  AddButtonStatus(button, SRLabel[8], &Green);

  button = CreateButton(28, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 16

  button = CreateButton(28, 5);
  AddButtonStatus(button, SRLabel[0], &Blue);
  AddButtonStatus(button, SRLabel[0], &Green);

  button = CreateButton(28, 6);
  AddButtonStatus(button, SRLabel[1], &Blue);
  AddButtonStatus(button, SRLabel[1], &Green);

  button = CreateButton(28, 7);
  AddButtonStatus(button, SRLabel[2], &Blue);
  AddButtonStatus(button, SRLabel[2], &Green);

  button = CreateButton(28, 8);
  AddButtonStatus(button, SRLabel[3], &Blue);
  AddButtonStatus(button, SRLabel[3], &Green);

  button = CreateButton(28, 9);
  AddButtonStatus(button, SRLabel[4], &Blue);
  AddButtonStatus(button, SRLabel[4], &Green);
}

void Start_Highlights_Menu28()
{
  // SR
}

void Define_Menu29()
{
  int button;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  color_t Grey;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;
  Grey.r=127; Grey.g=127; Grey.b=127;

  strcpy(MenuTitle[29], "Call, Locator and PID Setting Menu (29)"); 

  // Bottom Row, Menu 29

  button = CreateButton(29, 0);
  AddButtonStatus(button, "Video PID", &Blue);

  button = CreateButton(29, 1);
  AddButtonStatus(button, "Audio PID", &Blue);

  button = CreateButton(29, 2);
  AddButtonStatus(button, "PMT PID", &Blue);

  button = CreateButton(29, 3);
  AddButtonStatus(button, "PCR PID", &Grey);

  button = CreateButton(29, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 29

  button = CreateButton(29, 5);
  AddButtonStatus(button, "Call", &Blue);

  button = CreateButton(29, 6);
  AddButtonStatus(button, "Locator", &Blue);
}

void Start_Highlights_Menu29()
{
  // Call, locator and PID

  char Buttext[31];
  color_t Grey;
  color_t Blue;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Grey.r=127; Grey.g=127; Grey.b=127;

  snprintf(Buttext, 13, "Call^%s", CallSign);
  AmendButtonStatus(ButtonNumber(29, 5), 0, Buttext, &Blue);

  snprintf(Buttext, 16, "Locator^%s", Locator);
  AmendButtonStatus(ButtonNumber(29, 6), 0, Buttext, &Blue);

  snprintf(Buttext, 17, "Video PID^%s", PIDvideo);
  AmendButtonStatus(ButtonNumber(29, 0), 0, Buttext, &Blue);

  snprintf(Buttext, 17, "Audio PID^%s", PIDaudio);
  AmendButtonStatus(ButtonNumber(29, 1), 0, Buttext, &Blue);

  snprintf(Buttext, 17, "PMT PID^%s", PIDpmt);
  AmendButtonStatus(ButtonNumber(29, 2), 0, Buttext, &Blue);

  snprintf(Buttext, 17, "PCR PID^%s", PIDstart);
  AmendButtonStatus(ButtonNumber(29, 3), 0, Buttext, &Grey);
}

void Define_Menu32()
{
  int button;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  color_t Green;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;
  Green.r=0; Green.g=128; Green.b=0;

  strcpy(MenuTitle[32], "Select and Set ADF Ref Frequency Menu (32)"); 

  // Bottom Row, Menu 32

  button = CreateButton(32, 0);
  AddButtonStatus(button, "Set Ref 1", &Blue);

  button = CreateButton(32, 1);
  AddButtonStatus(button, "Set Ref 2", &Blue);

  button = CreateButton(32, 2);
  AddButtonStatus(button, "Set Ref 3", &Blue);

  //button = CreateButton(32, 3);
  //AddButtonStatus(button, "Set 5355", &Blue);

  button = CreateButton(32, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 32

  button = CreateButton(32, 5);
  AddButtonStatus(button, "Use Ref 1", &Blue);
  AddButtonStatus(button, "Use Ref 1", &Green);

  button = CreateButton(32, 6);
  AddButtonStatus(button, "Use Ref 2", &Blue);
  AddButtonStatus(button, "Use Ref 2", &Green);

  button = CreateButton(32, 7);
  AddButtonStatus(button, "Use Ref 3", &Blue);
  AddButtonStatus(button, "Use Ref 3", &Green);

  //button = CreateButton(32, 6);
  //AddButtonStatus(button, "Switch", &Blue);
}

void Start_Highlights_Menu32()
{
  // Call, locator and PID

  char Buttext[31];
  color_t Green;
  color_t Blue;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Green.r=0; Green.g=128; Green.b=0;

  snprintf(Buttext, 20, "Use Ref 1^%s", ADFRef[0]);
  AmendButtonStatus(ButtonNumber(32, 5), 0, Buttext, &Blue);
  AmendButtonStatus(ButtonNumber(32, 5), 1, Buttext, &Green);

  snprintf(Buttext, 20, "Use Ref 2^%s", ADFRef[1]);
  AmendButtonStatus(ButtonNumber(32, 6), 0, Buttext, &Blue);
  AmendButtonStatus(ButtonNumber(32, 6), 1, Buttext, &Green);

  snprintf(Buttext, 20, "Use Ref 3^%s", ADFRef[2]);
  AmendButtonStatus(ButtonNumber(32, 7), 0, Buttext, &Blue);
  AmendButtonStatus(ButtonNumber(32, 7), 1, Buttext, &Green);

  if (strcmp(ADFRef[0], CurrentADFRef) == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 7, 5, 1);
  }
  else if (strcmp(ADFRef[1], CurrentADFRef) == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 7, 6, 1);
  }
  else if (strcmp(ADFRef[2], CurrentADFRef) == 0)
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 7, 7, 1);
  }
  else
  {
    SelectInGroupOnMenu(CurrentMenu, 5, 7, 7, 0);
  }
  
  snprintf(Buttext, 20, "Set Ref 1^%s", ADFRef[0]);
  AmendButtonStatus(ButtonNumber(32, 0), 0, Buttext, &Blue);

  snprintf(Buttext, 20, "Set Ref 2^%s", ADFRef[1]);
  AmendButtonStatus(ButtonNumber(32, 1), 0, Buttext, &Blue);

  snprintf(Buttext, 20, "Set Ref 3^%s", ADFRef[2]);
  AmendButtonStatus(ButtonNumber(32, 2), 0, Buttext, &Blue);

  //snprintf(Buttext, 20, "Set 5355^%s", ADF5355Ref);
  //AmendButtonStatus(ButtonNumber(32, 3), 0, Buttext, &Blue);
}

void Define_Menu33()
{
  int button;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  color_t Green;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;
  Green.r=0; Green.g=128; Green.b=0;

  strcpy(MenuTitle[33], "Check for Software Update Menu (33)"); 

  // Bottom Row, Menu 33

  button = CreateButton(33, 4);
  AddButtonStatus(button, "Exit", &DBlue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 33

  button = CreateButton(33, 5);
  AddButtonStatus(button, "Update", &Blue);
  AddButtonStatus(button, "Update", &Green);

  button = CreateButton(33, 6);
  AddButtonStatus(button, "Dev Update", &Blue);
  AddButtonStatus(button, "Dev Update", &Green);

  button = CreateButton(33, 7);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);
}

void Start_Highlights_Menu33()
{
  // Check for update

  color_t Red;
  color_t Blue;
  color_t Grey;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=128; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;

  if (strcmp(UpdateStatus, "NotAvailable") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "NormalUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, "Update^Now", &Blue);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "ForceUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, "Force^Update", &Blue);
    AmendButtonStatus(ButtonNumber(33, 6), 0, "Dev^Update", &Blue);
    AmendButtonStatus(ButtonNumber(33, 7), 0, " ", &Grey);
  }
  if (strcmp(UpdateStatus, "DevUpdate") == 0)
  {
    AmendButtonStatus(ButtonNumber(33, 5), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 6), 0, " ", &Grey);
    AmendButtonStatus(ButtonNumber(33, 7), 0, "Confirm^Dev Update", &Red);
  }
}

void Define_Menu41()
{
  int button;
  //color_t Green;
  color_t Blue;
  color_t LBlue;
  //Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;

  strcpy(MenuTitle[41], ""); 

  button = CreateButton(41, 0);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  //button = CreateButton(41, 1);
  //AddButtonStatus(button, "X", &Blue);
  //AddButtonStatus(button, "X", &LBlue);
  button = CreateButton(41, 2);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  button = CreateButton(41, 3);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  button = CreateButton(41, 4);
  AddButtonStatus(button, "-", &Blue);
  AddButtonStatus(button, "-", &LBlue);
  AddButtonStatus(button, "_", &Blue);
  AddButtonStatus(button, "_", &LBlue);
  //button = CreateButton(41, 5);
  //AddButtonStatus(button, "N", &Blue);
  //AddButtonStatus(button, "N", &LBlue);
  button = CreateButton(41, 6);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(41, 7);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(41, 8);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  button = CreateButton(41, 9);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  button = CreateButton(41, 10);
  AddButtonStatus(button, "Z", &Blue);
  AddButtonStatus(button, "Z", &LBlue);
  AddButtonStatus(button, "z", &Blue);
  AddButtonStatus(button, "z", &LBlue);
  button = CreateButton(41, 11);
  AddButtonStatus(button, "X", &Blue);
  AddButtonStatus(button, "X", &LBlue);
  AddButtonStatus(button, "x", &Blue);
  AddButtonStatus(button, "x", &LBlue);
  button = CreateButton(41, 12);
  AddButtonStatus(button, "C", &Blue);
  AddButtonStatus(button, "C", &LBlue);
  AddButtonStatus(button, "c", &Blue);
  AddButtonStatus(button, "c", &LBlue);
  button = CreateButton(41, 13);
  AddButtonStatus(button, "V", &Blue);
  AddButtonStatus(button, "V", &LBlue);
  AddButtonStatus(button, "v", &Blue);
  AddButtonStatus(button, "v", &LBlue);
  button = CreateButton(41, 14);
  AddButtonStatus(button, "B", &Blue);
  AddButtonStatus(button, "B", &LBlue);
  AddButtonStatus(button, "b", &Blue);
  AddButtonStatus(button, "b", &LBlue);
  button = CreateButton(41, 15);
  AddButtonStatus(button, "N", &Blue);
  AddButtonStatus(button, "N", &LBlue);
  AddButtonStatus(button, "n", &Blue);
  AddButtonStatus(button, "n", &LBlue);
  button = CreateButton(41, 16);
  AddButtonStatus(button, "M", &Blue);
  AddButtonStatus(button, "M", &LBlue);
  AddButtonStatus(button, "m", &Blue);
  AddButtonStatus(button, "m", &LBlue);
  button = CreateButton(41, 17);
  AddButtonStatus(button, ",", &Blue);
  AddButtonStatus(button, ",", &LBlue);
  AddButtonStatus(button, "!", &Blue);
  AddButtonStatus(button, "!", &LBlue);
  button = CreateButton(41, 18);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  button = CreateButton(41, 19);
  AddButtonStatus(button, "/", &Blue);
  AddButtonStatus(button, "/", &LBlue);
  AddButtonStatus(button, "?", &Blue);
  AddButtonStatus(button, "?", &LBlue);
  button = CreateButton(41, 20);
  AddButtonStatus(button, "A", &Blue);
  AddButtonStatus(button, "A", &LBlue);
  AddButtonStatus(button, "a", &Blue);
  AddButtonStatus(button, "a", &LBlue);
  button = CreateButton(41, 21);
  AddButtonStatus(button, "S", &Blue);
  AddButtonStatus(button, "S", &LBlue);
  AddButtonStatus(button, "s", &Blue);
  AddButtonStatus(button, "s", &LBlue);
  button = CreateButton(41, 22);
  AddButtonStatus(button, "D", &Blue);
  AddButtonStatus(button, "D", &LBlue);
  AddButtonStatus(button, "d", &Blue);
  AddButtonStatus(button, "d", &LBlue);
  button = CreateButton(41, 23);
  AddButtonStatus(button, "F", &Blue);
  AddButtonStatus(button, "F", &LBlue);
  AddButtonStatus(button, "f", &Blue);
  AddButtonStatus(button, "f", &LBlue);
  button = CreateButton(41, 24);
  AddButtonStatus(button, "G", &Blue);
  AddButtonStatus(button, "G", &LBlue);
  AddButtonStatus(button, "g", &Blue);
  AddButtonStatus(button, "g", &LBlue);
  button = CreateButton(41, 25);
  AddButtonStatus(button, "H", &Blue);
  AddButtonStatus(button, "H", &LBlue);
  AddButtonStatus(button, "h", &Blue);
  AddButtonStatus(button, "h", &LBlue);
  button = CreateButton(41, 26);
  AddButtonStatus(button, "J", &Blue);
  AddButtonStatus(button, "J", &LBlue);
  AddButtonStatus(button, "j", &Blue);
  AddButtonStatus(button, "j", &LBlue);
  button = CreateButton(41, 27);
  AddButtonStatus(button, "K", &Blue);
  AddButtonStatus(button, "K", &LBlue);
  AddButtonStatus(button, "k", &Blue);
  AddButtonStatus(button, "k", &LBlue);
  button = CreateButton(41, 28);
  AddButtonStatus(button, "L", &Blue);
  AddButtonStatus(button, "L", &LBlue);
  AddButtonStatus(button, "l", &Blue);
  AddButtonStatus(button, "l", &LBlue);
  //button = CreateButton(41, 29);
  //AddButtonStatus(button, "/", &Blue);
  //AddButtonStatus(button, "/", &LBlue);
  button = CreateButton(41, 30);
  AddButtonStatus(button, "Q", &Blue);
  AddButtonStatus(button, "Q", &LBlue);
  AddButtonStatus(button, "q", &Blue);
  AddButtonStatus(button, "q", &LBlue);
  button = CreateButton(41, 31);
  AddButtonStatus(button, "W", &Blue);
  AddButtonStatus(button, "W", &LBlue);
  AddButtonStatus(button, "w", &Blue);
  AddButtonStatus(button, "w", &LBlue);
  button = CreateButton(41, 32);
  AddButtonStatus(button, "E", &Blue);
  AddButtonStatus(button, "E", &LBlue);
  AddButtonStatus(button, "e", &Blue);
  AddButtonStatus(button, "e", &LBlue);
  button = CreateButton(41, 33);
  AddButtonStatus(button, "R", &Blue);
  AddButtonStatus(button, "R", &LBlue);
  AddButtonStatus(button, "r", &Blue);
  AddButtonStatus(button, "r", &LBlue);
  button = CreateButton(41, 34);
  AddButtonStatus(button, "T", &Blue);
  AddButtonStatus(button, "T", &LBlue);
  AddButtonStatus(button, "t", &Blue);
  AddButtonStatus(button, "t", &LBlue);
  button = CreateButton(41, 35);
  AddButtonStatus(button, "Y", &Blue);
  AddButtonStatus(button, "Y", &LBlue);
  AddButtonStatus(button, "y", &Blue);
  AddButtonStatus(button, "y", &LBlue);
  button = CreateButton(41, 36);
  AddButtonStatus(button, "U", &Blue);
  AddButtonStatus(button, "U", &LBlue);
  AddButtonStatus(button, "u", &Blue);
  AddButtonStatus(button, "u", &LBlue);
  button = CreateButton(41, 37);
  AddButtonStatus(button, "I", &Blue);
  AddButtonStatus(button, "I", &LBlue);
  AddButtonStatus(button, "i", &Blue);
  AddButtonStatus(button, "i", &LBlue);
  button = CreateButton(41, 38);
  AddButtonStatus(button, "O", &Blue);
  AddButtonStatus(button, "O", &LBlue);
  AddButtonStatus(button, "o", &Blue);
  AddButtonStatus(button, "o", &LBlue);
  button = CreateButton(41, 39);
  AddButtonStatus(button, "P", &Blue);
  AddButtonStatus(button, "P", &LBlue);
  AddButtonStatus(button, "p", &Blue);
  AddButtonStatus(button, "p", &LBlue);

  button = CreateButton(41, 40);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  button = CreateButton(41, 41);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  button = CreateButton(41, 42);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  button = CreateButton(41, 43);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  button = CreateButton(41, 44);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  button = CreateButton(41, 45);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  button = CreateButton(41, 46);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  button = CreateButton(41, 47);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  button = CreateButton(41, 48);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  button = CreateButton(41, 49);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
}

static void
terminate(int dummy)
{
  strcpy(ModeInput, "DESKTOP"); // Set input so webcam reset script is not called
  TransmitStop();
  ReceiveStop();
  finish();
  printf("Terminate\n");
  char Commnd[255];
  sprintf(Commnd,"stty echo");
  system(Commnd);
  sprintf(Commnd,"reset");
  system(Commnd);
  exit(1);
}

// main initializes the system and starts Menu 1 

int main(int argc, char **argv)
{
  int NoDeviceEvent=0;
  saveterm();
  init(&wscreen, &hscreen);
  rawterm();
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int ReceiveDirect=0;
  int i;
  char Param[255];
  char Value[255];
  char USBVidDevice[255];
  char SetStandard[255];
  char vcoding[256];
  char vsource[256];

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

  // Initialise all the spi GPIO ports to the correct state
  InitialiseGPIO();

  // Determine if using waveshare or waveshare B screen
  // Either by first argument or from rpidatvconfig.txt
  if(argc>1)
    Inversed=atoi(argv[1]);
  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if(strcmp(Value,"Waveshare")==0)
    Inversed=1;
  if(strcmp(Value,"WaveshareB")==0)
    Inversed=1;
  if(strcmp(Value,"Waveshare4")==0)
    Inversed=1;

  // Set the Band (and filter) Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using rpidatvconfig.txt
  usleep(100000);

  // Set the Analog Capture (input) Standard
  GetUSBVidDev(USBVidDevice);
  if (strlen(USBVidDevice) == 12)  // /dev/video* with a new line
  {
    strcpy(Param,"analogcamstandard");
    GetConfigParam(PATH_PCONFIG,Param,Value);
    USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
    strcpy(SetStandard, "v4l2-ctl -d ");
    strcat(SetStandard, USBVidDevice);
    strcat(SetStandard, " --set-standard=");
    strcat(SetStandard, Value);
    //printf(SetStandard);
    system(SetStandard);
  }

  // Determine if ReceiveDirect 2nd argument 
  if(argc>2)
  {
    ReceiveDirect=atoi(argv[2]);
  }
  if(ReceiveDirect==1)
  {
    getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax);
    ProcessLeandvb(); // For FrMenu and no 
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

  // Replace BATC Logo with IP address with BATC Logo alone
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");

  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  printf ("X Scale Factor = %f\n", scaleXvalue);
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;
  printf ("Y Scale Factor = %f\n", scaleYvalue);

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize=(wscreen-25)/5;
  hbuttonsize=hscreen/6;

  // Read in the touchscreen Calibration
  ReadTouchCal();
  
  printf("Read in the presets from the Config file \n");
  // Read in the presets from the Config file
  ReadPresets();
  ReadModeInput(vcoding, vsource);
  ReadModeOutput(vcoding);
  ReadModeEasyCap();
  ReadCaptionState();
  ReadAudioState();
  ReadAttenState();
  ReadBand();
  ReadBandDetails();
  ReadCallLocPID();
  ReadADFRef();

  // Initialise all the button Status Indexes to 0
  InitialiseButtons();

  // Define all the Menu buttons
  Define_Menu1();
  Define_Menu2();
  Define_Menu3();

  Define_Menu11();
  Define_Menu12();
  Define_Menu13();
  Define_Menu14();
  Define_Menu15();
  Define_Menu16();
  Define_Menu17();
  Define_Menu18();
  Define_Menu19();

  Define_Menu21();
  Define_Menu22();
  Define_Menu23();
  Define_Menu24();
  Define_Menu26();
  Define_Menu27();
  Define_Menu28();
  Define_Menu29();
  //Define_Menu30();
  //Define_Menu31();
  Define_Menu32();
  Define_Menu33();

  Define_Menu41();


  // Start the button Menu
  Start(wscreen,hscreen);

  // Determine button highlights
  Start_Highlights_Menu1();
  printf("Entering Update Window\n");  
  UpdateWindow();
  printf("Update Window\n");

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Not sure that the program flow ever gets here

  restoreterm();
  finish();
  return 0;
}
