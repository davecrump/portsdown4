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
#include <stdbool.h>

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
#define PATH_RTLPRESETS "/home/pi/rpidatv/scripts/rtl-fm_presets.txt"
#define PATH_LOCATORS "/home/pi/rpidatv/scripts/portsdown_locators.txt"
#define PATH_RXPRESETS "/home/pi/rpidatv/scripts/rx_presets.txt"
#define PATH_STREAMPRESETS "/home/pi/rpidatv/scripts/stream_presets.txt"

#define PI 3.14159265358979323846
#define deg2rad(DEG) ((DEG)*((PI)/(180.0)))
#define rad2deg(RAD) ((RAD)*180/PI)

char ImageFolder[63]="/home/pi/rpidatv/image/";

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
	int x,y,w,h;                   // Position and size of button
	status_t Status[MAX_STATUS];   // Array of text and required colour for each status
	int IndexStatus;               // The number of valid status definitions.  0 = do not display
	int NoStatus;                  // This is the active status (colour and text)
} button_t;

// 	int LastEventTime; Was part of button_t.  No longer used

#define MAX_BUTTON 600
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
#define TIME_ANTI_BOUNCE 500
int CurrentMenu = 1;
int CallingMenu = 1;

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
int GPIO_Band_LSB = 31;
int GPIO_Band_MSB = 24;
int GPIO_Tverter = 7;

char ScreenState[255] = "NormalMenu";  // NormalMenu SpecialMenu TXwithMenu TXwithImage RXwithImage VideoOut SnapView VideoView Snap SigGen
char MenuTitle[40][127];

// Values for buttons
// May be over-written by values from from portsdown_config.txt:
char TabBand[9][3]={"d1", "d2", "d3", "d4", "d5", "t1", "t2", "t3", "t4"};
char TabBandLabel[9][15]={"71_MHz", "146_MHz", "70_cm", "23_cm", "13_cm", "9_cm", "6_cm", "3_cm", "1.2_cm"};
char TabPresetLabel[4][15]={"-", "-", "-", "-"};
float TabBandAttenLevel[9]={-10, -10, -10, -10, -10, -10, -10, -10, -10};
int TabBandExpLevel[9]={30, 30, 30, 30, 30, 30, 30, 30, 30};
int TabBandLimeGain[9]={90, 90, 90, 90, 90, 90, 90, 90, 90};
int TabBandExpPorts[9]={2, 2, 2, 2, 2, 2, 2, 2, 2};
float TabBandLO[9]={0, 0, 0, 0, 0, 3024, 5328, 9936, 23616};
char TabBandNumbers[9][10]={"1111", "2222", "3333", "4444", "5555", "6666", "7777", "8888", "9999"};
int TabSR[9]={125,333,1000,2000,4000, 88, 250, 500, 3000};
char SRLabel[9][255]={"SR 125","SR 333","SR1000","SR2000","SR4000", "SR 88", "SR 250", "SR 500", "SR3000"};
int TabFec[5]={1, 2, 3, 5, 7};
int TabS2Fec[9]={14, 13, 12, 35, 23, 34, 56, 89, 91};
char S2FECLabel[9][7]={"1/4", "1/3", "1/2", "3/5", "2/3", "3/4", "5/6", "8/9", "9/10"};
char TabModeInput[12][255]={"CAMMPEG-2","CAMH264","PATERNAUDIO","ANALOGCAM","CARRIER","CONTEST"\
  ,"IPTSIN","ANALOGMPEG-2", "CARDMPEG-2", "CAMHDMPEG-2", "DESKTOP", "FILETS"};
char TabFreq[9][255]={"71", "146.5", "437", "1249", "1255", "436", "436.5", "437.5", "438"};
char FreqLabel[31][255];
char TabModeAudio[6][15]={"auto", "mic", "video", "bleeps", "no_audio", "webcam"};
char TabModeSTD[2][7]={"6","0"};
char TabModeVidIP[2][7]={"0","1"};
char TabModeOP[9][31]={"IQ", "QPSKRF", "DATVEXPRESS", "LIMEUSB", "STREAMER", "COMPVID", "DTX1", "IP", "LIMEMINI"};
char TabModeOPtext[9][31]={"Portsdown", " UGLY ", "EXPRESS", "Lime USB", "BATC^STREAM", "Comp Vid", " DTX1 ", "IPTS out", "Lime Mini"};
char TabAtten[4][15] = {"NONE", "PE4312", "PE43713", "HMC1119"};
char CurrentModeOP[31] = "QPSKRF";
char CurrentModeOPtext[31] = " UGLY ";
char TabTXMode[6][255] = {"DVB-S", "Carrier", "S2QPSK", "8PSK", "16APSK", "32APSK"};
char CurrentTXMode[255] = "DVB-S";
char CurrentModeInput[255] = "DESKTOP";
char TabEncoding[4][255] = {"H264", "MPEG-2", "IPTS in", "TS File"};
char CurrentEncoding[255] = "H264";
char TabSource[8][15] = {"Pi Cam", "CompVid", "TCAnim", "TestCard", "PiScreen", "Contest", "Webcam", "C920"};
char CurrentSource[15] = "PiScreen";
char TabFormat[4][15] = {"4:3", "16:9","720p", "1080p"};
char CurrentFormat[15] = "4:3";
char CurrentCaptionState[15] = "on";
char CurrentAudioState[255] = "auto";
char CurrentAtten[255] = "NONE";
int CurrentBand = 2; // 0 thru 8
char KeyboardReturn[64];
char FreqBtext[31];
char MenuText[5][63];

// Composite Video Output variables
char TabVidSource[8][15] = {"Pi Cam", "CompVid", "TCAnim", "TestCard", "Snap", "Contest", "Webcam", "Movie"};
char CurrentVidSource[15] = "TestCard";
int VidPTT = 0;             // 0 is off, 1 is on
int CompVidBand = 2;        // Band used for Comp Vid Contest numbers
int ImageIndex = 0;         // Test Card selection number
int ImageRange = 5;         // Number of Test Cards

// RTL FM Parameters. [0] is current
char RTLfreq[10][15];       // String with frequency in MHz
char RTLlabel[10][15];      // String for label
char RTLmode[10][15];       // String for mode: fm, wbfm, am, usb, lsb
int RTLsquelch[10];         // between 0 and 1000.  0 is off
int RTLsquelchoveride = 1;  // if 0, squelch has been over-riden
int RTLppm = 0;             // RTL frequency correction in integer ppm
int RTLStoreTrigger = 0;    // Set to 1 if ready to store RTL preset
int RTLdetected = 0;        // Set to 1 at first entry to Menu 6 if RTL detected
int RTLactive = 0;          // Set to 1 if RTL_FM receiver running

// LeanDVB RX Parameters. [0] is current. 1-4 are presets
char RXfreq[5][15];         // String with frequency in MHz
char RXlabel[5][15];        // String for label
int RXsr[5];                // Symbol rate in K
char RXfec[5][7];           // FEC as String
int RXsamplerate[5];        // Samplerate in K. 0 = auto
int RXgain[5];              // Gain
char RXmodulation[5][15];   // Modulation
char RXencoding[5][15];     // Encoding
char RXsdr[5][15];          // SDR Type
char RXgraphics[5][7];      // Graphics on/off
char RXparams[5][7];        // Parameters on/off
char RXsound[5][7];         // Sound on/off
char RXfastlock[5][7];      // Fastlock on/off
int RXStoreTrigger = 0;     // Set to 1 if ready to store RX preset
int FinishedButton2 = 1;    // Used to control FFT
fftwf_complex *fftout=NULL; // FFT for RX
#define FFT_SIZE 256        // for RX display

// Stream Display Parameters
char StreamAddress[9][127];  // Full rtmp address of stream
char StreamLabel[9][31];     // Button Label for stream
int IQAvailable = 1;         // Flag set to 0 if RPi audio output has been used
int StreamStoreTrigger = 0;   // Set to 1 if stream amendment needed

// Range and Bearing Calculator Parameters
int GcBearing(const float, const float, const float, const float);
float GcDistance(const float, const float, const float, const float, const char *);
float Locator_To_Lat(char *);
float Locator_To_Lon(char *);
int CalcBearing(char *, char *);
int CalcRange(char *, char *);
bool CheckLocator(char *);

// Touch display variables
int Inversed=0;               //Display is inversed (Waveshare=1)
int PresetStoreTrigger = 0;   //Set to 1 if awaiting preset being stored
int FinishedButton = 0;       // Used to indicate screentouch during TX or RX

pthread_t thfft, thbutton, thview, thwait3;

// Function Prototypes

void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();
void Start_Highlights_Menu4();
void Start_Highlights_Menu5();
void Start_Highlights_Menu6();
void Start_Highlights_Menu11();
void Start_Highlights_Menu12();
void Start_Highlights_Menu13();
void Start_Highlights_Menu14();
void Start_Highlights_Menu15();
void Start_Highlights_Menu16();
void Start_Highlights_Menu17();
void Start_Highlights_Menu18();
void Start_Highlights_Menu19();
void Start_Highlights_Menu20();
void Start_Highlights_Menu21();
void Start_Highlights_Menu22();
void Start_Highlights_Menu23();
void Start_Highlights_Menu24();
void Start_Highlights_Menu25();
void Start_Highlights_Menu26();
void Start_Highlights_Menu27();
void Start_Highlights_Menu28();
void Start_Highlights_Menu29();
void Start_Highlights_Menu30();
void Start_Highlights_Menu31();
void Start_Highlights_Menu32();
void Start_Highlights_Menu33();
void MsgBox(const char *);
void MsgBox2(const char *, const char *);
void MsgBox4(const char *, const char *, const char *, const char *);
void wait_touch();
void waituntil(int, int);
void Keyboard(char *, char *, int);
void DoFreqChange();
void CompVidStart();
void ReceiveLOStart();
int getTouchSample(int *, int *, int *);
void TransformTouchMap(int, int);
int ButtonNumber(int, int);
int CheckLimeInstalled();
int CheckLimeConnect();

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

void DisplayHere(char *DisplayCaption)
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


/***************************************************************************//**
 * @brief Looks up the card number for the RPi Audio Card
 *
 * @param card (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetPiAudioCard(char card[15])
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
 * @brief Looks up the card number for the USB Microphone Card
 *
 * @param mic (str) as a single character string with no <CR>
 *
 * @return void
*******************************************************************************/

void GetMicAudioCard(char mic[15])
{
  FILE *fp;

  /* Open the command for reading. */
  fp = popen("cat /proc/asound/modules | grep \"usb_audio\" | head -c 2 | tail -c 1", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(mic, 7, fp) != NULL)
  {
    sprintf(mic, "%d", atoi(mic));
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
  strcpy(LinuxCommand, "wget -4 --timeout=2 https://raw.githubusercontent.com/BritishAmateurTelevisionClub/");
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
  case 0:  // Install Lime Mini and USB
    // Display the update message
    finish();
    system("/home/pi/rpidatv/scripts/install_lime.sh");
    init(&wscreen, &hscreen);
    Start(wscreen, hscreen);
    wait_touch();
    break;
  case 1:  // Upgrade Lime Firmware
    if (CheckLimeConnect() == 0)
    {
      MsgBox4("Upgrading Lime Firmware", " ", " ", " ");
      system("LimeUtil --update");
      MsgBox4("Firmware Upgrade Complete", " ", "Touch Screen to Continue" ," ");
      wait_touch();
    }
    else
    {
      MsgBox4("No Lime Mini Connected", " ", "Touch Screen to Continue" ," ");
      wait_touch();
    }
    break;
  default:
    break;
  }
}


/***************************************************************************//**
 * @brief Checks if valid MPEG-2 decoder license is loaded
 *
 * @param nil
 *
 * @return 0 if not enabled, 1 if enabled
*******************************************************************************/

int CheckMPEG2()
{
  FILE *fp;
  int check = 0;
  char response[63];

  /* Open the command for reading. */
  fp = popen("vcgencmd codec_enabled MPG2", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while ((fgets(response, 16, fp) != NULL) && (check == 0))
  {
    //printf("%s\n", response);
    if(strcmp(response, "MPG2=enabled\n") == 0)
    {
      check = 1;
    }
  }

  /* close */
  pclose(fp);
  return check;
}

/***************************************************************************//**
 * @brief Gets the RPi Serial number for the MPEG-2 Key
 *
 * @param SerialString (str) RPi Serial Number
 *
 * @return void
*******************************************************************************/

void GetRPiSerial(char* SerialString)
{
  FILE *fp;
  int check = 0;
  char response[63];
  char cutresponse[31];

  /* Open the command for reading. */
  fp = popen("cat /proc/cpuinfo", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while ((fgets(response, 32, fp) != NULL) && (check == 0))
  {
    //printf("%s\n", response);
    strcpy(cutresponse, response);
    cutresponse[6]='\0';
    //printf("%s\n", cutresponse);
    if(strcmp(cutresponse, "Serial") == 0)
    {
      strcpy(SerialString, response);
      check = 1;
    }
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief Gets the MPEG-2 Key if loaded.  If not, 0x.
 *
 * @param KeyString (str) MPEG-2 Key
 *
 * @return void
*******************************************************************************/

void GetMPEGKey(char* KeyString)
{
  FILE *fp;
  int check = 0;
  char response[63];
  char cutresponse[31];
  strcpy(KeyString, "0x");

  /* Open the command for reading. */
  fp = popen("cat /boot/config.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while ((fgets(response, 32, fp) != NULL) && (check == 0))
  {
    //printf("%s\n", response);
    strcpy(cutresponse, response);
    cutresponse[12]='\0';
    //printf("%s\n", cutresponse);
    if(strcmp(cutresponse, "decode_MPG2=") == 0)
    {
      strcpy(KeyString, &response[12]); // Remove first 12 characters of the string
      KeyString[10] = '\0';             // remove trailing <cr>
      check = 1;
    }
  }

  /* close */
  pclose(fp);
}

/***************************************************************************//**
 * @brief If key line exists in /boot/config.txt, overwrite with new entry
 *        If key line does not exist, add it
 *
 * @param KeyString (str) 10 character MPEG-2 Key
 *
 * @return void
*******************************************************************************/

void NewMPEGKey(char* KeyString)
{
  FILE *fp;
  int check = 0;
  char response[63];
  char cutresponse[31];
  char CommandString[127];

  /* Open the command for reading. */
  fp = popen("cat /boot/config.txt", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while ((fgets(response, 32, fp) != NULL) && (check == 0))
  {
    strcpy(cutresponse, response);
    cutresponse[12]='\0';
    if(strcmp(cutresponse, "decode_MPG2=") == 0)
    {
      check = 1;
    }
  }

  if (check == 1) // key line already exists
  {
    snprintf(CommandString, 100, "sudo sh -c \"sed -i.bak \'/decode_MPG2/d\' /boot/config.txt\"");
    //printf("%s\n", CommandString);
    system(CommandString);
  }
  else  // Add the title
  {
    snprintf(CommandString, 100, "sudo sh -c \"echo \\\"# MPEG-2 Decoder Key\\\" >> /boot/config.txt\"");
    //printf("%s\n", CommandString);
    system(CommandString);
  }

  // Now add the new key
  snprintf(CommandString, 100, "sudo sh -c \"echo \\\"decode_MPG2=%s\\\" >> /boot/config.txt\"", KeyString);
  //printf("%s\n", CommandString);
  system(CommandString);

  /* close */
  pclose(fp);
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
  char ModeInput[31];
  GetConfigParam(PATH_PCONFIG,"modeinput", ModeInput);

  char ModulationMode[31];
  GetConfigParam(PATH_PCONFIG,"modulation", ModulationMode);
  if (strlen(ModulationMode) < 1)
  {
    strcpy(CurrentTXMode, "DVB-S");
  }
  else
  {
    strcpy(CurrentTXMode, ModulationMode);
  }

  strcpy(coding, "notset");
  strcpy(vsource, "notset");

  if (strcmp(ModeInput, "CAMH264") == 0) 
  {
    strcpy(coding, "H264");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  } 
  else if (strcmp(ModeInput, "ANALOGCAM") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Static Test Card F");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "PATERNAUDIO") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Test Card");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[2]); // TCAnim
  }
  else if (strcmp(ModeInput, "CONTEST") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "DESKTOP") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "Screen");
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
    strcpy(CurrentEncoding, "H264");
  }
  else if (strcmp(ModeInput, "FILETS") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "TS File");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentEncoding, "TS File");
  }
  else if (strcmp(ModeInput, "IPTSIN") == 0)
  {
    strcpy(coding, "Native");
    strcpy(vsource, "IP Transport Stream");
    strcpy(CurrentEncoding, "IPTS in");
  }
  else if (strcmp(ModeInput, "VNC") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "VNC");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
  }
  else if (strcmp(ModeInput, "CAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Camera");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOGMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAMMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[3]); // Desktop
  }
  else if (strcmp(ModeInput, "CONTESTMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Contest Numbers");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[5]); // Contest
  }
  else if (strcmp(ModeInput, "CAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam 16:9");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "CAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "RPi Cam HD");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[0]); // Pi Cam
  }
  else if (strcmp(ModeInput, "ANALOG16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Ext Video Input");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[1]); // EasyCap
  }
  else if (strcmp(ModeInput, "WEBCAM16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "WEBCAMHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Webcam");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[6]); // Webcam
  }
  else if (strcmp(ModeInput, "CARD16MPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "16:9");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "CARDHDMPEG-2") == 0)
  {
    strcpy(coding, "MPEG-2");
    strcpy(vsource, "Static Test Card");
    strcpy(CurrentEncoding, "MPEG-2");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[3]); // TestCard
  }
  else if (strcmp(ModeInput, "C920H264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "4:3");
    strcpy(CurrentSource, TabSource[7]); // C920
  }
  else if (strcmp(ModeInput, "C920HDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
    strcpy(CurrentEncoding, "H264");
    strcpy(CurrentFormat, "720p");
    strcpy(CurrentSource, TabSource[7]); // C920
  }
  else if (strcmp(ModeInput, "C920FHDH264") == 0)
  {
    strcpy(coding, "H264");
    strcpy(vsource, "C920 Webcam");
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
  else if (strcmp(ModeOutput, "LIMEUSB") == 0) 
  {
    strcpy(Moutput, "LimeSDR USB");
    strcpy(CurrentModeOPtext, TabModeOPtext[3]);
  } 
  else if (strcmp(ModeOutput, "LIMEMINI") == 0) 
  {
    strcpy(Moutput, "LimeSDR Mini");
    strcpy(CurrentModeOPtext, TabModeOPtext[8]);
  } 
  else if (strcmp(ModeOutput, "STREAMER") == 0) 
  {
    strcpy(Moutput, "BATC Streaming");
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
 * @brief Initialises all the GPIOs at startup
 *
 * @param None
 *
 * @return void
*******************************************************************************/

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
  pinMode(GPIO_Band_LSB, OUTPUT);
  pinMode(GPIO_Band_MSB, OUTPUT);
  pinMode(GPIO_Tverter, OUTPUT);
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

/***************************************************************************//**
 * @brief Saves RTL-FM Freq, Mode, squelch and label
 *        
 * @param Preset button number 0-9, but not 4
 *
 * @return void
*******************************************************************************/

void SaveRTLPreset(int PresetButton)
{
  char Param[255];
  char Value[255];
  char Prompt[63];
  int index;
  int Spaces;
  int j;

  // Transform button number into preset index
  index = PresetButton + 6;  // works for bottom row
  if (PresetButton > 4)      // second row
  {
    index = PresetButton - 4;
  }

  if (index != 0)
  {
    // Read the current preset label and ask for a new value
    snprintf(Prompt, 62, "Enter the new label for RTL Preset %d (no spaces):", index);

    // Check that there are no spaces
    while (Spaces >= 1)
    {
      Keyboard(Prompt, RTLlabel[index], 10);

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
    strcpy(RTLlabel[index], KeyboardReturn);
    snprintf(Param, 10, "r%dtitle", index);
    SetConfigParam(PATH_RTLPRESETS, Param, KeyboardReturn);
  }

  // Copy the current values into the presets arrays


  strcpy(RTLfreq[index], RTLfreq[0]);
  strcpy(RTLmode[index], RTLmode[0]);
  RTLsquelch[index] = RTLsquelch[0];

  // Save the current values into the presets File

  snprintf(Param, 10, "r%dfreq", index);
  SetConfigParam(PATH_RTLPRESETS, Param, RTLfreq[0]);

  snprintf(Param, 10, "r%dmode", index);
  SetConfigParam(PATH_RTLPRESETS, Param, RTLmode[index]);

  snprintf(Param, 10, "r%dsquelch", index);
  snprintf(Value, 4, "%d", RTLsquelch[0]);
  SetConfigParam(PATH_RTLPRESETS, Param, Value);
}

/***************************************************************************//**
 * @brief Loads RTL-FM Freq, Mode, squelch and label from in-use variables
 *        
 * @param Preset button number 0-9, but not 4
 *
 * @return void
*******************************************************************************/

void RecallRTLPreset(int PresetButton)
{
  int index;

  // Transform button number into preset index
  index = PresetButton + 6;  // works for bottom row
  if (PresetButton > 4)      // second row
  {
    index = PresetButton - 4;
  }

  // Copy stored parameters into in-use parameters
  strcpy(RTLfreq[0], RTLfreq[index]);
  strcpy(RTLlabel[0], RTLlabel[index]);
  strcpy(RTLmode[0], RTLmode[index]);
  RTLsquelch[0] = RTLsquelch[index];
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM frequency
 *        
 * @param none
 *
 * @return void.  Sets global RTLfreq[0]
*******************************************************************************/

void ChangeRTLFreq()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new frequency in MHz:");
  strcpy(InitText, RTLfreq[0]);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2500");
  while ((atof(KeyboardReturn) < 1) || (atof(KeyboardReturn) > 2000))
  {
    Keyboard(RequestText, InitText, 11);
  }

  // Store Response
  strcpy(RTLfreq[0], KeyboardReturn);
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM Squelch setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLsquelch[0] in range 0 - 1000
*******************************************************************************/

void ChangeRTLSquelch()
{
  char RequestText[64];
  char InitText[64];

  //Define request string 
  strcpy(RequestText, "Enter new squelch setting 0 (off) to 1000 (fully on):");
  snprintf(InitText, 4, "%d", RTLsquelch[0]);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2000");
  while ((atoi(KeyboardReturn) < 0) || (atoi(KeyboardReturn) > 1000))
  {
    Keyboard(RequestText, InitText, 3);
  }

  // Store Response
  RTLsquelch[0] = atoi(KeyboardReturn);
}

/***************************************************************************//**
 * @brief Uses keyboard to ask for a new RTL-FM ppm setting
 *        
 * @param none
 *
 * @return void.  Sets global int RTLppm in range -1000 - 1000
*******************************************************************************/

void ChangeRTLppm()
{
  char RequestText[64];
  char InitText[64];
  char Value[15];
  char Param[15];

  //Define request string 
  strcpy(RequestText, "Enter new ppm setting -1000 to 1000:");
  snprintf(InitText, 4, "%d", RTLppm);

  // Ask for response and check validity
  strcpy(KeyboardReturn, "2000");
  while ((atoi(KeyboardReturn) < -1000) || (atoi(KeyboardReturn) > 1000))
  {
    Keyboard(RequestText, InitText, 4);
  }

  // Store Response in global variable
  RTLppm = atoi(KeyboardReturn);

  // Save Response in file
  strcpy(Param, "roffset");
  snprintf(Value, 6, "%d", RTLppm);
  SetConfigParam(PATH_RTLPRESETS, Param, Value);
}


/***************************************************************************//**
 * @brief Reads the Presets from rtl-fm_config.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadRTLPresets()
{
  int n;
  char Value[15] = "";
  char Param[15];

  for(n = 0; n < 10; n = n + 1)
  {
    // Title
    snprintf(Param, 10, "r%dtitle", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpy(RTLlabel[n], Value);

    // Frequency
    snprintf(Param, 10, "r%dfreq", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpy(RTLfreq[n], Value);

    // Mode
    snprintf(Param, 10, "r%dmode", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    strcpy(RTLmode[n], Value);

    // Squelch
    snprintf(Param, 10, "r%dsquelch", n);
    GetConfigParam(PATH_RTLPRESETS, Param, Value);
    RTLsquelch[n] = atoi(Value);
  }

  // Frequency correction ppm
  strcpy(Param, "roffset");
  GetConfigParam(PATH_RTLPRESETS, Param, Value);
  RTLppm = atoi(Value);
}

/***************************************************************************//**
 * @brief Reads the Presets from rx_config.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadRXPresets()
{
  int n;
  char Value[15] = "";
  char Param[20];

  for(n = 0; n < 5; n = n + 1)
  {
    // Frequency
    snprintf(Param, 15, "rx%dfrequency", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfreq[n], Value);

    // SR
    snprintf(Param, 15, "rx%dsr", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXsr[n] = atoi(Value);

    // FEC
    snprintf(Param, 15, "rx%dfec", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfec[n], Value);

    // Sample Rate
    snprintf(Param, 15, "rx%dsamplerate", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXsamplerate[n] = atoi(Value);

    // Gain
    snprintf(Param, 15, "rx%dgain", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    RXgain[n] = atoi(Value);

    // Modulation
    snprintf(Param, 15, "rx%dmodulation", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXmodulation[n], Value);

    // Encoding
    snprintf(Param, 15, "rx%dencoding", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXencoding[n], Value);

    // SDR Type
    snprintf(Param, 15, "rx%dsdr", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXsdr[n], Value);

    // Graphics on/off
    snprintf(Param, 15, "rx%dgraphics", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXgraphics[n], Value);

    // Parameters on/off
    snprintf(Param, 15, "rx%dparameters", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXparams[n], Value);

    // Sound on/off
    snprintf(Param, 15, "rx%dsound", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXsound[n], Value);

    // Fastlock on/off
    snprintf(Param, 15, "rx%dfastlock", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXfastlock[n], Value);

    // Label
    snprintf(Param, 12, "rx%dlabel", n);
    GetConfigParam(PATH_RXPRESETS, Param, Value);
    strcpy(RXlabel[n], Value);
  }
}

/***************************************************************************//**
 * @brief Saves Current LeanDVB Config to file as Preset 0
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void SaveCurrentRX()
{
  char Param[255];
  char Value[255];
  int index = 0;

  // Save the current values into the presets File
  snprintf(Param, 15, "rx%dfrequency", index);          // Frequency
  SetConfigParam(PATH_RXPRESETS, Param, RXfreq[0]);

  snprintf(Param, 15, "rx%dlabel", index);              // Label
  SetConfigParam(PATH_RXPRESETS, Param, RXlabel[0]);
    
  snprintf(Param, 15, "rx%dsr", index);                 // SR
  snprintf(Value, 5, "%d", RXsr[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dfec", index);                // FEC
  SetConfigParam(PATH_RXPRESETS, Param, RXfec[0]);

  snprintf(Param, 15, "rx%dsamplerate", index);         // Sample Rate
  snprintf(Value, 5, "%d", RXsamplerate[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dgain", index);               // Gain
  snprintf(Value, 5, "%d", RXgain[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dmodulation", index);         // Modulation
  SetConfigParam(PATH_RXPRESETS, Param, RXmodulation[0]);

  snprintf(Param, 15, "rx%dencoding", index);           // Encoding
  SetConfigParam(PATH_RXPRESETS, Param, RXencoding[0]);

  snprintf(Param, 15, "rx%dsdr", index);                // SDR Type
  SetConfigParam(PATH_RXPRESETS, Param, RXsdr[0]);

  snprintf(Param, 15, "rx%dgraphics", index);           // Graphics on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXgraphics[0]);

  snprintf(Param, 15, "rx%dparameters", index);         // Parameters on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXparams[0]);

  snprintf(Param, 15, "rx%dsound", index);              // Sound on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXsound[0]);

  snprintf(Param, 15, "rx%dfastlock", index);           // Fastlock on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXfastlock[0]);
}

/***************************************************************************//**
 * @brief Saves Current LeanDVB Config as a Preset
 *        
 * @param Preset button number 0-9, but not 4
 *
 * @return void
*******************************************************************************/

void SaveRXPreset(int PresetButton)
{
  char Param[255];
  char Value[255];
  char Prompt[63];
  int index;
  int Spaces = 1;
  int j;

  // Transform button number into preset index
  index = PresetButton + 1;  // works for bottom row

  // Read the current preset label and ask for a new value
  snprintf(Prompt, 62, "Enter the new label for LeanDVB Preset %d (no spaces):", index);

  // Check that there are no spaces
  while (Spaces >= 1)
  {
    Keyboard(Prompt, RXlabel[index], 10);

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
  strcpy(RXlabel[index], KeyboardReturn);
  snprintf(Param, 10, "rx%dlabel", index);
  SetConfigParam(PATH_RXPRESETS, Param, KeyboardReturn);

  // Copy the current values into the presets arrays
  strcpy(RXfreq[index], RXfreq[0]);               // String with frequency in MHz
  RXsr[index] = RXsr[0];                          // Symbol rate in K
  strcpy(RXfec[index], RXfec[0]);                 // FEC as String
  RXsamplerate[index] = RXsamplerate[0];          // Samplerate in K. 0 = auto
  RXgain[index] = RXgain[0];                      // Gain
  strcpy(RXmodulation[index], RXmodulation[0]);   // Modulation
  strcpy(RXencoding[index], RXencoding[0]);       // Encoding
  strcpy(RXsdr[index], RXsdr[0]);                 // SDR Type
  strcpy(RXgraphics[index], RXgraphics[0]);       // Graphics on/off
  strcpy(RXparams[index], RXparams[0]);           // Parameters on/off
  strcpy(RXsound[index], RXsound[0]);             // Sound on/off
  strcpy(RXfastlock[index], RXfastlock[0]);       // Fastlock on/off

  // Save the current values into the presets File for the preset
  snprintf(Param, 15, "rx%dfrequency", index);          // Frequency
  SetConfigParam(PATH_RXPRESETS, Param, RXfreq[0]);
    
  snprintf(Param, 15, "rx%dsr", index);                 // SR
  snprintf(Value, 5, "%d", RXsr[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dfec", index);                // FEC
  SetConfigParam(PATH_RXPRESETS, Param, RXfec[0]);

  snprintf(Param, 15, "rx%dsamplerate", index);         // Sample Rate
  snprintf(Value, 5, "%d", RXsamplerate[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dgain", index);               // Gain
  snprintf(Value, 5, "%d", RXgain[0]);
  SetConfigParam(PATH_RXPRESETS, Param, Value);

  snprintf(Param, 15, "rx%dmodulation", index);         // Modulation
  SetConfigParam(PATH_RXPRESETS, Param, RXmodulation[0]);

  snprintf(Param, 15, "rx%dencoding", index);           // Encoding
  SetConfigParam(PATH_RXPRESETS, Param, RXencoding[0]);

  snprintf(Param, 15, "rx%dsdr", index);                // SDR Type
  SetConfigParam(PATH_RXPRESETS, Param, RXsdr[0]);

  snprintf(Param, 15, "rx%dgraphics", index);           // Graphics on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXgraphics[0]);

  snprintf(Param, 15, "rx%dparameters", index);         // Parameters on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXparams[0]);

  snprintf(Param, 15, "rx%dsound", index);              // Sound on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXsound[0]);

  snprintf(Param, 15, "rx%dfastlock", index);           // Fastlock on/off
  SetConfigParam(PATH_RXPRESETS, Param, RXfastlock[0]);
}

/***************************************************************************//**
 * @brief Loads LeanDVB settings from in-use variables which store preset
 *  and saves them to file as Preset 0    
 * @param Preset button number 0-3
 *
 * @return void
*******************************************************************************/

void RecallRXPreset(int PresetButton)
{
  int index;

  // Transform button number into preset index
  index = PresetButton + 1;  // works for bottom row

  // Copy stored parameters into in-use parameters
  strcpy(RXfreq[0], RXfreq[index]);               // String with frequency in MHz
  strcpy(RXlabel[0], RXlabel[index]);             // String for label
  RXsr[0] = RXsr[index];                          // Symbol rate in K
  strcpy(RXfec[0], RXfec[index]);                 // FEC as String
  RXsamplerate[0] = RXsamplerate[index];          // Samplerate in K. 0 = auto
  RXgain[0] = RXgain[index];                      // Gain
  strcpy(RXmodulation[0], RXmodulation[index]);   // Modulation
  strcpy(RXencoding[0], RXencoding[index]);       // Encoding
  strcpy(RXsdr[0], RXsdr[index]);                 // SDR Type
  strcpy(RXgraphics[0], RXgraphics[index]);       // Graphics on/off
  strcpy(RXparams[0], RXparams[index]);           // Parameters on/off
  strcpy(RXsound[0], RXsound[index]);             // Sound on/off
  strcpy(RXfastlock[0], RXfastlock[index]);       // Fastlock on/off

  // Save the new values as preset 0
  SaveCurrentRX();
}

/***************************************************************************//**
 * @brief Loads the current transmit settings to LeanDVB
 *  and saves them to file as Preset 0    
 * @param Preset button number 0-3
 *
 * @return void
*******************************************************************************/

void SetRXLikeTX()
{
  char Param[255];
  char Value[255];

  // Copy TX parameters into in-use LeanDVB parameters

  strcpy(Param, "freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(RXfreq[0], Value);                       // String with frequency in MHz
  strcpy(RXlabel[0], "As_TX");                    // String for label
  strcpy(Param, "symbolrate");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  RXsr[0] = atoi(Value);                          // Symbol rate in K
  strcpy(Param, "fec");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  strcpy(RXfec[0], Value);                        // FEC as String
  RXsamplerate[0] = 0;                            // Samplerate: 0 = auto
  RXgain[0] = 0;                                  // Gain: 0 = auto
  strcpy(RXmodulation[0], "DVB-S");               // Modulation = DVB-S
  strcpy(RXencoding[0], CurrentEncoding);         // Encoding
  strcpy(RXsdr[0], "RTLSDR");                     // SDR Type
  strcpy(RXgraphics[0], "ON");                    // Graphics on/off
  strcpy(RXparams[0], "ON");                      // Parameters on/off
  strcpy(RXsound[0], "OFF");                      // Sound on/off
  strcpy(RXfastlock[0], "OFF");                   // Fastlock on/off

  // Save the new values as preset 0
  SaveCurrentRX();
}


/***************************************************************************//**
 * @brief Sets the RTLmode after a button press
 *
 * @param NoButton = button pressed in range 10 - 14  
 *
 * @return void. Works on the global variable RTLmode[0]
*******************************************************************************/

void SelectRTLmode(int NoButton)
{
  switch (NoButton)
  {
  case 10:
    strcpy(RTLmode[0], "am");
    break;
  case 11:
    strcpy(RTLmode[0], "fm");
    break;
  case 12:
    strcpy(RTLmode[0], "wbfm");
    break;
  case 13:
    strcpy(RTLmode[0], "usb");
    break;
  case 14:
    strcpy(RTLmode[0], "lsb");
    break;
  }
}

/***************************************************************************//**
 * @brief Starts the RTL Receiver using the stored parameters
 *
 * @param nil.  Works on Globals
 *
 * @return void. 
*******************************************************************************/

void RTLstart()
{
  char fragment[15];

  if(RTLdetected == 1)
  {
    char rtlcall[256];
    char card[15];
    char mic[15];

    GetMicAudioCard(mic);
    if (strlen(mic) == 1)   // Use USB audio output if present
    {
      strcpy(card, mic);
    }
    else                    // Use RPi audio if no USB
    {
      GetPiAudioCard(card);
      IQAvailable = 0;     // Set flag to say transmit unavailable
    }
    strcpy(rtlcall, "(rtl_fm");
    snprintf(fragment, 12, " -M %s", RTLmode[0]);  // -M mode
    strcat(rtlcall, fragment);
    snprintf(fragment, 12, " -f %sM", RTLfreq[0]); // -f frequencyM
    strcat(rtlcall, fragment);
    if (strcmp(RTLmode[0], "am") == 0)
    {
      strcat(rtlcall, " -s 12k");
    }
    if (strcmp(RTLmode[0], "fm") == 0)
    {
      strcat(rtlcall, " -s 12k");
    }
    if (RTLsquelchoveride == 1)
    {
      snprintf(fragment, 12, " -l %d", RTLsquelch[0]); // -l squelch
      strcat(rtlcall, fragment);
    }
    snprintf(fragment, 12, " -p %d", RTLppm); // -p ppm_error
    strcat(rtlcall, fragment);
    strcpy(fragment, " -E pad"); // -E pad so that aplay does not crash
    strcat(rtlcall, fragment);
    strcat(rtlcall, " | aplay -D plughw:");
    strcat(rtlcall, card);
    if (strcmp(RTLmode[0], "am") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r12) &"); // 12 KHz for AM
    }
    if (strcmp(RTLmode[0], "fm") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r12) &"); // 12 KHz for FM
    }
    if (strcmp(RTLmode[0], "wbfm") == 0)
    {
      strcat(rtlcall, ",0 -f S16_LE -r32) &"); // 32 KHz for WBFM
    }
    if ((strcmp(RTLmode[0], "usb") == 0) || (strcmp(RTLmode[0], "lsb") == 0))
    {
      strcat(rtlcall, ",0 -f S16_LE -r6) &"); // 6 KHz for SSB
    }
    system(rtlcall);
  }
  else
  {
    MsgBox("No RTL-SDR Connected");
    wait_touch();
  }
}

/***************************************************************************//**
 * @brief Stops the RTL Receiver and cleans up
 *
 * @param nil
 *
 * @return void. 
*******************************************************************************/

void RTLstop()
{
  system("sudo killall rtl_fm >/dev/null 2>/dev/null");
  system("sudo killall aplay >/dev/null 2>/dev/null");
  usleep(1000);
  system("sudo killall -9 rtl_fm >/dev/null 2>/dev/null");
  system("sudo killall -9 aplay >/dev/null 2>/dev/null");
}

/***************************************************************************//**
 * @brief Reads the Presets from stream_presets.txt and formats them for
 *        Display and switching
 *
 * @param None.  Works on global variables
 *
 * @return void
*******************************************************************************/

void ReadStreamPresets()
{
  int n;
  char Value[127] = "";
  char Param[20];

  for(n = 0; n < 9; n = n + 1)
  {
    // Stream Address
    snprintf(Param, 15, "stream%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamAddress[n], Value);

    // Stream Button Label
    snprintf(Param, 12, "label%d", n);
    GetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamLabel[n], Value);
  }
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
    strcat(BashText, "sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &");
    system(BashText);
    strcpy(BashText, "cd /home/pi");
    system(BashText);
    MsgBox4("Please wait 5 seconds", "while the DATV Express firmware", "is loaded", "");
    usleep(5000000);
    responseint = CheckExpressRunning();
    if (responseint == 0)  // Running OK
    {
      MsgBox4("", "", "", "DATV Express Firmware Loaded");
      usleep(1000000);
    }
    else
    {
      MsgBox4("Failed to load", "DATV Express firmware.", "Please check connections", "and try again");
      wait_touch();
    }
    BackgroundRGB(0,0,0,255);
  }
  else
  {
    responseint = 0;
  }
  return responseint;
}

/***************************************************************************//**
 * @brief Called on GUI start to check if the DATV Express Server needs to be started
 *
 * @param nil
 *
 * @return nil
*******************************************************************************/

void CheckExpress()
{
  //Check if DATV Express Required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // Startup mode is DATV Express
  {
    if (CheckExpressConnect() == 1)   // Not connected
    {
      MsgBox2("DATV Express Not connected", "Connect it now or select another mode");
      wait_touch();
      BackgroundRGB(0,0,0,255);
    }
    if (CheckExpressConnect() != 1)   // Connected
    {
      StartExpressServer();
    }
  }
}

/***************************************************************************//**
 * @brief Checks whether LimeSuite is installed
 *
 * @param 
 *
 * @return 0 if installed, 1 if not
*******************************************************************************/

int CheckLimeInstalled()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("if [ ! -d \"/home/pi/LimeSuite/\" ]; then echo \"1\"; else echo \"0\"; fi", "r");
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
 * @brief Checks whether a Lime Mini is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckLimeConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q '0403:601f' ; echo $?", "r");
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
 * @brief Checks whether a Lime Mini is connected if selected
 *        and displays error message if not
 * @param 
 *
 * @return void
*******************************************************************************/

void CheckLimeReady()
{
  if (strcmp(CurrentModeOP, TabModeOP[8]) == 0)  // Lime mini Output selected
  {
    if (CheckLimeConnect() == 1)
    {
      MsgBox2("No LimeMini Detected", "Check Connections");
      wait_touch();
    }
  }
  if (strcmp(CurrentModeOP, TabModeOP[3]) == 0)  // Lime USB Output selected
  {
    if (CheckLimeConnect() == 1)
    {
      MsgBox2("No Lime USB Detected", "Check Connections");
      wait_touch();
    }
  }
}

void MPEG2License()
{
  //char Param[31];
  //char Value[31];
  char Prompt[63];
  char serialstring[63];
  char keystring[63];

  // Check and display validity here

  // Compose the prompt
  GetRPiSerial(serialstring);
  strcpy(Prompt, "Enter key for ");
  strcat(Prompt, serialstring);

  // Look for existing key
  GetMPEGKey(keystring);
  
  Keyboard(Prompt, keystring, 15);

  // If key is unchanged, return without doing anything
  if(strcmp(KeyboardReturn, keystring) == 0)
  {
    return;
  }

  // If key line exists in /boot/config.txt, overwrite with new entry

  // If key line does not exist, add it.
  NewMPEGKey(KeyboardReturn);

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

int IsImageToBeChanged(int x,int y)
{
  // Returns -1 for LHS touch, 0 for centre and 1 for RHS

  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map
  CorrectTouchMap();            // Calibrates each individual screen

  if (scaledY >= hscreen/2)
  {
    return 0;
  }
  if (scaledX <= wscreen/8)
  {
    return -1;
  }
  else if (scaledX >= 7 * wscreen/8)
  {
    return 1;
  }
  else
  {
    return 0;
  }
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
      case 5:                  // Clear
          y = 0;
          x = 0;
          w = 2 * wscreen/12;
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
  
    // Draw overlay button.  Menus 1 or 4, 2 lines and Button status = 0 only
    if (((CurrentMenu == 1) || (CurrentMenu == 4)) && (Button->NoStatus == 0))
    {
      Fill(Button->Status[1].Color.r, Button->Status[1].Color.g, Button->Status[1].Color.b, 1);
      Roundrect(Button->x,Button->y,Button->w,Button->h/2, Button->w/10, Button->w/10);
      Fill(255, 255, 255, 1);				   // White text
      TextMid(Button->x+Button->w/2, Button->y+Button->h* 3/16, line2, SansTypeface, 18);
    }
  }
  else                                              // One line only
  {
    if ((CurrentMenu <= 10) && (CurrentMenu != 4))
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
  int i;
  int first;
  int last;

  // Set the background colour
  // Maybe a select statement here in future?
  if (CurrentMenu == 1)
  {
    BackgroundRGB(255,255,255,255);
  }

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
    if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
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

void EnforceValidTXMode()
{
  char Param[15]="modulation";

  if ((strcmp(CurrentModeOP, TabModeOP[3]) != 0) && (strcmp(CurrentModeOP, TabModeOP[8]) != 0)) // not Lime Mini or USB
  {
    if ((strcmp(CurrentTXMode, TabTXMode[0]) != 0) && (strcmp(CurrentTXMode, TabTXMode[1]) != 0))  // Not DVB-S and not Carrier
    {
      strcpy(CurrentTXMode, TabTXMode[0]);
      printf("************** Set TXMode = %s\n", CurrentTXMode);
      SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);
    }
  }
}

void EnforceValidFEC()
{
  int FECChanged = 0;
  char Param[7]="fec";
  char Value[7];

  if ((strcmp(CurrentTXMode, TabTXMode[0]) == 0) || (strcmp(CurrentTXMode, TabTXMode[1]) == 0)) // Carrier or DVB-S
  {
    if (fec > 10)
    {
      fec = 7;
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[2]) == 0)  // DVB-S2 QPSK
  {
    if (fec < 9)
    {
      fec = 91;
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[3]) == 0)  // 8PSK
  {
    if (fec < 20)
    {
      fec = 91;
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[4]) == 0)  // 16APSK
  {
    if ((fec < 20) || (fec == 35))
    {
      fec = 91;
      FECChanged = 1;
    }
  }
  if (strcmp(CurrentTXMode, TabTXMode[5]) == 0)  // 32APSK
  {
    if ((fec < 30) || (fec == 35))
    {
      fec = 91;
      FECChanged = 1;
    }
  }
  // and save to config
  if (FECChanged == 1)
  {
    sprintf(Value, "%d", fec);
    printf("************** Set FEC = %s\n",Value);
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }
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
    if ((strcmp(CurrentModeOP, "STREAMER") == 0)\
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
      // If no attenuator and not DATV Express or Lime then Grey out Atten Level
      if ((strcmp(CurrentAtten, "NONE") == 0) 
        && (strcmp(CurrentModeOP, "DATVEXPRESS") != 0) 
        && (strcmp(CurrentModeOP, TabModeOP[3]) != 0) 
        && (strcmp(CurrentModeOP, TabModeOP[8]) != 0))
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

void GreyOutReset11()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 0), 0); // S2 QPSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 1), 0); // 8PSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 2), 0); // 16 APSK
  SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // 32APSK
}

void GreyOut11()
{
  if ((strcmp(CurrentModeOP, TabModeOP[3]) != 0) && (strcmp(CurrentModeOP, TabModeOP[8]) != 0)) // not Lime Mini or USB
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 0), 2); // S2 QPSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 1), 2); // 8PSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 2), 2); // 16 APSK
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // 32APSK
  }
}

void GreyOutReset13()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 3), 0); // Lime Mini
  SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // Lime USB
}

void GreyOut13()
{
  if (CheckLimeInstalled() == 1)  // Lime not installed so GreyOut
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 3), 2); // Lime Mini
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // Lime USB
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

void GreyOutReset25()
{
  SetButtonStatus(ButtonNumber(CurrentMenu, 5), 0); // FEC 1/4
  SetButtonStatus(ButtonNumber(CurrentMenu, 6), 0); // FEC 1/3
  SetButtonStatus(ButtonNumber(CurrentMenu, 7), 0); // FEC 1/2
  SetButtonStatus(ButtonNumber(CurrentMenu, 8), 0); // FEC 3/5
  SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0); // FEC 2/3
}

void GreyOut25()
{
  if (strcmp(CurrentTXMode, TabTXMode[3]) == 0)  // 8PSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
  }
  if (strcmp(CurrentTXMode, TabTXMode[4]) == 0)  // 16APSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // FEC 3/5
  }
  if (strcmp(CurrentTXMode, TabTXMode[5]) == 0)  // 32APSK
  {
    SetButtonStatus(ButtonNumber(CurrentMenu, 5), 2); // FEC 1/4
    SetButtonStatus(ButtonNumber(CurrentMenu, 6), 2); // FEC 1/3
    SetButtonStatus(ButtonNumber(CurrentMenu, 7), 2); // FEC 1/2
    SetButtonStatus(ButtonNumber(CurrentMenu, 8), 2); // FEC 3/5
    SetButtonStatus(ButtonNumber(CurrentMenu, 9), 2); // FEC 2/3
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
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton > 3)  // Correct numbering
  {
    NoButton = NoButton - 5;
  }
  else
  {
    NoButton = NoButton + 2;
  }
  strcpy(CurrentTXMode, TabTXMode[NoButton]);
  char Param[15]="modulation";
  SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);
  EnforceValidTXMode();
  EnforceValidFEC();
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
  // Stop or reset DATV Express Server if required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // mode was DATV Express
  {
    system("sudo killall express_server >/dev/null 2>/dev/null");
    system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
  }

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(ModeOP, TabModeOP[NoButton - 5]);
  printf("************** Set Output Mode = %s\n",ModeOP);
  char Param[15]="modeoutput";
  SetConfigParam(PATH_PCONFIG, Param, ModeOP);

  // Set the Current Mode Output variable
  strcpy(CurrentModeOP, TabModeOP[NoButton - 5]);
  strcpy(CurrentModeOPtext, TabModeOPtext[NoButton - 5]);

  // Start DATV Express if required
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // new mode is DATV Express
  {
    StartExpressServer();
  }
  EnforceValidTXMode();
  EnforceValidFEC();

  // Check Lime Connected if selected
  CheckLimeReady();
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

  if (CallingMenu == 1)  // Transmit Frequency
  {
    char Param[] = "freqoutput";
    printf("************** Set Frequency = %s\n",freqtxt);
    SetConfigParam(PATH_PCONFIG, Param, freqtxt);

    DoFreqChange();
  }
  else                    // Lean DVB Receive frequency
  {
    strcpy(RXfreq[0], freqtxt);              
    SetConfigParam(PATH_RXPRESETS, "rx0frequency", RXfreq[0]);
  }
}

void SelectSR(int NoButton)  // Symbol Rate
{
  char Value[255];

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton < 4)
  {
    NoButton = NoButton + 10;
  }

  if (CallingMenu == 1)  // Transmit SR
  {
    SR = TabSR[NoButton - 5];
    sprintf(Value, "%d", SR);
    printf("************** Set Transmit SR = %s\n",Value);
    SetConfigParam(PATH_PCONFIG, "symbolrate", Value);
  }
  else                    // Lean DVB Receive SR
  {
    RXsr[0] = TabSR[NoButton - 5];
    sprintf(Value, "%d", RXsr[0]);
    printf("************** Set Receive SR = %s\n",Value);
    SetConfigParam(PATH_RXPRESETS, "rx0sr", Value);
  }
}

void SelectFec(int NoButton)  // FEC
{
  char Param[7]="fec";
  char Value[255];
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  if (CallingMenu == 1)  // Transmit FEC
  {
    fec = TabFec[NoButton - 5];
    sprintf(Value, "%d", fec);
    printf("************** Set Transmit FEC = %s\n",Value);
    SetConfigParam(PATH_PCONFIG, Param, Value);
  }
  else                    // Lean DVB Receive SR
  {
    sprintf(Value, "%d", TabFec[NoButton - 5]);
    strcpy(RXfec[0], Value);
    printf("************** Set Receive FEC = %s\n",Value);
    SetConfigParam(PATH_RXPRESETS, "rx0fec", Value);
  }
}

void SelectS2Fec(int NoButton)  // DVB-S2 FEC
{
  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);
  if (NoButton > 3)  // Correct numbering
  {
    NoButton = NoButton - 5;
  }
  else
  {
    NoButton = NoButton + 5;
  }
  fec = TabS2Fec[NoButton];
  EnforceValidFEC();
  char Param[7]="fec";
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
  int LimeGain = -1;
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

  // Lime Gain
  if (CheckLimeInstalled() == 0)
  {
    strcpy(Param, TabBand[band]);
    strcat(Param, "limegain");
    GetConfigParam(PATH_PPRESETS, Param, Value);
    while ((LimeGain < 0) || (LimeGain > 100))
    {
      snprintf(Prompt, 63, "Set the Lime Gain for the %s Band:", TabBandLabel[band]);
      Keyboard(Prompt, Value, 3);
      LimeGain = atoi(KeyboardReturn);
    }
    TabBandLimeGain[band] = LimeGain;
    SetConfigParam(PATH_PPRESETS ,Param, KeyboardReturn);
  }

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

  // Set the Modulation in portsdown_config.txt
  strcpy(Param,"modulation");    
  SetConfigParam(PATH_PCONFIG, Param, CurrentTXMode);

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

  // Lime Gain
  if (CheckLimeInstalled() == 0)
  {
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "limegain");
    GetConfigParam(PATH_PPRESETS, Param, Value);
 
    TabBandLimeGain[CurrentBand] = atoi(Value);

    strcpy(Param, "limegain");
    SetConfigParam(PATH_PCONFIG ,Param, Value);
  }

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

  // Now check if the Receive upconverter LO needs to be started or stopped
  ReceiveLOStart();  
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
  int LimeGain = -1;

  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // DATV Express
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
  else if ((strcmp(CurrentModeOP, TabModeOP[3]) == 0) || (strcmp(CurrentModeOP, TabModeOP[8]) == 0))  // Lime Mini or USB
  {
    while ((LimeGain < 0) || (LimeGain > 100))
    {
      snprintf(Prompt, 62, "Set the Lime Gain for the %s Band:", TabBandLabel[CurrentBand]);
      snprintf(Value, 4, "%d", TabBandLimeGain[CurrentBand]);
      Keyboard(Prompt, Value, 3);
      LimeGain = atoi(KeyboardReturn);
    }
    TabBandLimeGain[CurrentBand] = LimeGain;
    strcpy(Param, TabBand[CurrentBand]);
    strcat(Param, "limegain");
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(Param, "limegain");
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

void SetReceiveLOFreq(int NoButton)
{
  char Param[31];
  char Value[31];
  char Prompt[63];
  int band;

  // Convert button number to band number
  band = NoButton + 5;  // this is correct only for lower line of buttons
  if (NoButton > 4)     // Upper line of buttons
  {
    band = NoButton - 5;
  }

  // Compose the prompt
  strcpy(Param, TabBand[band]);
  strcat(Param, "label");
  GetConfigParam(PATH_PPRESETS, Param, Value);
  snprintf(Prompt, 63, "Enter LO Freq (MHz) for %s Band (0 for off)", Value);
  
  // Look up the current value
  strcpy(Param, TabBand[band]);
  strcat(Param, "lo");
  GetConfigParam(PATH_RXPRESETS, Param, Value);

  Keyboard(Prompt, Value, 15);

  SetConfigParam(PATH_RXPRESETS ,Param, KeyboardReturn);
}

void SetSampleRate()
{
  char Prompt[63];
  char Value[15];
  int NewSampleRate = -1;
  snprintf(Value, 6, "%d", RXsamplerate[0]); 
  while ((NewSampleRate < 0) || (NewSampleRate > 2400))
  {
    snprintf(Prompt, 63, "Set the LeanDVB Sample Rate in KSamples/sec (0 = auto)");
    Keyboard(Prompt, Value, 6);
    NewSampleRate = atoi(KeyboardReturn);
  }
  RXsamplerate[0] = NewSampleRate;
  SetConfigParam(PATH_RXPRESETS, "rx0samplerate", KeyboardReturn);
}

void SetRXGain()
{
  char Prompt[63];
  char Value[15];
  int NewGain = -1;
  snprintf(Value, 6, "%d", RXgain[0]); 
  while ((NewGain < 0) || (NewGain > 100))
  {
    snprintf(Prompt, 63, "Set the LeanDVB Gain in the range 0 to 100 dB");
    Keyboard(Prompt, Value, 6);
    NewGain = atoi(KeyboardReturn);
  }
  RXgain[0] = NewGain;
  SetConfigParam(PATH_RXPRESETS, "rx0gain", KeyboardReturn);
}

void ToggleEncoding()
{
  if (strcmp(RXencoding[0], "H264") == 0)
  {
     if (CheckMPEG2() == 1)
    {
      strcpy(RXencoding[0], "MPEG-2");
    }
    else
    {
      MsgBox2("Please purchase and install the", "MPEG-2 decoder licence");
      wait_touch();
    }
  }
  else
  {
    strcpy(RXencoding[0], "H264");
  }
  SetConfigParam(PATH_RXPRESETS, "rx0encoding", RXencoding[0]);
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

  // Save the current modulation to Preset
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

void SelectVidSource(int NoButton)  // Video Source
{
  //SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  //SelectInGroupOnMenu(CurrentMenu, 0, 2, NoButton, 1);
  if (NoButton < 4) // allow for reverse numbering of rows
  {
    NoButton = NoButton + 10;
  }
  strcpy(CurrentVidSource, TabVidSource[NoButton - 5]);
  CompVidStart();
}

void ChangeVidBand(int NoButton)
{
  if (NoButton < 4) // Bottom row
  {
    CompVidBand = NoButton + 5;
  }
  else // second row
  {
    CompVidBand = NoButton - 5;
  }
}

void ReceiveLOStart()
{
  char Param[15];
  char Value[15];
  char bashCall[127];

  strcpy(Param, TabBand[CurrentBand]);
  strcat(Param, "lo");
  GetConfigParam(PATH_RXPRESETS, Param, Value);

  if(strcmp(Value, "0") != 0)  // Start the LO
  {
    strcpy(bashCall, "sudo /home/pi/rpidatv/bin/adf4351 ");
    strcat(bashCall, Value);
    strcat(bashCall, " ");
    strcat(bashCall, CurrentADFRef);
    strcat(bashCall, " 3"); //max level
  }
  else                        // Stop the LO
  {
    strcpy(bashCall, "sudo /home/pi/rpidatv/bin/adf4351 off");
  }
  system(bashCall);
}

void CompVidInitialise()
{
  VidPTT = 1;
  digitalWrite(GPIO_Band_LSB, LOW);
  digitalWrite(GPIO_Band_MSB, LOW);
  digitalWrite(GPIO_Tverter, LOW);
  digitalWrite(GPIO_PTT, HIGH);
  char mic[15] = "xx";
  // char card[15];
  char commnd[255];

  // Start the audio pipe from mic to USB Audio out socket
  GetMicAudioCard(mic);
  if (strlen(mic) == 1)
  {
    // GetPiAudioCard(card);
    strcpy(commnd, "arecord -D plughw:");
    strcat(commnd, mic);
  //strcat(commnd, ",0 -f cd - | /dev/null &");
  strcat(commnd, ",0 -f cd - | aplay -D plughw:");
  //strcat(commnd, card);
  strcat(commnd, mic);
  strcat(commnd, ",0 - &");
  system(commnd);
  //printf("%s\n", commnd);
  }
  else
  {
    printf("mic = %s, so not starting arecord\n", mic);
  }
}

void CompVidStart()
{
  char fbicmd[255];
  int TCDisplay = 1;
  int rawX, rawY, rawPressure;
  FILE *fp;
  char SnapIndex[256];
  int SnapNumber;
  int Snap;

  if (strcmp(CurrentVidSource, "Pi Cam") == 0)
  {
    finish();
    system("v4l2-ctl --set-fmt-overlay=left=0,top=0,width=656,height=512");
    system("v4l2-ctl --overlay=1 >/dev/null 2>/dev/null");
    //system("v4l2-ctl -p 25"); // Set framerate??
    strcpy(ScreenState, "VideoOut");
    wait_touch();
    system("v4l2-ctl --overlay=0");
  }

  if (strcmp(CurrentVidSource, "TCAnim") == 0)
  {
    finish();
    strcpy(fbicmd, "/home/pi/rpidatv/bin/tcanim1v16 \"/home/pi/rpidatv/video/*10\" ");
    strcat(fbicmd, " \"48\" \"72\" \"CQ\" \"CQ CQ CQ de ");
    strcat(fbicmd, CallSign);
    strcat(fbicmd, " - ATV on ");
    strcat(fbicmd, TabBandLabel[CompVidBand]);
    strcat(fbicmd, " \" &");
    system(fbicmd);
    strcpy(ScreenState, "VideoOut");
    wait_touch();
    system("sudo killall tcanim1v16");
  }

  if (strcmp(CurrentVidSource, "Contest") == 0)
  {
    // Delete any previous Contest image
    system("rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null");

    // Create the new image
    strcpy(fbicmd, "convert -size 720x576 xc:white ");
    strcat(fbicmd, "-gravity North -pointsize 125 -annotate 0 ");
    strcat(fbicmd, CallSign); 
    strcat(fbicmd, " -gravity Center -pointsize 200 -annotate 0 ");
    strcat(fbicmd, TabBandNumbers[CompVidBand]);
    strcat(fbicmd, " -gravity South -pointsize 75 -annotate 0 \"");
    strcat(fbicmd, Locator);
    strcat(fbicmd, "    ");
    strcat(fbicmd, TabBandLabel[CompVidBand]);
    strcat(fbicmd, "\" /home/pi/tmp/contest.jpg");
    system(fbicmd);

    // Make the display ready
    strcpy(ScreenState, "VideoOut");
    finish();

    strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest");
    strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
    system(fbicmd);
    wait_touch();
  }

  if (strcmp(CurrentVidSource, "TestCard") == 0)
  {
    // Make the display ready
    strcpy(ScreenState, "VideoOut");
    finish();

    // Delete any old test card with caption
    system("rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null");
    system("rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null");

    if (strcmp(CurrentCaptionState, "on") == 0)
    {
      // Compose the new card
      strcpy(fbicmd, "convert -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 ");
      strcat(fbicmd, CallSign); 
      strcat(fbicmd, " /home/pi/tmp/caption.png");
      system(fbicmd);

      strcpy(fbicmd, "convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png ");
      strcat(fbicmd, "-geometry +0+475 -composite /home/pi/tmp/tcf2.jpg");
      system(fbicmd);
    }
    else
    {
      system("cp /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/tcf2.jpg");
    }

    while ((TCDisplay == 1) || (TCDisplay == -1))
    {
      switch(ImageIndex)
      {
      case 0:
        system("sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null");
        break;
      case 1:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcc.jpg >/dev/null 2>/dev/null");
        break;
      case 2:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/pm5544.jpg >/dev/null 2>/dev/null");
        break;
      case 3:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/75cb.jpg >/dev/null 2>/dev/null");
        break;
      case 4:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/11g.jpg >/dev/null 2>/dev/null");
        break;
      case 5:
        system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/pb.jpg >/dev/null 2>/dev/null");
        break;
      }
      if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
      TCDisplay = IsImageToBeChanged(rawX, rawY);
      if (TCDisplay != 0)
      {
        ImageIndex = ImageIndex + TCDisplay;
        if (ImageIndex > ImageRange)
        {
          ImageIndex = 0;
        }
        if (ImageIndex < 0)
        {
          ImageIndex = ImageRange;
        }
      }
    }
  }

  if (strcmp(CurrentVidSource, "Snap") == 0)
  {
    // Make the display ready
    strcpy(ScreenState, "VideoOut");
    finish();

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
    Snap = SnapNumber - 1;

    while (((TCDisplay == 1) || (TCDisplay == -1)) && (SnapNumber != 0))
      {
        sprintf(SnapIndex, "%d", Snap);
        strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
        strcat(fbicmd, SnapIndex);
        strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
        system(fbicmd);

        if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

        TCDisplay = IsImageToBeChanged(rawX, rawY);
        if (TCDisplay != 0)
        {
        Snap = Snap + TCDisplay;
        if (Snap >= SnapNumber)
        {
          Snap = 0;
        }
        if (Snap < 0)
        {
          Snap = SnapNumber - 1;
        }
      }
    }
  }
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}

void CompVidStop()
{
  // Set PTT low
  VidPTT = 0;
  digitalWrite(GPIO_PTT, LOW);

  // Stop the audio relay
  system("killall arecord >/dev/null 2>/dev/null");
  system("killall aplay >/dev/null 2>/dev/null");

  // Reset the Band Switching
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using rpidatvconfig.txt
  usleep(100000);

}

void TransmitStart()
{
  // printf("Transmit Start\n");

  char Param[255];
  char Value[255];
  #define PATH_SCRIPT_A "/home/pi/rpidatv/scripts/a.sh >/dev/null 2>/dev/null"

  // Turn the VCO off in case it has been used for receive
  system("sudo /home/pi/rpidatv/bin/adf4351 off");

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

  // Stop Lime transmitting
  if((strcmp(ModeOutput, "LIMEMINI")==0) || (strcmp(ModeOutput, "LIMEUSB")==0))
  {
    system("sudo killall dvb2iq >/dev/null 2>/dev/null");
    system("sudo killall limetx >/dev/null 2>/dev/null");
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

  // Start the Receive LO Here
  ReceiveLOStart();

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

  while(FinishedButton2 == 0)
  {
    fread( fftin,sizeof(fftwf_complex),FFT_SIZE,pFileIQ);

    if((strcmp(RXgraphics[0], "ON") == 0))
    {
      fftwf_execute( plan );
      fseek(pFileIQ,(1200000-FFT_SIZE)*sizeof(fftwf_complex),SEEK_CUR);
    }
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

int CheckStream()
{
  // first check file exists, if not, return 1
  // then read first 5 characters of file
  // if Video, return 0
  // if Audio, return 2.
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
  else if (strcmp(response, "Audio") == 0)
  {
    return 2;
  }
  else
  {
    return 3;
  }
}

/*   Old Code!!
void DisplayStream(int NoButton)
{
  char OMXCommand[127];
  int NoPreset;
  //VGfloat shapecolor[4];
  //RGBA(255, 255, 128, 1, shapecolor);

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  printf("Starting Stream Display\n");
  FinishedButton = 0;
  BackgroundRGB(0, 0, 0, 0);
  End();

  strcpy(OMXCommand, "omxplayer ");
  strcat(OMXCommand, StreamAddress[NoPreset]);
  strcat(OMXCommand, " >/dev/null 2>/dev/null &");
  printf("starting omxplayer with command: %s\n", OMXCommand);
  IQAvailable = 0;         // Set flag to prompt user reboot before transmitting
  system(OMXCommand); 

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  while(FinishedButton == 0)
  {
    usleep(1000);
  }
  system("sudo killall omxplayer.bin"); 
  pthread_join(thbutton, NULL);
}
*/

void DisplayStream(int NoButton)
{
  int NoPreset;
  int StreamStatus;
  int count;
  char startCommand[255];
  char WaitMessage[63];

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  strcpy(startCommand, "/home/pi/rpidatv/scripts/omx.sh ");
  strcat(startCommand, StreamAddress[NoPreset]);
  strcat(startCommand, " &");

  strcpy(WaitMessage, "Waiting for Stream ");
  strcat(WaitMessage, StreamLabel[NoPreset]);

  printf("Starting Stream receiver ....\n");
  IQAvailable = 0;         // Set flag to prompt user reboot before transmitting
  FinishedButton = 0;
  BackgroundRGB(0, 0, 0, 0);
  End();
  DisplayHere(WaitMessage);

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  while (FinishedButton == 0)
  {
    // With no stream, this loop is executed about once every 10 seconds

    // first make sure that the stream status is not stale
    system("rm /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null");
    usleep(500000);

    // run the omxplayer script
 
    system(startCommand);

    StreamStatus = CheckStream();

    // = 0 Stream running
    // = 1 Not started yet
    // = 2 started but audio only
    // = 3 terminated
    
    // Now wait 10 seconds for omxplayer to respond
    // checking every 0.5 seconds.  It will time out at 5 seconds

    count = 0;
    while ((StreamStatus == 1) && (count < 20) && (FinishedButton == 0))
    {
      usleep(500000); 
      count = count + 1;
      StreamStatus = CheckStream();
    }

    // If it is running properly, wait here
    if (StreamStatus == 0)
    {
      DisplayHere("Valid Stream Detected");

      while (StreamStatus == 0 && (FinishedButton == 0))
      {
        // Wait in this loop while the stream is running
        usleep(500000); // Check every 0.5 seconds
        StreamStatus = CheckStream();
      }

      if (FinishedButton == 0)
      {
        DisplayHere("Stream Dropped Out");
        usleep(500000); // Display dropout message for 0.5 sec
      }
      else
      {
        DisplayHere(""); // Clear messages
      }
    }     

    if (StreamStatus == 2)  // Audio only
    {
      DisplayHere("Audio Stream Detected, Trying for Video");
    }

    if ((StreamStatus == 3) || (StreamStatus == 1))  // Nothing detected
    {
      DisplayHere(WaitMessage);
    }

    // Make sure that omxplayer is no longer running
    system("killall -9 omxplayer.bin >/dev/null 2>/dev/null");
  }
  DisplayHere("");
  pthread_join(thbutton, NULL);
}

void AmendStreamPreset(int NoButton)
{
  int NoPreset;
  char Param[255];
  char Value[255];
  char TestValue[255];
  char Prompt[63];

  if(NoButton < 5) // bottom row
  {
    NoPreset = NoButton + 5;
  }
  else  // top row
  {
    NoPreset = NoButton - 4;
  }

  // Read the Preset address and ask for a new address
  strcpy(TestValue, StreamAddress[NoPreset]);  // Read the full rtmp address
  TestValue[29] = '\0';                        // Shorten to 29 cahracters
  if (strcmp(TestValue, "rtmp://rtmp.batc.org.uk/live/") == 0) // BATC Address
  {
    snprintf(Prompt, 62, "Enter the new lower case StreamName for Preset %d:", NoPreset);
    strcpy(Value, StreamAddress[NoPreset]);  // Read the full rtmp address

    // Copy the text to the right of character 29 (the last /)
    strncpy(TestValue, &Value[29], strlen(Value));
    TestValue[strlen(Value) - 29] = '\0';
    // Ask user to edit streamname
    Keyboard(Prompt, TestValue, 15);
    // Put the full url back together
    strcpy(Value, "rtmp://rtmp.batc.org.uk/live/");
    strcat(Value, KeyboardReturn);
    snprintf(Param, 10, "stream%d", NoPreset);
    SetConfigParam(PATH_STREAMPRESETS, Param, Value);
    strcpy(StreamAddress[NoPreset], Value);

    // Read the Preset Label and ask for a new value
    snprintf(Prompt, 62, "Enter the new label for Preset %d:", NoPreset);
    strcpy(Value, StreamLabel[NoPreset]);  // Read the old label
    Keyboard(Prompt, Value, 15);
    snprintf(Param, 16, "label%d", NoPreset);
    SetConfigParam(PATH_STREAMPRESETS, Param, KeyboardReturn);
    strcpy(StreamLabel[NoPreset], KeyboardReturn);
  }
  else  // not a BATC Address
  {
    MsgBox4("Not a BATC Streamer Address", "Please edit it directly in", "rpidatv/scripts/stream_presets.txt", "Touch Screen to Continue");
    wait_touch();
  }
  BackgroundRGB(0, 0, 0, 255);  // Clear the background
}

void ProcessLeandvb2()
{
  #define PATH_SCRIPT_LEAN2 "sudo /home/pi/rpidatv/scripts/leandvbgui2.sh 2>&1"
  char *line=NULL;
  size_t len = 0;
  ssize_t read;
  FILE *fp;
  VGfloat shapecolor[4];
  RGBA(255, 255, 128, 1, shapecolor);

  printf("Entering LeanDVB Process\n");
  FinishedButton = 0;
  FinishedButton2 = 0;  // Enable the fft to run if required

  if((strcmp(RXgraphics[0], "ON") == 0) || (strcmp(RXparams[0], "ON") == 0))
  {
    // create FFT Thread 
    pthread_create (&thfft, NULL, &DisplayFFT, NULL);
  }

  // Create Wait Button thread
  pthread_create (&thbutton, NULL, &WaitButtonEvent, NULL);

  // Start RTL-SDR, LeanDVB and HelloVideo from BASH
  fp=popen(PATH_SCRIPT_LEAN2, "r");
  if(fp==NULL) printf("Process error\n");

  // Deal with each display case differently

  // No graphics or parameters
  if((strcmp(RXgraphics[0], "OFF") == 0) && (strcmp(RXparams[0], "OFF") == 0))
  {
    BackgroundRGB(0, 0, 0, 0);
    End();
    while (FinishedButton == 0)
    {
      usleep(1000);
      if(FinishedButton == 1)
      {
        printf("Trying to kill LeanDVB\n");
        system("(sudo killall -9 leandvb >/dev/null 2>/dev/null) &");
      }
    }
  }

  // Both graphics and parameters
  if((strcmp(RXgraphics[0], "ON") == 0) && (strcmp(RXparams[0], "ON") == 0))
  { 
    // While there is data, display it
    while (((read = getline(&line, &len, fp)) != -1) && (FinishedButton == 0))
    {
      char  strTag[20];
      int NbData;
      static int Decim = 0;
      sscanf(line,"%s ",strTag);
      char * token;
      static int Lock = 0;
      static float SignalStrength = 0;
      static float MER = 0;
      static float FREQ = 0;

      // Deal with the Symbol Data
      if(strcmp(strTag, "SYMBOLS")==0)
      {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sscanf(token,"%d",&NbData);

        if(Decim%25==0)  // Lock, signal strength and MER
        {
          BackgroundRGB(0, 0, 0, 0);

          // Lock status Rectangle (red/green with white text)
          char sLock[100];
          if(Lock == 1)
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
          Fill(255, 255, 255, 1);
          Text(200, 20, sLock, SerifTypeface, 25);

          // Signal Strength: White text to right of Lock status
          char sSignalStrength[100];
          sprintf(sSignalStrength, "%3.0f", SignalStrength);
          Fill(255-SignalStrength, SignalStrength, 0, 1);
          Roundrect(350, 0, 20+SignalStrength/2, 50, 10, 10);
          Fill(255, 255, 255, 1);
          Text(350, 20, sSignalStrength, SerifTypeface, 25);

          //MER: 2-30 to right of Sig Stength.  Bar length indicative.
          char sMER[100];
          sprintf(sMER, "%2.1fdB", MER);
          Fill(255-MER*8, (MER*8), 0, 1);
          Roundrect(500, 0, (MER*8), 50, 10, 10);
          Fill(255, 255, 255, 1);
          Text(500, 20, sMER, SerifTypeface, 25);

          // Frequency indicator bar
          Stroke(0, 0, 255, 0.8);
          Line(FFT_SIZE/2, 0, FFT_SIZE/2, 10);
          Stroke(0, 0, 255, 0.8);
          Line(0,hscreen-300,256,hscreen-300);
          StrokeWidth(10);
          Line(128+(FREQ/40000.0)*256.0,hscreen-300-20,128+(FREQ/40000.0)*256.0,hscreen-300+20);

          // Frequency text
          char sFreq[100];
          sprintf(sFreq,"%2.1fkHz",FREQ/1000.0);
          Text(0,hscreen-300+25, sFreq, SerifTypeface, 20);
        }

        if(Decim%25==0)
        {
          // Draw FFT
          static VGfloat PowerFFTx[FFT_SIZE];
          static VGfloat PowerFFTy[FFT_SIZE];
          StrokeWidth(2);
          Stroke(150, 150, 200, 0.8);
          int i;
          if(fftout != NULL)
          {
            for(i = 0; i < FFT_SIZE; i += 2)
            {
              PowerFFTx[i] = (i<FFT_SIZE/2)?(FFT_SIZE+i)/2:i/2;
              PowerFFTy[i] = log10f(sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])/FFT_SIZE)*100;	
              Line(PowerFFTx[i], 0, PowerFFTx[i], PowerFFTy[i]);
            }

            // Draw Constellation
            int x, y;
            Decim++;
            StrokeWidth(2);
            Stroke(255, 255, 128, 0.8);
            for(i = 0; i < NbData ; i++)
            {
              token = strtok(NULL, " ");
              sscanf(token, "%d, %d", &x, &y);
              coordpoint(x+128, hscreen-(y+128), 5, shapecolor); // dots

              Stroke(0, 255, 255, 0.8);
              Line(0, hscreen-128, 256, hscreen-128);
              Line(128, hscreen, 128, hscreen-256);  // Axis
            }
          }
          End();
        }
        else
        {
          Decim++;
        }
      }

      if((strcmp(strTag, "SS") == 0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&SignalStrength);
      }

      if((strcmp(strTag, "MER") == 0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&MER);
      }

      if((strcmp(strTag,"FREQ")==0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&FREQ);
      }

      if((strcmp(strTag,"LOCK")==0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%d",&Lock);
      }

      free(line);
      line=NULL;

      if(FinishedButton == 1)
      {
        printf("Trying to kill LeanDVB\n");
        system("(sudo killall -9 leandvb >/dev/null 2>/dev/null) &");
      }
    }
  }
 
  // Only graphics, no  parameters
  if((strcmp(RXgraphics[0], "ON") == 0) && (strcmp(RXparams[0], "OFF") == 0))
  { 
    // While there is data, display it
    while (((read = getline(&line, &len, fp)) != -1) && (FinishedButton == 0))
    {
      char  strTag[20];
      int NbData;
      static int Decim = 0;
      sscanf(line,"%s ",strTag);
      char * token;

      // Deal with the Symbol Data
      if(strcmp(strTag, "SYMBOLS")==0)
      {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sscanf(token,"%d",&NbData);

        if(Decim%25==0)
        {
          BackgroundRGB(0, 0, 0, 0);

          // Draw FFT
          static VGfloat PowerFFTx[FFT_SIZE];
          static VGfloat PowerFFTy[FFT_SIZE];
          StrokeWidth(2);
          Stroke(150, 150, 200, 0.8);
          int i;
          if(fftout != NULL)
          {
            for(i = 0; i < FFT_SIZE; i += 2)
            {
              PowerFFTx[i] = (i<FFT_SIZE/2)?(FFT_SIZE+i)/2:i/2;
              PowerFFTy[i] = log10f(sqrt(fftout[i][0]*fftout[i][0]+fftout[i][1]*fftout[i][1])/FFT_SIZE)*100;	
              Line(PowerFFTx[i], 0, PowerFFTx[i], PowerFFTy[i]);
            }
          }

          // Draw Constellation
          int x, y;
          Decim++;
          StrokeWidth(2);
          Stroke(255, 255, 128, 0.8);
          for(i = 0; i < NbData ; i++)
          {
            token = strtok(NULL, " ");
            sscanf(token, "%d, %d", &x, &y);
            coordpoint(x+128, hscreen-(y+128), 5, shapecolor); // dots

            Stroke(0, 255, 255, 0.8);
            Line(0, hscreen-128, 256, hscreen-128);
            Line(128, hscreen, 128, hscreen-256);  // Axis
          }
          End();
        }
        else
        {
          Decim++;
        }
      }

      free(line);
      line=NULL;

      if(FinishedButton == 1)
      {
        printf("Trying to kill LeanDVB\n");
        system("(sudo killall -9 leandvb >/dev/null 2>/dev/null) &");
      }
    }
  }

  // Only parameters, no graphics
  if((strcmp(RXgraphics[0], "OFF") == 0) && (strcmp(RXparams[0], "ON") == 0))
  { 
    // While there is data, display it
    while (((read = getline(&line, &len, fp)) != -1) && (FinishedButton == 0))
    {
      char  strTag[20];
      int NbData;
      static int Decim = 0;
      sscanf(line,"%s ",strTag);
      char * token;
      static int Lock = 0;
      static float SignalStrength = 0;
      static float MER = 0;
      static float FREQ = 0;

      // Deal with the Symbol Data
      if(strcmp(strTag, "SYMBOLS")==0)
      {

        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sscanf(token,"%d",&NbData);

        if(Decim%25==0)  // Lock, signal strength and MER
        {
          BackgroundRGB(0, 0, 0, 0);
          StrokeWidth(2);

          // Lock status Rectangle (red/green with white text)
          char sLock[100];
          if(Lock == 1)
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
          Fill(255, 255, 255, 1);
          Text(200, 20, sLock, SerifTypeface, 25);

          // Signal Strength: White text to right of Lock status
          char sSignalStrength[100];
          sprintf(sSignalStrength, "%3.0f", SignalStrength);
          Fill(255-SignalStrength, SignalStrength, 0, 1);
          Roundrect(350, 0, 20+SignalStrength/2, 50, 10, 10);
          Fill(255, 255, 255, 1);
          Text(350, 20, sSignalStrength, SerifTypeface, 25);

          //MER: 2-30 to right of Sig Stength.  Bar length indicative.
          char sMER[100];
          sprintf(sMER, "%2.1fdB", MER);
          Fill(255-MER*8, (MER*8), 0, 1);
          Roundrect(500, 0, (MER*8), 50, 10, 10);
          Fill(255, 255, 255, 1);
          Text(500, 20, sMER, SerifTypeface, 25);

          // Frequency indicator bar
          Stroke(0, 0, 255, 0.8);
          Line(FFT_SIZE/2, 0, FFT_SIZE/2, 10);
          Stroke(0, 0, 255, 0.8);
          Line(0,hscreen-300,256,hscreen-300);
          StrokeWidth(10);
          Line(128+(FREQ/40000.0)*256.0,hscreen-300-20,128+(FREQ/40000.0)*256.0,hscreen-300+20);

          // Frequency text
          char sFreq[100];
          sprintf(sFreq,"%2.1fkHz",FREQ/1000.0);
          Text(0,hscreen-300+25, sFreq, SerifTypeface, 20);
        }

        if(Decim%25==0)
        {
          // Do the data elements of Draw FFT

          int i;
          if(fftout != NULL)
          {
            // Draw Constellation
            int x, y;
            Decim++;
            for(i = 0; i < NbData ; i++)
            {
              token = strtok(NULL, " ");
              sscanf(token, "%d, %d", &x, &y);
            }
          }
          End();
        }
        else
        {
          Decim++;
        }
      }

      if((strcmp(strTag, "SS") == 0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&SignalStrength);
      }

      if((strcmp(strTag, "MER") == 0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&MER);
      }

      if((strcmp(strTag,"FREQ")==0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%f",&FREQ);
      }

      if((strcmp(strTag,"LOCK")==0))
      {
        token = strtok(line," ");
        token = strtok(NULL," ");
        sscanf(token,"%d",&Lock);
      }

      free(line);
      line=NULL;

      if(FinishedButton == 1)
      {
        printf("Trying to kill LeanDVB\n");
        system("(sudo killall -9 leandvb >/dev/null 2>/dev/null) &");
      }
    }
  }

  FinishedButton2 = 1;  // Stop the FFT running at a safe point in time

  system("(sudo killall -9 leandvb >/dev/null 2>/dev/null) &");
  system("(sudo killall rtl_sdr >/dev/null 2>/dev/null) &");

  system("sudo killall fbi >/dev/null 2>/dev/null");  // kill any previous images
  system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png");  // Add logo image

  pthread_join(thfft, NULL);
  pclose(fp);
  pthread_join(thbutton, NULL);
 
  system("sudo killall -9 hello_video.bin >/dev/null 2>/dev/null");
  system("sudo killall -9 hello_video2.bin >/dev/null 2>/dev/null");
  system("sudo killall fbi >/dev/null 2>/dev/null");
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall ts2es >/dev/null 2>/dev/null");
  finish();
}


void ReceiveStart2()
{
  strcpy(ScreenState, "RXwithImage");  //  Signal to display touch menu without further touch
  system("sudo killall hello_video.bin >/dev/null 2>/dev/null");
  system("sudo killall hello_video2.bin >/dev/null 2>/dev/null");
  ProcessLeandvb2();
}


void ReceiveStop()
{
  system("sudo killall leandvb >/dev/null 2>/dev/null");
  system("sudo killall -9 hello_video.bin >/dev/null 2>/dev/null");
  system("sudo killall -9 hello_video2.bin >/dev/null 2>/dev/null");
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
  if (CheckLimeInstalled() == 0)
  {
    strcat(swversion, " + Lime");
  }

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

  char Param[31];
  char Value[31];
  int pointsize = 20;

  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if (strcmp(Value,"Element14_7")==0)  // Reduce text size for 7 inch screen
  {
    pointsize = 15;
  }

  // Initialise and calculate the text display
  init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
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

void RangeBearing()
{
  char Param[31];
  char Value[31];
  char IntValue[31];
  int pointsize = 20;
  char DispName[10][20];
  char Locator[10][11];
  char MyLocator[11]="IO90LU";
  int Bearing[10];
  int Range[10];
  int i, j;
  int offset;
  char Prompt[63];
  bool IsValid = FALSE;

  // read which entry is currently top of the list
  GetConfigParam(PATH_LOCATORS, "index", Value);
  offset = atoi(Value);
  
  // Calculate the bottom of the list
  offset = offset - 1;
  if (offset < 0)
  {
    offset = offset + 10;
  }

  sprintf(Prompt, "Enter the callsign (select enter to view list)");
  Keyboard(Prompt, "", 19);

  if (strlen(KeyboardReturn) > 0)
  {
    sprintf(Param, "callsign%d", offset);
    SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

    strcpy(KeyboardReturn, "");
    while (IsValid == FALSE)
    {
      sprintf(Prompt, "Enter the Locator (6, 8 or 10 chars)");
      Keyboard(Prompt, KeyboardReturn, 10);
      IsValid = CheckLocator(KeyboardReturn);
    }

    sprintf(Param, "locator%d", offset);
    SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

    snprintf(Value, 2, "%d", offset);
    SetConfigParam(PATH_LOCATORS, "index", Value);
  }
  else
  {
    offset = offset + 1;
    if (offset > 9)
    {
      offset = offset - 10;
    }
  }

  // Reduce text size for 7 inch screen
  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  if (strcmp(Value,"Element14_7")==0)  
  {
    pointsize = 15;
  }

  // Initialise and calculate the text display
  init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.2 * (txtht + txtdp);

  // Read my locator
  strcpy(Param,"mylocator");
  GetConfigParam(PATH_LOCATORS, Param, MyLocator);

  // Read Callsigns and Locators from file, and calculate each r/b
  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "callsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    sprintf(Param, "locator%d", i);
    GetConfigParam(PATH_LOCATORS, Param, Locator[i]);

    Bearing[i] = CalcBearing(MyLocator, Locator[i]);
    Range[i] = CalcRange(MyLocator, Locator[i]);
  }

  // Display Title Text
  sprintf(Value, "From %s", CallSign);
  Text(wscreen * 1.0 / 40.0, hscreen - linepitch, Value, font, pointsize);
  Text(wscreen * 15.0 / 40.0, hscreen - linepitch, MyLocator, font, pointsize);
  Text(wscreen * 27.0 / 40.0, hscreen - linepitch, "Bearing", font, pointsize);
  Text(wscreen * 34.0 / 40.0, hscreen - linepitch, "Range", font, pointsize);

  // Display each row in turn
  for(i = 0; i < 10 ; i++)
  {
    // Correct for offset
    j = i + offset;
    if (j > 9)
    {
      j = j - 10;
    }

    Text(wscreen * 1.0 /40.0, hscreen - (i + 3) * linepitch, DispName[j], font, pointsize);
    Text(wscreen * 15.0 /40.0, hscreen - (i + 3) * linepitch, Locator[j], font, pointsize);
    snprintf(IntValue, 4, "%d", Bearing[j]);
    if (strlen(IntValue) == 3)
    {
       strcpy(Value, IntValue);
    }
    if (strlen(IntValue) == 2)
    {
      strcpy(Value, "0");
      strcat(Value, IntValue);
    }
    if (strlen(IntValue) == 1)
    {
      strcpy(Value, "00");
      strcat(Value, IntValue);
    }
    strcat(Value, " deg");
    Text(wscreen * 27.0 / 40.0, hscreen - (i + 3) * linepitch, Value, font, pointsize);
    sprintf(Value, "%d km", Range[j]);
    Text(wscreen * 34.0 / 40.0, hscreen - (i + 3) * linepitch, Value, font, pointsize);
  }

  TextMid(wscreen/2, 20, "Touch Screen to Continue",  font, pointsize);

  // Push text to screen
  End();

  printf("Locator Bearing called and waiting for touch\n");
  wait_touch();
}

void BeaconBearing()
{
  char Param[31];
  char Value[31];
  char IntValue[31];
  int pointsize = 20;
  char DispName[10][20];
  char Locator[10][11];
  char MyLocator[11]="IO90LU";
  int Bearing[10];
  int Range[10];
  int i;

  // Reduce text size for 7 inch screen
  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG, Param, Value);
  if (strcmp(Value,"Element14_7")==0)  
  {
    pointsize = 15;
  }

  // Initialise and calculate the text display
  init(&wscreen, &hscreen);  // Restart the gui
  BackgroundRGB(0,0,0,255);  // Black background
  Fill(255, 255, 255, 1);    // White text
  Fontinfo font = SansTypeface;
  VGfloat txtht = TextHeight(font, pointsize);
  VGfloat txtdp = TextDepth(font, pointsize);
  VGfloat linepitch = 1.2 * (txtht + txtdp);

  // Read Callsigns and Locators from file, and calculate each r/b
  strcpy(Param,"mylocator");
  GetConfigParam(PATH_LOCATORS, Param, MyLocator);
  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "bcallsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    sprintf(Param, "blocator%d", i);
    GetConfigParam(PATH_LOCATORS, Param, Locator[i]);

    Bearing[i] = CalcBearing(MyLocator, Locator[i]);
    Range[i] = CalcRange(MyLocator, Locator[i]);
  }

  // Display Title Text
  sprintf(Value, "From %s", CallSign);
  Text(wscreen * 1.0 / 40.0, hscreen - linepitch, Value, font, pointsize);
  Text(wscreen * 15.0 / 40.0, hscreen - linepitch, MyLocator, font, pointsize);
  Text(wscreen * 27.0 / 40.0, hscreen - linepitch, "Bearing", font, pointsize);
  Text(wscreen * 34.0 / 40.0, hscreen - linepitch, "Range", font, pointsize);

  // Display each row in turn
  for(i=0; i<10 ;i++)
  {
    Text(wscreen * 1.0 /40.0, hscreen - (i + 3) * linepitch, DispName[i], font, pointsize);
    Text(wscreen * 15.0 /40.0, hscreen - (i + 3) * linepitch, Locator[i], font, pointsize);
    snprintf(IntValue, 4, "%d", Bearing[i]);
    if (strlen(IntValue) == 3)
    {
       strcpy(Value, IntValue);
    }
    if (strlen(IntValue) == 2)
    {
      strcpy(Value, "0");
      strcat(Value, IntValue);
    }
    if (strlen(IntValue) == 1)
    {
      strcpy(Value, "00");
      strcat(Value, IntValue);
    }
    strcat(Value, " deg");
    Text(wscreen * 27.0 / 40.0, hscreen - (i + 3) * linepitch, Value, font, pointsize);
    sprintf(Value, "%d km", Range[i]);
    Text(wscreen * 34.0 / 40.0, hscreen - (i + 3) * linepitch, Value, font, pointsize);
  }

  TextMid(wscreen/2, 20, "Touch Screen to Continue",  font, pointsize);

  // Push to screen
  End();

  printf("Beacon Bearing called and waiting for touch\n");
  wait_touch();
}

void AmendBeacon(int i)
{
  char Param[15];
  char Value[31];
  char Prompt[63];
  bool IsValid = FALSE;

  // Correct button number to site number 
  i = i - 5;
  if (i < 0)
  {
    i = i + 10;
  }
  printf("Amend Beacon %d\n", i);

  // Retrieve amend and save the details
  sprintf(Param, "bcallsign%d", i);
  GetConfigParam(PATH_LOCATORS, Param, Value);
  sprintf(Prompt, "Enter the new name for the site/beacon (no spaces)");
  Keyboard(Prompt, Value, 19);
  SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);

  sprintf(Param, "blocator%d", i);
  GetConfigParam(PATH_LOCATORS, Param, Value);
  sprintf(Prompt, "Enter the locator for this new Site/beacon");

  while (IsValid == FALSE)
  {
    Keyboard(Prompt, Value, 10);
    IsValid = CheckLocator(KeyboardReturn);
  }
  SetConfigParam(PATH_LOCATORS, Param, KeyboardReturn);
}

int CalcBearing(char *myLocator, char *remoteLocator)
{
  float myLat;
  float myLong;
  float remoteLat;
  float remoteLong;
  int Bearing;

  myLat = Locator_To_Lat(myLocator);
  myLong = Locator_To_Lon(myLocator);
  remoteLat = Locator_To_Lat(remoteLocator);
  remoteLong = Locator_To_Lon(remoteLocator);

  //printf(" Calc Bearing: My Locator %s, myLat %f, myLong %f\n", myLocator, myLat, myLong);

  Bearing =  GcBearing(myLat, myLong, remoteLat, remoteLong);
  return Bearing;
}

int CalcRange(char *myLocator, char *remoteLocator)
{
  float myLat;
  float myLong;
  float remoteLat;
  float remoteLong;
  int Range;

  myLat = Locator_To_Lat(myLocator);
  myLong = Locator_To_Lon(myLocator);
  remoteLat = Locator_To_Lat(remoteLocator);
  remoteLong = Locator_To_Lon(remoteLocator);
  Range = (int)GcDistance(myLat, myLong, remoteLat, remoteLong, "K");

  return Range;
}

bool CheckLocator(char *Loc)
{
  // Returns false if locator not valid format

  char szLoc[32] = {0};
  int V;
  int n;
  bool bRet = TRUE;
  int LocLength;

  strcpy(szLoc, Loc);
  LocLength = strlen(szLoc);
  if ((LocLength < 4) || (LocLength > 10))
  {
    bRet = FALSE;
    return bRet;
  }
  switch(LocLength)
  {
  case 4:
    strcat(szLoc, "LL55AA");
    break;
  case 6:
    strcat(szLoc, "55AA");
    break;
  case 8:
    strcat(szLoc, "LL");
    break;
  case 5:
  case 7:
  case 9:
    bRet = FALSE;
    return bRet;
  }

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];

    // Check Values
    if ((n==0) || (n==1) || (n==4) || (n==5) || (n==8) || (n==9))
    {
      if( (V < 'A') || (V > 'X') )
      {
        bRet = FALSE;
      }
    }
    else
    {
      if( V > '9' )
      {
        bRet = FALSE;
      }
    }
  }
  return bRet;
}

float Locator_To_Lat(char *Loc)
{
  char szLoc[32] = {0};
  int V;
  float P[10] = {0};
  float Lat;
  unsigned int n;
  bool bRet = TRUE;

  strcpy(szLoc,Loc);

  if (strlen(szLoc) < 4 )
  {
    bRet = FALSE;
    return bRet;
  }
  if( (strlen(szLoc) == 4) )
  {
    strcat(szLoc, "LL55AA");
  }
  if( (strlen(szLoc) == 6) )
  {
    strcat(szLoc, "55AA");
  }
  if( (strlen(szLoc) == 8) )
  {
    strcat(szLoc, "LL");
  }

  bRet = CheckLocator(szLoc);

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];
    if( V < 'A' )
    {
      P[n] = V - '0';
    }
    else
    {
      P[n] = V - 'A';
    }
  }

  if ( bRet )
  {
    Lat = (P[1]*10) + P[3] + (P[5]/24) + (P[7]/240) + (P[9]/5760) - 90;
  }
  return Lat;
}

float Locator_To_Lon(char *Loc)
{
  char szLoc[32] = {0};
  int V;
  float P[10] = {0};
  float Lon;
  unsigned int n;
  bool bRet = TRUE;

  strcpy(szLoc, Loc);

  if (strlen(szLoc) < 4 )
  {
    bRet = FALSE;
    return bRet;
  }
  if( (strlen(szLoc) == 4) )
  {
    strcat(szLoc, "LL55AA");
  }
  if( (strlen(szLoc) == 6) )
  {
    strcat(szLoc, "55AA");
  }
  if( (strlen(szLoc) == 8) )
  {
    strcat(szLoc, "LL");
  }

  bRet = CheckLocator(szLoc);

  for( n = 0; n <= strlen(szLoc) ; n++ )
  {
    V = szLoc[n];
    if( V < 'A' )
    {
      P[n] = V - '0';
    }
    else
    {
      P[n] = V - 'A';
    }
  }

  if ( bRet )
  {
    Lon = (P[0]*20) + (P[2]*2) + (P[4]/12) + (P[6]/120) + (P[8]/2880) - 180;
  }
  return Lon;
}


float GcDistance( const float lat1, const float lon1, const float lat2, const float lon2, const char *unit )
{
  // Units:
  // "K" = Kilometers, "N" = Nautical Miles, "M" = Miles

  float theta;
  float dist;
  float miles;

  theta = lon1 - lon2;
  dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) +  cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
  dist = acos(dist);
  dist = rad2deg(dist);
  miles = dist * 60 * 1.1515;

  if( strcmp(unit, "K") == 0 )
  {
    return (miles * 1.609344);
  }
  else if (strcmp(unit, "N") == 0 )
  {
    return (miles * 0.8684);
  }
  else
  {
    return miles;
  }

    return 0;
}

int GcBearing( const float lat1, const float lon1, const float lat2, const float lon2 )
{
  float theta;
  float x, y;
  float brng;

  // printf("lat %f lon %f, lat %f lon %f\n", lat1, lon1, lat2, lon2);

  theta = lon2 - lon1;
  y = sin(deg2rad(theta)) * cos(deg2rad(lat2));
  x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) - sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));

  brng = atan2(y, x);
  brng = rad2deg(brng);

  return ((int) brng + 360) % 360;
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
    else if (strcmp(Value,"Element14_7")==0)
    // Write directly to the touchscreen framebuffer for 7 inch displays
    {
      USBVidDevice[strcspn(USBVidDevice, "\n")] = 0;  //remove the newline
      strcpy(ffmpegCMD, "/home/pi/rpidatv/bin/ffmpeg -hide_banner -loglevel panic -f v4l2 -i ");
      strcat(ffmpegCMD, USBVidDevice);
      strcat(ffmpegCMD, " -vf \"yadif=0:1:0,scale=800:480\" -f rawvideo -pix_fmt rgb32 -vframes 2 /home/pi/tmp/frame.raw");

      // Refresh image until display touched
      while ( FinishedButton == 0 )
      {
        system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
        system(ffmpegCMD);
        system("split -b 1536000 -d -a 1 /home/pi/tmp/frame.raw /home/pi/tmp/frame");
        system("cat /home/pi/tmp/frame1>/dev/fb0");
      }
      // Screen has been touched so stop and tidy up
      system("sudo rm /home/pi/tmp/* >/dev/null 2>/dev/null");
    }
    else  // not a waveshare or 7 inch display so write to the main framebuffer
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
  int rawX, rawY, rawPressure;
  int TCDisplay = -1;

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
  Snap = SnapNumber - 1;

  while (((TCDisplay == 1) || (TCDisplay == -1)) && (SnapNumber != 0))
  {
    sprintf(SnapIndex, "%d", Snap);
    strcpy(fbicmd, "sudo fbi -T 1 -noverbose -a /home/pi/snaps/snap");
    strcat(fbicmd, SnapIndex);
    strcat(fbicmd, ".jpg >/dev/null 2>/dev/null");
    system(fbicmd);

    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    TCDisplay = IsImageToBeChanged(rawX, rawY);
    if (TCDisplay != 0)
    {
      Snap = Snap + TCDisplay;
      if (Snap >= SnapNumber)
      {
        Snap = 0;
      }
      if (Snap < 0)
      {
        Snap = SnapNumber - 1;
      }
    }
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

void do_freqshow()
{
  char Param[31];
  char Value[31];

  strcpy(Param,"display");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  if (strcmp(Value,"Element14_7")==0)  // load modified freqshow.py
  {
    system("cp -f /home/pi/rpidatv/scripts/configs/freqshow/freqshow.py.7inch /home/pi/FreqShow/freqshow.py");
  }
  else   // load orignal freqshow.py
  {
    system("cp -f /home/pi/rpidatv/scripts/configs/freqshow/waveshare_freqshow.py /home/pi/FreqShow/freqshow.py");
  }

  // Exit and load freqshow
  cleanexit(131);
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
    if ((token == 5) || (token == 8) || (token == 9) || (token == 2) || (token == 3)) // Clear, Enter, backspace, L and R arrows
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
      else if (token == 5)   // Clear
      {
        EditText[0]='\0';
        CursorPos = 0;
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
  if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
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
  if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
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
  if (((TabBandLO[CurrentBand] < 0.1) && (TabBandLO[CurrentBand] > -0.1)) || (CallingMenu == 5))
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

    // Check call length which must be > 0
    if (strlen(KeyboardReturn) == 0)
    {
        Spaces = Spaces +1;
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
  bool IsValid = FALSE;
  char Locator10[15];

  //Retrieve (10 char) Current Locator from locator file
  GetConfigParam(PATH_LOCATORS, "mylocator", Locator10);

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter new Locator (6, 8 or 10 char)");
    snprintf(InitText, 11, "%s", Locator10);
    Keyboard(RequestText, InitText, 10);
  
    // Check locator is valid
    IsValid = CheckLocator(KeyboardReturn);
  }
  strcpy(Locator, KeyboardReturn);
  printf("Locator set to: %s\n", Locator);

  // Save Full locator to Locators file
  SetConfigParam(PATH_LOCATORS, "mylocator", KeyboardReturn);

  //Truncate to 6 Characters for Contest display
  KeyboardReturn[6] = '\0';
  SetConfigParam(PATH_PCONFIG, "locator", KeyboardReturn);
  snprintf(Locator, 11, "%s", KeyboardReturn);
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
    if ((strcmp(ScreenState, "RXwithImage") != 0) && (strcmp(ScreenState, "VideoOut") != 0)) // Don't wait for touch if returning from recieve
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
      // Video Output                               VideoOut    (implemented)
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
      if (CallingMenu == 1)
      {
        BackgroundRGB(255,255,255,255);
        SelectPTT(21,0);
      }
      else
      {
        BackgroundRGB(0, 0, 0,255);
      }
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    if (strcmp(ScreenState, "VideoOut") == 0)
    {
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(63, 63, 127, 255);
      strcpy(ScreenState, "NormalMenu");
      UpdateWindow();
      continue;
    }

    if (strcmp(ScreenState, "SnapView") == 0)
    {
      system("sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null");  // Add logo image
      system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
      init(&wscreen, &hscreen);
      Start(wscreen, hscreen);
      BackgroundRGB(0, 0, 0, 255);
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
        CallingMenu = 1;

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
          BackgroundRGB(0,0,0,255);
          if ((strcmp(CurrentTXMode, TabTXMode[0]) == 0) || (strcmp(CurrentTXMode, TabTXMode[1]) == 0)) // DVB-S
          {
            printf("MENU 18 \n");       // FEC
            CurrentMenu=18;
            Start_Highlights_Menu18();
          }
          else  // DVB-S2
          {
            printf("MENU 25 \n");       // DVB-S2 FEC
            CurrentMenu=25;
            Start_Highlights_Menu25();
          }
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
          if (strcmp(CurrentModeOP, "COMPVID") == 0) // Comp Vid OP
          {
            printf("MENU 4 \n");        // Source
            CurrentMenu=4;
            BackgroundRGB(63, 63, 127,255);
            CompVidInitialise();
            Start_Highlights_Menu4();
            UpdateWindow();
            //CompVidStart();
          }
          else     // Transmit, but only if audio has not been used
          {
            if (!((IQAvailable == 0) && ((strcmp(CurrentModeOP, "QPSKRF") == 0) || (strcmp(CurrentModeOP, "IQ") == 0))))
            {
              if ((strcmp(CurrentModeOP, "LIMEMINI") == 0) || (strcmp(CurrentModeOP, "LIMEUSB") == 0))
              {  
                system("/home/pi/rpidatv/scripts/lime_ptt.sh &");
              }
              SelectPTT(i,1);
              UpdateWindow();
              TransmitStart();
            }
            else
            {
              MsgBox4("Transmit unavailable, as the", "audio output has been used.", "Please re-boot to reset.", "Touch screen to continue");
              wait_touch();
              UpdateWindow();
            }
          }
          break;
        case 21:                       // RX
          printf("MENU 5 \n");
          CurrentMenu=5;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu5();
          UpdateWindow();
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
          system("sudo killall express_server >/dev/null 2>/dev/null");
          system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
          sync();  // Prevents shutdown hang in Stretch
          usleep(1000000);
          finish();
          system("sudo shutdown now");
          break;
        case 1:                               // Reboot
          MsgBox4("", "Rebooting now", "", "");
          system("sudo killall express_server >/dev/null 2>/dev/null");
          system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
          sync();  // Prevents shutdown hang in Stretch
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
        case 5:                               // Locator Bearings
          RangeBearing();
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 6:                               // Sites and Beacons Bearing
          BeaconBearing();
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
        case 14:                              // Stream Viewer
          printf("MENU 20 \n");
          CurrentMenu=20;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu20();
          UpdateWindow();
          break;
          UpdateWindow();
          break;
        case 15:                               // Select FreqShow
          if(CheckRTL()==0)
          {
            do_freqshow();
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
          if(CheckRTL()==0)
          {
            rtl_tcp();
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
          }
          BackgroundRGB(0,0,0,255);
          UpdateWindow();
          break;
        case 18:                              // RTL-FM
          if(CheckRTL()==0)
          {
            RTLdetected = 1;
          }
          else
          {
            RTLdetected = 0;
            MsgBox2("No RTL-SDR Connected", "Connect RTL-SDR to enable RX");
            wait_touch();
          }
          printf("MENU 6 \n");
          CurrentMenu=6;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu6();
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
          SetButtonStatus(ButtonNumber(3, 0), 1);  // and highlight button
          UpdateWindow();
          SetButtonStatus(ButtonNumber(3, 0), 0);
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
        case 10:                               // Amend Sites/Beacons
          printf("MENU 31 \n"); 
          CurrentMenu=31;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu31();
          UpdateWindow();
          break;
        case 11:                               // Set Receive LO
          CallingMenu = 302;
          printf("MENU 26 \n"); 
          CurrentMenu=26;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu26();
          UpdateWindow();
          break;
        case 12:                               // Blank
          break;
        case 13:                               // Blank
          break;
        case 14:                               // Blank
          break;
        case 15:                              // Set Band Details
          CallingMenu = 301;
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

      if (CurrentMenu == 4)  // Menu 4 Composite Video Output
      {
        printf("Button Event %d, Entering Menu 4 Case Statement\n",i);
        switch (i)
        {
        case 5:                               // PiCam
        case 6:                               // CompVid
        case 7:                               // TCAnim
        case 8:                               // TestCard
        case 9:                               // Snap
        case 0:                               // Contest
        case 1:                               // Webcam
        case 2:                               // Movie
          // temporary code to put buttons in a line:
          if (i == 6)
          {
            i = 0;
          }
          SelectVidSource(i);
          printf("%s\n", CurrentVidSource);
          break;
        case 13:                             // Select Vid Band
          printf("MENU 30 \n");
          CurrentMenu=30;
          BackgroundRGB(0, 0, 0,255);
          Start_Highlights_Menu30();
          UpdateWindow();
          break;
        case 20:                       // PTT ON/OFF
          if (VidPTT == 1)
          {
            VidPTT = 0;
            digitalWrite(GPIO_PTT, LOW);
          }
          else
          {
            VidPTT = 1;
            digitalWrite(GPIO_PTT, HIGH);
          }
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 22:                       // Exit
          CompVidStop();
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        default:
          printf("Menu 4 Error\n");
        }
        continue;   // Completed Menu 4 action, go and wait for touch
      }

      if (CurrentMenu == 5)  // Menu 5 LeanDVB
      {
        printf("Button Event %d, Entering Menu 5 Case Statement\n",i);
        CallingMenu = 5;

        // Clear RX Preset store trigger if not a preset
        if ((i > 3) && (RXStoreTrigger == 1))
        {
          RXStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          UpdateWindow();
          continue;
        }
        switch (i)
        {
        case 0:                              // Preset 1
        case 1:                              // Preset 2
        case 2:                              // Preset 3
        case 3:                              // Preset 4
          if (RXStoreTrigger == 0)
          {
            RecallRXPreset(i);  // Recall preset
          }
          else
          {
            SaveRXPreset(i);  // Set preset
            RXStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
            BackgroundRGB(0,0,0,255);
          }
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          break;
        case 4:                              // Store RX Preset
          if (RXStoreTrigger == 0)
          {
            RXStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),1);
          }
          else
          {
            RXStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),0);
          }
          UpdateWindow();
          break;
        case 5:                                            // Fastlock on/off
          if (strcmp(RXfastlock[0], "ON") == 0)
          {
            strcpy(RXfastlock[0], "OFF");
          }
          else
          {
            strcpy(RXfastlock[0], "ON");
          }
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          SetConfigParam(PATH_RXPRESETS, "rx0fastlock", RXfastlock[0]);
          break;
        case 7:                                            // Audio on/off
          if (strcmp(RXsound[0], "ON") == 0)
          {
            strcpy(RXsound[0], "OFF");
          }
          else
          {
            strcpy(RXsound[0], "ON");
          }
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          SetConfigParam(PATH_RXPRESETS, "rx0sound", RXsound[0]);
          break;
        case 9:                                            // SetRXLikeTX
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          SetRXLikeTX();
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
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
          printf("MENU 17 \n");        // SR
          CurrentMenu=17;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu17();
          UpdateWindow();
          break;
        case 12:
          printf("MENU 18 \n");        // FEC
          CurrentMenu=18;
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu18();
          UpdateWindow();
          break;
        case 13:                       // Sample Rate
          SetSampleRate();
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 14:                       // Gain
          SetRXGain();
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 16:                       // Encoding
          ToggleEncoding();
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu5();
          UpdateWindow();
          break;
        case 18:                                            // Constellation on/off
          if (strcmp(RXgraphics[0], "ON") == 0)
          {
            strcpy(RXgraphics[0], "OFF");
          }
          else
          {
            strcpy(RXgraphics[0], "ON");
          }
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          SetConfigParam(PATH_RXPRESETS, "rx0graphics", RXgraphics[0]);
          break;
        case 19:                                            // Parameters on/off
          if (strcmp(RXparams[0], "ON") == 0)
          {
            strcpy(RXparams[0], "OFF");
          }
          else
          {
            strcpy(RXparams[0], "ON");
          }
          Start_Highlights_Menu5();    // Refresh button labels
          UpdateWindow();
          SetConfigParam(PATH_RXPRESETS, "rx0parameters", RXparams[0]);
          break;
        case 21:                       // RX
          if(CheckRTL()==0)
          {
            BackgroundRGB(0,0,0,255);
            Start(wscreen,hscreen);
            ReceiveStart2();
            break;
          }
          else
          {
            MsgBox("No RTL-SDR Connected");
            wait_touch();
            BackgroundRGB(0, 0, 0, 255);
            UpdateWindow();
          }
          break;
        case 22:                                          // Back to Menu 1
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        default:
          printf("Menu 5 Error\n");
        }
        continue;   // Completed Menu 5 action, go and wait for touch
      }

      if (CurrentMenu == 6)  // Menu 6 RTL-FM
      {
        printf("Button Event %d, Entering Menu 6 Case Statement\n",i);

        // Clear RTL Preset store trigger if not a preset
        if ((i > 9) && (RTLStoreTrigger == 1))
        {
          RTLStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
          UpdateWindow();
          continue;
        }
        switch (i)
        {
        case 0:                              // Preset 6
        case 1:                              // Preset 7
        case 2:                              // Preset 8
        case 3:                              // Preset 9
          if (RTLStoreTrigger == 0)
          {
            RecallRTLPreset(i);  // Recall preset
            // and start/restart RX
          }
          else
          {
            SaveRTLPreset(i);  // Set preset
            RTLStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
            BackgroundRGB(0,0,0,255);
          }
          RTLstop();
          RTLstart();
          RTLactive = 1;
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          Start_Highlights_Menu6();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
          UpdateWindow();
          break;
        case 4:                              // Store RTL Preset
          if (RTLStoreTrigger == 0)
          {
            RTLStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),1);
          }
          else
          {
            RTLStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4),0);
          }
          UpdateWindow();
          break;
        case 5:                              // Preset 1
        case 6:                              // Preset 2
        case 7:                              // Preset 3
        case 8:                              // Preset 4
        case 9:                              // Preset 5
          if (RTLStoreTrigger == 0)
          {
            RecallRTLPreset(i);  // Recall preset
            // and start/restart RX
          }
          else
          {
            SaveRTLPreset(i);  // Set preset
            RTLStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 4), 0);
            BackgroundRGB(0,0,0,255);
          }
          RTLstop();
          RTLstart();
          RTLactive = 1;
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          Start_Highlights_Menu6();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 10:                             // AM
        case 11:                             // FM
        case 12:                             // WBFM
        case 13:                             // USB
        case 14:                             // LSB
          SelectRTLmode(i);
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 15:                             // Frequency
          ChangeRTLFreq();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 16:                             // Set Squelch Level
          ChangeRTLSquelch();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 17:                             // Squelch on/off
          if (RTLsquelchoveride == 1)
          {
            RTLsquelchoveride = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 17),1);
          }
          else
          {
            RTLsquelchoveride = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 17),0);
          }
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          UpdateWindow();
          break;
        case 18:                             // Save
          SaveRTLPreset(-6);                 // Saves current state
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 1);
          Start_Highlights_Menu6();    // Refresh button labels
          UpdateWindow();
          usleep(500000);
          SetButtonStatus(ButtonNumber(CurrentMenu, i), 0); 
          UpdateWindow();
          break;
        case 19:                             // Set ppm offset
          ChangeRTLppm();
          if (RTLactive == 1)
          {
            RTLstop();
            RTLstart();
          }
          BackgroundRGB(0,0,0,255);
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
        case 20:                            // Not shown
          break;
        case 21:                            // RX on
          if (RTLactive == 1)
          {
            RTLstop();
            RTLactive = 0;
          }
          else
          {
            RTLactive = 1;
            RTLstart();
          }
          Start_Highlights_Menu6();          // Refresh button labels
          UpdateWindow();
          break;
         case 22:                            // Exit
          RTLstop();
          RTLactive = 0;
          printf("MENU 1 \n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 23:                            // Blank
          break;
        default:
          printf("Menu 6 Error\n");
        }
        continue;   // Completed Menu 6 action, go and wait for touch
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
        case 0:                               // QPSK
          SelectTX(i);
          printf("S2 QPSK\n");
          break;
        case 1:                               // S2 8PSK
          SelectTX(i);
          printf("S2 8PSK\n");
          break;
        case 2:                               // S2 16APSK
          SelectTX(i);
          printf("S2 16APSK\n");
          break;
        case 3:                               // S2 32APSK
          SelectTX(i);
          printf("S2 32APSK\n");
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
        case 8:                               // Lime USB
          SelectOP(i);
          printf("LIME USB\n");
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
        case 3:                               // LIME Mini
          SelectOP(i);
          printf("LIME Mini\n");
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
        case 5:                               // Freq 1
        case 6:                               // Freq 2
        case 7:                               // Freq 3
        case 8:                               // Freq 4
        case 9:                               // Freq 5
          SelectFreq(i);
          printf("Frequency Button %d\n", i);
          break;
        case 3:                               // Freq 9 Direct Entry
          ChangePresetFreq(i);
          SelectFreq(i);
          printf("Frequency Button %d\n", i);
          break;
        default:
          printf("Menu 16 Error\n");
        }
        if(i != 3)  // Don't pause if frequency has been set on keyboard
        {
          UpdateWindow();
          usleep(500000);
        }
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        if (CallingMenu == 1)
        {
          printf("Returning to MENU 1 from Menu 16\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
        }
        else
        {
          printf("Returning to MENU 5 from Menu 16\n");
          CurrentMenu=5;
          BackgroundRGB(0, 0, 0, 255);
          Start_Highlights_Menu5();
        }
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
        if (CallingMenu == 1)
        {
          printf("Returning to MENU 1 from Menu 17\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
        }
        else
        {
          printf("Returning to MENU 5 from Menu 17\n");
          CurrentMenu=5;
          BackgroundRGB(0, 0, 0, 255);
          Start_Highlights_Menu5();
        }
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
        if (CallingMenu == 1)
        {
          printf("Returning to MENU 1 from Menu 18\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
        }
        else
        {
          printf("Returning to MENU 5 from Menu 18\n");
          CurrentMenu=5;
          BackgroundRGB(0, 0, 0, 255);
          Start_Highlights_Menu5();
        }
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
      if (CurrentMenu == 20)  // Menu 20 Stream Display
      {
        printf("Button Event %d, Entering Menu 20 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Stream Select Cancel\n");
          StreamStoreTrigger = 0;
          SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 20\n");
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 5:                               // Preset 1
        case 6:                               // Preset 2
        case 7:                               // Preset 3
        case 8:                               // Preset 4
        case 0:                               // Preset 5
        case 1:                               // Preset 6
        case 2:                               // Preset 7
        case 3:                               // Preset 8
          if (StreamStoreTrigger == 0)        // Normal
          {
            DisplayStream(i);
            BackgroundRGB(0, 0, 0, 255);
          }
          else                                // Amend the Preset
          {
            AmendStreamPreset(i);
            StreamStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
            Start_Highlights_Menu20();        // Refresh the button labels
          }
          UpdateWindow();                     // Stay in Menu 20
          break;
        case 9:                               // Amend Preset
          if (StreamStoreTrigger == 0)
          {
            StreamStoreTrigger = 1;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 1);
          }
          else
          {
            StreamStoreTrigger = 0;
            SetButtonStatus(ButtonNumber(CurrentMenu, 9), 0);
          }
          UpdateWindow();
          break;
          printf("Amend Preset\n");
          break;
        default:
          printf("Menu 20 Error\n");
        }
        continue;   // Completed Menu 20 action, go and wait for touch
      }
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

      if (CurrentMenu == 25)  // Menu 25 DVB-S2 FEC
      {
        printf("Button Event %d, Entering Menu 25 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("FEC Cancel\n");
          break;
        case 5:                               // FEC 1/4
          SelectS2Fec(i);
          printf("FEC 1/4\n");
          break;
        case 6:                               // FEC 1/3
          SelectS2Fec(i);
          printf("FEC 1/3\n");
          break;
        case 7:                               // FEC 1/2
          SelectS2Fec(i);
          printf("FEC 1/2\n");
          break;
        case 8:                               // FEC 3/5
          SelectS2Fec(i);
          printf("FEC 3/5\n");
          break;
        case 9:                               // FEC 2/3
          SelectS2Fec(i);
          printf("FEC 2/3\n");
          break;
        case 0:                               // FEC 3/4
          SelectS2Fec(i);
          printf("FEC 3/4\n");
          break;
        case 1:                               // FEC 5/6
          SelectS2Fec(i);
          printf("FEC 5/6\n");
          break;
        case 2:                               // FEC 8/9
          SelectS2Fec(i);
          printf("FEC 8/9\n");
          break;
        case 3:                               // FEC 9/10
          SelectS2Fec(i);
          printf("FEC 9/10\n");
          break;
        default:
          printf("Menu 25 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 25\n");
        CurrentMenu=1;
        BackgroundRGB(255,255,255,255);
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 25 action, go and wait for touch
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
          if(CallingMenu == 301) // Set Band Details
          {
            printf("Changing Band Details %d\n", i);
            ChangeBandDetails(i);
            CurrentMenu=26;
            BackgroundRGB(0,0,0,255);
            Start_Highlights_Menu26();
          }
          else  // 302, Set Receive LO
          {
            SetReceiveLOFreq(i);      // Set the LO frequency
            ReceiveLOStart();         // Start the LO if it is required
            printf("Returning to MENU 1 from Menu 26\n");
            CurrentMenu=1;
            BackgroundRGB(255,255,255,255);
            Start_Highlights_Menu1();
          }
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

      if (CurrentMenu == 30)  // Menu 30 Comp Vid Band
      {
        printf("Button Event %d, Entering Menu 30 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Band Details Cancel\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 4 from Menu 30\n");
          CurrentMenu=4;
          BackgroundRGB(63, 63, 127, 255);
          Start_Highlights_Menu4();
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
          printf("Changing Vid Band to Button %d\n", i);
          ChangeVidBand(i);
          CurrentMenu=4;
          BackgroundRGB(63, 63, 127, 255);
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        default:
          printf("Menu 30 Error\n");
        }
        // stay in Menu 30 to set another band
        continue;
      }

      if (CurrentMenu == 31)  // Menu 31 Change Beacon/site details
      {
        printf("Button Event %d, Entering Menu 31 Case Statement\n",i);
        AmendBeacon(i);
        // No exit button, so go straight to menu 3
        printf("Completed band/site change, going to Menu 3 %d\n", i);
        CurrentMenu=3;
        BackgroundRGB(0, 0, 0, 255);
        Start_Highlights_Menu3();
        UpdateWindow();
        continue;   // Completed Menu 31 action, go and wait for touch
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
        case 3:                                 // Add MPEG-2 Licence
          MPEG2License();
          CurrentMenu=1;
          BackgroundRGB(255,255,255,255);
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:                                 // Install/update Lime drivers
        case 1:                                 // Update Lime Firmware
        case 5:                                 // All defined in ExecuteUpdate(i)
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

/*  // Button 22 Temp RX
  button = CreateButton(1, 22);
  AddButtonStatus(button," RX 2 ",&Blue);
*/
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
    case 14:strcpy(FECtext, "  FEC  ^  1/4 ") ;break;
    case 13:strcpy(FECtext, "  FEC  ^  1/3 ") ;break;
    case 12:strcpy(FECtext, "  FEC  ^  1/2 ") ;break;
    case 35:strcpy(FECtext, "  FEC  ^  3/5 ") ;break;
    case 23:strcpy(FECtext, "  FEC  ^  2/3 ") ;break;
    case 34:strcpy(FECtext, "  FEC  ^  3/4 ") ;break;
    case 56:strcpy(FECtext, "  FEC  ^  5/6 ") ;break;
    case 89:strcpy(FECtext, "  FEC  ^  8/9 ") ;break;
    case 91:strcpy(FECtext, "  FEC  ^  9/10 ") ;break;
    default:strcpy(FECtext, "  FEC  ^Error") ;break;
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
  if (strcmp(CurrentModeOP, TabModeOP[2]) == 0)  // DATV Express
  {
    snprintf(Leveltext, 20, "Exp Level^%d", TabBandExpLevel[CurrentBand]);
  }
  else if ((strcmp(CurrentModeOP, TabModeOP[3]) == 0) || (strcmp(CurrentModeOP, TabModeOP[8]) == 0))  // Lime
  {
    snprintf(Leveltext, 20, "Lime Gain^%d", TabBandLimeGain[CurrentBand]);
  }
  else
  {
    snprintf(Leveltext, 20, "Att Level^%.2f", TabBandAttenLevel[CurrentBand]);
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
  if (strcmp(CurrentModeOPtext, "BATC^STREAM") == 0)
  {
    strcpy(Outputtext, "Output to^BATC");
  }
  else
  {
    strcat(Outputtext, CurrentModeOPtext);
  }
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
  AddButtonStatus(button, "Locator^Bearings", &Blue);

  button = CreateButton(2, 6);
  AddButtonStatus(button, "Sites/Bcns^Bearings", &Blue);

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

  button = CreateButton(2, 14);
  AddButtonStatus(button, "Stream^Viewer", &Blue);
  AddButtonStatus(button, "Stream^Viewer", &Green);

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

  button = CreateButton(2, 18);
  AddButtonStatus(button, "RTL-FM^Receiver", &Blue);
  AddButtonStatus(button, "RTL-FM^Receiver", &Green);

  //button = CreateButton(2, 19);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

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
  AddButtonStatus(button, "Checking^for Update", &Green);

  // 3rd line up Menu 3: Amend Sites/Beacons, 

  button = CreateButton(3, 10);
  AddButtonStatus(button, "Amend^Sites/Bcns", &Blue);

  button = CreateButton(3, 11);
  AddButtonStatus(button, "Set Receive^LOs", &Blue);
  AddButtonStatus(button, "Set Receive^LOs", &Green);

  // 4th line up Menu 3: Band Details, Preset Freqs, Preset SRs, Call and ADFRef

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
{
  ;
}

void Define_Menu4()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t Grey;
  color_t LBlue;
  color_t DBlue;
  color_t Red;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Grey.r=127; Grey.g=127; Grey.b=127;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;
  Red.r=255; Red.g=0; Red.b=0;

  strcpy(MenuTitle[4], "Composite Video Output Menu (4)"); 

  // Bottom Row, Menu 4
  //button = CreateButton(4, 0);
  //AddButtonStatus(button, TabVidSource[5], &Blue);
  //AddButtonStatus(button, TabVidSource[5], &Green);
  //AddButtonStatus(button, TabVidSource[5], &Grey);

  //button = CreateButton(4, 1);
  //AddButtonStatus(button, TabVidSource[6], &Blue);
  //AddButtonStatus(button, TabVidSource[6], &Green);
  //AddButtonStatus(button, TabVidSource[6], &Grey);

  //button = CreateButton(4, 2);
  //AddButtonStatus(button, TabVidSource[7], &Blue);
  //AddButtonStatus(button, TabVidSource[7], &Green);
  //AddButtonStatus(button, TabVidSource[7], &Grey);

  // 2nd Row, Menu 4

  button = CreateButton(4, 5);
  AddButtonStatus(button, TabVidSource[0], &Blue);
  AddButtonStatus(button, TabVidSource[0], &Green);
  AddButtonStatus(button, TabVidSource[0], &Grey);

  //button = CreateButton(4, 6);
  //AddButtonStatus(button, TabVidSource[1], &Blue);
  //AddButtonStatus(button, TabVidSource[1], &Green);
  //AddButtonStatus(button, TabVidSource[1], &Grey);

  // Temporary entry to put buttons in a line
  button = CreateButton(4, 6);
  AddButtonStatus(button, TabVidSource[5], &Blue);
  AddButtonStatus(button, TabVidSource[5], &Green);
  AddButtonStatus(button, TabVidSource[5], &Grey);

  button = CreateButton(4, 7);
  AddButtonStatus(button, TabVidSource[2], &Blue);
  AddButtonStatus(button, TabVidSource[2], &Green);
  AddButtonStatus(button, TabVidSource[2], &Grey);

  button = CreateButton(4, 8);
  AddButtonStatus(button, TabVidSource[3], &Blue);
  AddButtonStatus(button, TabVidSource[3], &Green);
  AddButtonStatus(button, TabVidSource[3], &Grey);

  button = CreateButton(4, 9);
  AddButtonStatus(button, TabVidSource[4], &Blue);
  AddButtonStatus(button, TabVidSource[4], &Green);
  AddButtonStatus(button, TabVidSource[4], &Grey);

  // 3rd Row, Menu 4

  button = CreateButton(4, 13);
  AddButtonStatus(button, "Band^", &Blue);
  AddButtonStatus(button, "Band^", &Green);

  // Top of Menu 4

  button = CreateButton(4, 20);
  AddButtonStatus(button,"PTT", &Blue);
  AddButtonStatus(button,"PTT ON", &Red);

  button = CreateButton(4, 22);
  AddButtonStatus(button," Exit ", &DBlue);
  AddButtonStatus(button," Exit ", &LBlue);
}

void Start_Highlights_Menu4()
{
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  char CompVidBandText[256];

  // Display the Comp Vid Band
  strcpy(CompVidBandText, "Band^");
  strcat(CompVidBandText, TabBandLabel[CompVidBand]);
  AmendButtonStatus(ButtonNumber(4, 13), 0, CompVidBandText, &Blue);
  AmendButtonStatus(ButtonNumber(4, 13), 1, CompVidBandText, &Green);

  SelectInGroupOnMenu(4, 20, 20, 20, VidPTT);
}

void Define_Menu5()
{
  int button = 0;
  color_t Green;
  color_t Blue;
  color_t Red;
  color_t Grey;
  strcpy(MenuTitle[5], "LeanDVB DATV Receiver Menu (5)"); 

  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=255; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;

  // Presets - Bottom Row, Menu 5

  button = CreateButton(5, 0);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 1);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 2);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 3);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 4);
  AddButtonStatus(button, "Store^Preset", &Blue);
  AddButtonStatus(button, "Store^Preset", &Red);

  // 2nd Row, Menu 5.  

  button = CreateButton(5, 5);
  AddButtonStatus(button, "FastLock^ON", &Blue);
  AddButtonStatus(button, "FastLock^ON", &Green);

  //button = CreateButton(5, 6);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 7);
  AddButtonStatus(button, "Audio^OFF", &Blue);
  AddButtonStatus(button, "Audio^OFF", &Green);

  //button = CreateButton(5, 8);
  //AddButtonStatus(button, " ", &Blue);
  //AddButtonStatus(button, " ", &Green);

  button = CreateButton(5, 9);
  AddButtonStatus(button, "Set as^TX", &Blue);
  AddButtonStatus(button, "Set as^TX", &Green);

  // Freq, SR, FEC, Samp Rate, Gain - 3rd line up Menu 5

  button = CreateButton(5, 10);                        // AM
  AddButtonStatus(button, "Freq^not set", &Blue);
  AddButtonStatus(button, "Freq^not set", &Green);

  button = CreateButton(5, 11);                        // FM
  AddButtonStatus(button, "Sym Rate^not set", &Blue);
  AddButtonStatus(button, "Sym Rate^not set", &Green);

  button = CreateButton(5, 12);                        // WBFM
  AddButtonStatus(button, "FEC^not set", &Blue);
  AddButtonStatus(button, "FEC^not set", &Green);

  button = CreateButton(5, 13);                        // USB
  AddButtonStatus(button,"Samp Rate^not set",&Blue);
  AddButtonStatus(button,"Samp Rate^not set",&Green);

  button = CreateButton(5, 14);                        // LSB
  AddButtonStatus(button,"Gain^not set",&Blue);
  AddButtonStatus(button,"Gain^not set",&Green);

  // 4th line up Menu 5

  button = CreateButton(5, 15);
  AddButtonStatus(button, "Modulation^not set", &Blue);
  AddButtonStatus(button, "Modulation^not set", &Green);

  button = CreateButton(5, 16);
  AddButtonStatus(button, "Encoding^   ", &Blue);
  AddButtonStatus(button, "Encoding^   ", &Green);
  AddButtonStatus(button, "Encoding^   ", &Grey);

  button = CreateButton(5, 17);
  AddButtonStatus(button, "SDR^RTL-SDR", &Blue);
  AddButtonStatus(button, "SDR^RTL-SDR", &Blue);

  button = CreateButton(5, 18);
  AddButtonStatus(button, "Constel'n^ON", &Blue);
  AddButtonStatus(button, "Constel'n^ON", &Green);

  button = CreateButton(5, 19);
  AddButtonStatus(button, "Params^ON", &Blue);
  AddButtonStatus(button, "Params^ON", &Green);

  //RECEIVE and Exit - Top of Menu 5

  button = CreateButton(5, 21);
  AddButtonStatus(button," RX  ",&Blue);
  AddButtonStatus(button,"RX ON",&Red);

  button = CreateButton(5, 22);
  AddButtonStatus(button,"EXIT",&Blue);
  AddButtonStatus(button,"EXIT",&Green);
}

void Start_Highlights_Menu5()
{
  color_t Green;
  color_t Blue;
  color_t Grey;

  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Grey.r=127; Grey.g=127; Grey.b=127;
  int index;
  char RXBtext[31];
  int NoButton;

  // Buttons 0 to 3: Presets.  Set text here
  for(index = 1; index < 5 ; index = index + 1)
  {
    // Define the button text
    snprintf(RXBtext, 20, "%s^%s", RXlabel[index], RXfreq[index]);
    NoButton = index - 1;
    AmendButtonStatus(ButtonNumber(5, NoButton), 0, RXBtext, &Blue);
    AmendButtonStatus(ButtonNumber(5, NoButton), 1, RXBtext, &Green);
  }

  // Now highlight current preset by comparing frequency, SR and encoding

  // First set all presets off
  SelectInGroupOnMenu(5, 0, 3, 0, 0);

  for(index = 1; index < 5 ; index = index + 1)
  {
    NoButton = index - 1;
    if ((strcmp(RXfreq[0], RXfreq[index]) == 0) 
      && (RXsr[0] == RXsr[index]) 
      && (strcmp(RXencoding[0], RXencoding[index]) == 0))
    {
      SelectInGroupOnMenu(5, 0, 3, NoButton, 1);
    }
  }

  // Fastlock Button 5
  strcpy(RXBtext, "FastLock^");
  strcat(RXBtext, RXfastlock[0]);
  AmendButtonStatus(ButtonNumber(5, 5), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 5), 1, RXBtext, &Green);

  // Audio Button 7
  strcpy(RXBtext, "Audio^");
  strcat(RXBtext, RXsound[0]);
  AmendButtonStatus(ButtonNumber(5, 7), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 7), 1, RXBtext, &Green);

  // Frequency button 10
  strcpy(RXBtext, "Freq^");
  strcat(RXBtext, RXfreq[0]);
  AmendButtonStatus(ButtonNumber(5, 10), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 10), 1, RXBtext, &Green);

  // SR button 11
  strcpy(RXBtext, "Sym Rate");
  strcat(RXBtext, RXfreq[0]);
  snprintf(RXBtext, 20, "Sym Rate^%d", RXsr[0]);
  AmendButtonStatus(ButtonNumber(5, 11), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 11), 1, RXBtext, &Green);

  // FEC Button 12
  index = atoi(RXfec[0]);
  switch(index)
  {
    case 1:strcpy(RXBtext, "  FEC  ^  1/2 ") ;break;
    case 2:strcpy(RXBtext, "  FEC  ^  2/3 ") ;break;
    case 3:strcpy(RXBtext, "  FEC  ^  3/4 ") ;break;
    case 5:strcpy(RXBtext, "  FEC  ^  5/6 ") ;break;
    case 7:strcpy(RXBtext, "  FEC  ^  7/8 ") ;break;
    case 14:strcpy(RXBtext, "  FEC  ^  1/4 ") ;break;
    case 13:strcpy(RXBtext, "  FEC  ^  1/3 ") ;break;
    case 12:strcpy(RXBtext, "  FEC  ^  1/2 ") ;break;
    case 35:strcpy(RXBtext, "  FEC  ^  3/5 ") ;break;
    case 23:strcpy(RXBtext, "  FEC  ^  2/3 ") ;break;
    case 34:strcpy(RXBtext, "  FEC  ^  3/4 ") ;break;
    case 56:strcpy(RXBtext, "  FEC  ^  5/6 ") ;break;
    case 89:strcpy(RXBtext, "  FEC  ^  8/9 ") ;break;
    case 91:strcpy(RXBtext, "  FEC  ^  9/10 ") ;break;
    default:strcpy(RXBtext, "  FEC  ^Error") ;break;
  }
  AmendButtonStatus(ButtonNumber(5, 12), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 12), 1, RXBtext, &Green);
  AmendButtonStatus(ButtonNumber(5, 12), 2, RXBtext, &Grey);

  // Sample Rate button 13
  if(RXsamplerate[0] == 0)
  {
    strcpy(RXBtext, "Samp Rate^Auto");
  }
  else
  {
    snprintf(RXBtext, 20, "Samp Rate^%d", RXsamplerate[0]);
  }
  AmendButtonStatus(ButtonNumber(5, 13), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 13), 1, RXBtext, &Green);

  // Gain button 14
  if(RXgain[0] == 0)
  {
    strcpy(RXBtext, "Gain^Auto");
  }
  else
  {
    snprintf(RXBtext, 20, "Gain^%d", RXgain[0]);
  }
  AmendButtonStatus(ButtonNumber(5, 14), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 14), 1, RXBtext, &Green);

  // Modulation button 15
  strcpy(RXBtext, "Modulation^");
  strcat(RXBtext, RXmodulation[0]);
  AmendButtonStatus(ButtonNumber(5, 15), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 15), 1, RXBtext, &Green);

  // Encoding button 16
  strcpy(RXBtext, "Encoding^");
  strcat(RXBtext, RXencoding[0]);
  AmendButtonStatus(ButtonNumber(5, 16), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 16), 1, RXBtext, &Green);

  // SDR Type button 17
  strcpy(RXBtext, "SDR^");
  strcat(RXBtext, RXsdr[0]);
  AmendButtonStatus(ButtonNumber(5, 17), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 17), 1, RXBtext, &Green);

  // Constellation on/off button 18
  strcpy(RXBtext, "Constel'n^");
  strcat(RXBtext, RXgraphics[0]);
  AmendButtonStatus(ButtonNumber(5, 18), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 18), 1, RXBtext, &Green);

  // Parameters on/off button 19
  strcpy(RXBtext, "Params^");
  strcat(RXBtext, RXparams[0]);
  AmendButtonStatus(ButtonNumber(5, 19), 0, RXBtext, &Blue);
  AmendButtonStatus(ButtonNumber(5, 19), 1, RXBtext, &Green);

  // Make the RX button red if RX on
  SetButtonStatus(ButtonNumber(5, 21), RTLactive); 

}

void Define_Menu6()
{
  int button = 0;
  color_t Green;
  color_t Blue;
  color_t Red;
  color_t Grey;
  strcpy(MenuTitle[6], "RTL-FM Audio Receiver Menu (6)"); 

  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=255; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;

  // Presets - Bottom Row, Menu 6

  button = CreateButton(6, 0);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 1);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 2);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 3);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 4);
  AddButtonStatus(button, "Store^Preset", &Blue);
  AddButtonStatus(button, "Store^Preset", &Red);

  // 2nd Row, Menu 6.  Presets

  button = CreateButton(6, 5);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 6);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 7);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 8);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  button = CreateButton(6, 9);
  AddButtonStatus(button, " ", &Blue);
  AddButtonStatus(button, " ", &Green);

  // AM, FM, WBFM, USB, LSB - 3rd line up Menu 6

  button = CreateButton(6, 10);                        // AM
  AddButtonStatus(button, "  AM  ", &Blue);
  AddButtonStatus(button, "  AM  ", &Green);

  button = CreateButton(6, 11);                        // FM
  AddButtonStatus(button, " NBFM ", &Blue);
  AddButtonStatus(button, " NBFM ", &Green);

  button = CreateButton(6, 12);                        // WBFM
  AddButtonStatus(button, " WBFM ", &Blue);
  AddButtonStatus(button, " WBFM ", &Green);

  button = CreateButton(6, 13);                        // USB
  AddButtonStatus(button," USB  ",&Blue);
  AddButtonStatus(button," USB  ",&Green);

  button = CreateButton(6, 14);                        // LSB
  AddButtonStatus(button," LSB  ",&Blue);
  AddButtonStatus(button," LSB  ",&Green);

  // Freq, Squelch setting, Squelch on/off, blank, freq ppm. 4th line up Menu 6

  button = CreateButton(6, 15);
  AddButtonStatus(button, " Freq ^not set", &Blue);
  AddButtonStatus(button, " Freq ^not set", &Green);

  button = CreateButton(6, 16);
  AddButtonStatus(button, "Squelch^Level   ", &Blue);
  AddButtonStatus(button, "Squelch^Level   ", &Green);
  AddButtonStatus(button, "Squelch^Level   ", &Grey);

  button = CreateButton(6, 17);
  AddButtonStatus(button, "Squelch^  ON  ", &Blue);
  AddButtonStatus(button, "Squelch^  OFF  ", &Blue);

  button = CreateButton(6, 18);
  AddButtonStatus(button, "Save^Settings", &Blue);
  AddButtonStatus(button, "Save^Settings", &Green);

  button = CreateButton(6, 19);
  AddButtonStatus(button, "Freq Corrn^ppm", &Blue);
  AddButtonStatus(button, "Freq Corrn^ppm", &Green);

  //RECEIVE and Exit - Top of Menu 6

  button = CreateButton(6, 21);
  AddButtonStatus(button," RX  ",&Blue);
  AddButtonStatus(button,"RX ON",&Red);

  button = CreateButton(6, 22);
  AddButtonStatus(button,"EXIT",&Blue);
  AddButtonStatus(button,"EXIT",&Green);
}

void Start_Highlights_Menu6()
{
  color_t Green;
  color_t Blue;
  //color_t Red;
  //color_t Grey;

  Green.r=0; Green.g=96; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
//  Red.r=255; Red.g=0; Red.b=0;
//  Grey.r=127; Grey.g=127; Grey.b=127;
  int index;
  char RTLBtext[21];
  int NoButton;

  // Display the frequency
  strcpy(RTLBtext, " Freq ^");
  strcat(RTLBtext, RTLfreq[0]);
  AmendButtonStatus(ButtonNumber(6, 15), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 15), 1, RTLBtext, &Green);

  // Display the Squelch level
  snprintf(RTLBtext, 20, "Squelch^Level %d", RTLsquelch[0]);
  AmendButtonStatus(ButtonNumber(6, 16), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 16), 1, RTLBtext, &Green);

  // Display Squelch off/on
  if (RTLsquelchoveride == 1) // squelch on
  {
      SelectInGroupOnMenu(6, 17, 17, 17, 0);
  }
  else // squelch off
  {
      SelectInGroupOnMenu(6, 17, 17, 17, 1);
  }

  // Display Freq adjustment
  snprintf(RTLBtext, 20, "Freq Corrn^%d ppm", RTLppm);
  AmendButtonStatus(ButtonNumber(6, 19), 0, RTLBtext, &Blue);
  AmendButtonStatus(ButtonNumber(6, 19), 1, RTLBtext, &Green);

  // Highlight the current mode
  if (strcmp(RTLmode[0], "am") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 10, 1);
  }
  else if (strcmp(RTLmode[0], "fm") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 11, 1);
  }
  else if (strcmp(RTLmode[0], "wbfm") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 12, 1);
  }
  else if (strcmp(RTLmode[0], "usb") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 13, 1);
  }
  else if (strcmp(RTLmode[0], "lsb") == 0)
  {
    SelectInGroupOnMenu(6, 10, 14, 14, 1);
  }
 
  // Highlight current preset by comparing frequency
  for(index = 1; index < 10 ; index = index + 1)
  {
    // Define the button text
    snprintf(RTLBtext, 20, "%s^%s", RTLlabel[index], RTLfreq[index]);

    NoButton = index + 4;   // Valid for second row
    if (index > 5)          // Overwrite for bottom row
    {
      NoButton = index - 6;
    }
    AmendButtonStatus(ButtonNumber(6, NoButton), 0, RTLBtext, &Blue);
    AmendButtonStatus(ButtonNumber(6, NoButton), 1, RTLBtext, &Green);
    if (atof(RTLfreq[index]) == atof(RTLfreq[0]))
    {
      SelectInGroupOnMenu(6, 0, 3, NoButton, 1);
      SelectInGroupOnMenu(6, 5, 9, NoButton, 1);
    }
  }

  // Make the RX button red if RX on
  SetButtonStatus(ButtonNumber(6, 21), RTLactive); 
}

void Define_Menu11()
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

  strcpy(MenuTitle[11], "Modulation Selection Menu (11)"); 

  // Bottom Row, Menu 11

  button = CreateButton(11, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(11, 0);
  AddButtonStatus(button, TabTXMode[2], &Blue);
  AddButtonStatus(button, TabTXMode[2], &Green);
  AddButtonStatus(button, TabTXMode[2], &Grey);

  button = CreateButton(11, 1);
  AddButtonStatus(button, TabTXMode[3], &Blue);
  AddButtonStatus(button, TabTXMode[3], &Green);
  AddButtonStatus(button, TabTXMode[3], &Grey);

  button = CreateButton(11, 2);
  AddButtonStatus(button, TabTXMode[4], &Blue);
  AddButtonStatus(button, TabTXMode[4], &Green);
  AddButtonStatus(button, TabTXMode[4], &Grey);

  button = CreateButton(11, 3);
  AddButtonStatus(button, TabTXMode[5], &Blue);
  AddButtonStatus(button, TabTXMode[5], &Green);
  AddButtonStatus(button, TabTXMode[5], &Grey);

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

  GreyOutReset11();
  if(strcmp(CurrentTXMode, TabTXMode[0])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 5, 1);
    SelectInGroupOnMenu(11, 0, 3, 5, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[1])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 6, 1);
    SelectInGroupOnMenu(11, 0, 3, 6, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[2])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 0, 1);
    SelectInGroupOnMenu(11, 0, 3, 0, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[3])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 1, 1);
    SelectInGroupOnMenu(11, 0, 3, 1, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[4])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 2, 1);
    SelectInGroupOnMenu(11, 0, 3, 2, 1);
  }
  if(strcmp(CurrentTXMode, TabTXMode[5])==0)
  {
    SelectInGroupOnMenu(11, 5, 6, 3, 1);
    SelectInGroupOnMenu(11, 0, 3, 3, 1);
  }
  GreyOut11();
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
  color_t Grey;
  color_t LBlue;
  color_t DBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Grey.r=127; Grey.g=127; Grey.b=127;
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

  button = CreateButton(13, 3);
  AddButtonStatus(button, TabModeOPtext[8], &Blue);
  AddButtonStatus(button, TabModeOPtext[8], &Green);
  AddButtonStatus(button, TabModeOPtext[8], &Grey);

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
  AddButtonStatus(button, TabModeOPtext[3], &Grey);

  button = CreateButton(13, 9);
  AddButtonStatus(button, TabModeOPtext[4], &Blue);
  AddButtonStatus(button, TabModeOPtext[4], &Green);
}

void Start_Highlights_Menu13()
{
  GreyOutReset13();
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
  if(strcmp(CurrentModeOP, TabModeOP[8]) == 0)
  {
    SelectInGroupOnMenu(13, 5, 9, 2, 1);
    SelectInGroupOnMenu(13, 0, 3, 3, 1);
  }
  GreyOut13();
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
    if (index == 8)
    {
      strcpy(FreqBtext, "Keyboard^");
      strcat(FreqBtext, FreqLabel[index]);
    }
    else
    {
      strcpy(FreqBtext, FreqLabel[index]);
    }
  }
  else
  {
    if (index == 8)
    {
      strcpy(FreqBtext, "Keyboard^T");
      TvtrFreq = atof(TabFreq[index]) + TabBandLO[CurrentBand];
      if (TvtrFreq < 0)
      {
        TvtrFreq = TvtrFreq * -1;
      }
      snprintf(Value, 10, "%.1f", TvtrFreq);
      strcat(FreqBtext, Value);
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

  if (CallingMenu == 1)
  {
    strcpy(MenuTitle[16], "Transmit Frequency Selection Menu (16)"); 
  }
  else if (CallingMenu == 5)
  {
    strcpy(MenuTitle[16], " Receive Frequency Selection Menu (16)"); 
  }


  // Update info in memory
  ReadPresets();

  // Look up current transmit frequency for highlighting
  strcpy(Param,"freqoutput");
  GetConfigParam(PATH_PCONFIG, Param, Value);

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
    if(((strcmp(Value, TabFreq[index]) == 0) && (CallingMenu == 1)) 
      || ((strcmp(RXfreq[0], TabFreq[index]) == 0) && (CallingMenu == 5)))
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

  if (CallingMenu == 1)
  {
    strcpy(MenuTitle[17], "Transmit Symbol Rate Selection Menu (17)"); 
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
  else if (CallingMenu == 5)
  {
    strcpy(MenuTitle[17], " Receive Symbol Rate Selection Menu (17)"); 

    if ( RXsr[0] == TabSR[0] )
    {
      SelectInGroupOnMenu(17, 0, 3, 5, 1);
      SelectInGroupOnMenu(17, 5, 9, 5, 1);
    }
    else if ( RXsr[0] == TabSR[1] )
    {
      SelectInGroupOnMenu(17, 0, 3, 6, 1);
      SelectInGroupOnMenu(17, 5, 9, 6, 1);
    }
    else if ( RXsr[0] == TabSR[2] )
    {
      SelectInGroupOnMenu(17, 0, 3, 7, 1);
      SelectInGroupOnMenu(17, 5, 9, 7, 1);
    }
    else if ( RXsr[0] == TabSR[3] )
    {
      SelectInGroupOnMenu(17, 0, 3, 8, 1);
      SelectInGroupOnMenu(17, 5, 9, 8, 1);
    }
    else if ( RXsr[0] == TabSR[4] )
    {
      SelectInGroupOnMenu(17, 0, 3, 9, 1);
      SelectInGroupOnMenu(17, 5, 9, 9, 1);
    }
    else if ( RXsr[0] == TabSR[5] )
    {
      SelectInGroupOnMenu(17, 0, 3, 0, 1);
      SelectInGroupOnMenu(17, 5, 9, 0, 1);
    }
    else if ( RXsr[0] == TabSR[6] )
    {
      SelectInGroupOnMenu(17, 0, 3, 1, 1);
      SelectInGroupOnMenu(17, 5, 9, 1, 1);
    }
    else if ( RXsr[0] == TabSR[7] )
    {
      SelectInGroupOnMenu(17, 0, 3, 2, 1);
      SelectInGroupOnMenu(17, 5, 9, 2, 1);
    }
    else if ( RXsr[0] == TabSR[8] )
    {
      SelectInGroupOnMenu(17, 0, 3, 3, 1);
      SelectInGroupOnMenu(17, 5, 9, 3, 1);
    }
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
  if (CallingMenu == 1)
  {
    strcpy(MenuTitle[18], "Transmit FEC Selection Menu (18)"); 
    strcpy(Param,"fec");
    strcpy(Value,"");
    GetConfigParam(PATH_PCONFIG,Param,Value);
    printf("Value=%s %s\n",Value,"Fec");
    fec=atoi(Value);
  }
  else
  {
    strcpy(MenuTitle[18], "Receive FEC Selection Menu (18)"); 
    fec = atoi(RXfec[0]);
  }
  switch(fec)
  {
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

// Menu 20 Stream Viewer

void Define_Menu20()
{
  int button;
  int n;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  color_t Red;


  Red.r=255; Red.g=0; Red.b=0;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;

  strcpy(MenuTitle[20], "Stream Viewer Selection Menu (20)"); 

  // Top Row, Menu 20
  button = CreateButton(20, 9);
  AddButtonStatus(button, "Amend^Preset", &Blue);
  AddButtonStatus(button, "Amend^Preset", &Red);

  // Bottom Row, Menu 20
  button = CreateButton(20, 4);
  AddButtonStatus(button, "Exit to^Main Menu", &DBlue);
  AddButtonStatus(button, "Exit to^Main Menu", &LBlue);

  for(n = 1; n < 9; n = n + 1)
  {
    if (n < 5)  // top row
    {
      button = CreateButton(20, n + 4);
      AddButtonStatus(button, StreamLabel[n], &Blue);
      AddButtonStatus(button, StreamLabel[n], &Green);
    }
    else       // Bottom Row
    {
      button = CreateButton(20, n - 5);
      AddButtonStatus(button, StreamLabel[n], &Blue);
      AddButtonStatus(button, StreamLabel[n], &Green);
    }
  }
}

void Start_Highlights_Menu20()
{
  // Stream Display Menu
  //char Param[255];
  //char Value[255];
  int n;
  // int NoButton;
  color_t Green;
  color_t Blue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  for(n = 1; n < 9; n = n + 1)
  {
    if (n < 5)  // top row
    {
      AmendButtonStatus(ButtonNumber(20, n + 4), 0, StreamLabel[n], &Blue);
      AmendButtonStatus(ButtonNumber(20, n + 4), 1, StreamLabel[n], &Green);
    }
    else       // Bottom Row
    {
      AmendButtonStatus(ButtonNumber(20, n - 5), 0, StreamLabel[n], &Blue);
      AmendButtonStatus(ButtonNumber(20, n - 5), 1, StreamLabel[n], &Green);
    }
  }
}


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

void Define_Menu25()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  color_t DBlue;
  color_t Grey;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  DBlue.r=0; DBlue.g=0; DBlue.b=64;
  Grey.r=127; Grey.g=127; Grey.b=127;

  strcpy(MenuTitle[25], "DVB-S2 FEC Selection Menu (25)"); 

  // Bottom Row, Menu 25

  button = CreateButton(25, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(25, 0);
  AddButtonStatus(button,"FEC 3/4",&Blue);
  AddButtonStatus(button,"FEC 3/4",&Green);
  AddButtonStatus(button,"FEC 3/4",&Grey);

  button = CreateButton(25, 1);
  AddButtonStatus(button,"FEC 5/6",&Blue);
  AddButtonStatus(button,"FEC 5/6",&Green);
  AddButtonStatus(button,"FEC 5/6",&Grey);

  button = CreateButton(25, 2);
  AddButtonStatus(button,"FEC 8/9",&Blue);
  AddButtonStatus(button,"FEC 8/9",&Green);
  AddButtonStatus(button,"FEC 8/9",&Grey);

  button = CreateButton(25, 3);
  AddButtonStatus(button,"FEC 9/10",&Blue);
  AddButtonStatus(button,"FEC 9/10",&Green);
  AddButtonStatus(button,"FEC 9/10",&Grey);

  // 2nd Row, Menu 25

  button = CreateButton(25, 5);
  AddButtonStatus(button,"FEC 1/4",&Blue);
  AddButtonStatus(button,"FEC 1/4",&Green);
  AddButtonStatus(button,"FEC 1/4",&Grey);

  button = CreateButton(25, 6);
  AddButtonStatus(button,"FEC 1/3",&Blue);
  AddButtonStatus(button,"FEC 1/3",&Green);
  AddButtonStatus(button,"FEC 1/3",&Grey);

  button = CreateButton(25, 7);
  AddButtonStatus(button,"FEC 1/2",&Blue);
  AddButtonStatus(button,"FEC 1/2",&Green);
  AddButtonStatus(button,"FEC 1/2",&Grey);

  button = CreateButton(25, 8);
  AddButtonStatus(button,"FEC 3/5",&Blue);
  AddButtonStatus(button,"FEC 3/5",&Green);
  AddButtonStatus(button,"FEC 3/5",&Grey);

  button = CreateButton(25, 9);
  AddButtonStatus(button,"FEC 2/3",&Blue);
  AddButtonStatus(button,"FEC 2/3",&Green);
  AddButtonStatus(button,"FEC 2/3",&Grey);
}

void Start_Highlights_Menu25()
{
  // DVB-S2 FEC
  char Param[255];
  char Value[255];
  int fec;

  strcpy(Param,"fec");
  strcpy(Value,"");
  GetConfigParam(PATH_PCONFIG,Param,Value);
  printf("Value=%s %s\n",Value,"Fec");
  fec=atoi(Value);
  GreyOutReset25();  // Un-grey all FECs
  switch(fec)
  {
    case 14:
      SelectInGroupOnMenu(25, 5, 9, 5, 1);
      SelectInGroupOnMenu(25, 0, 3, 5, 1);
      break;
    case 13:
      SelectInGroupOnMenu(25, 5, 9, 6, 1);
      SelectInGroupOnMenu(25, 0, 3, 6, 1);
      break;
    case 12:
      SelectInGroupOnMenu(25, 5, 9, 7, 1);
      SelectInGroupOnMenu(25, 0, 3, 7, 1);
      break;
    case 35:
      SelectInGroupOnMenu(25, 5, 9, 8, 1);
      SelectInGroupOnMenu(25, 0, 3, 8, 1);
      break;
    case 23:
      SelectInGroupOnMenu(25, 5, 9, 9, 1);
      SelectInGroupOnMenu(25, 0, 3, 9, 1);
      break;
    case 34:
      SelectInGroupOnMenu(25, 5, 9, 0, 1);
      SelectInGroupOnMenu(25, 0, 3, 0, 1);
      break;
    case 56:
      SelectInGroupOnMenu(25, 5, 9, 1, 1);
      SelectInGroupOnMenu(25, 0, 3, 1, 1);
      break;
    case 89:
      SelectInGroupOnMenu(25, 5, 9, 2, 1);
      SelectInGroupOnMenu(25, 0, 3, 2, 1);
      break;
    case 91:
      SelectInGroupOnMenu(25, 5, 9, 3, 1);
      SelectInGroupOnMenu(25, 0, 3, 3, 1);
      break;
  }
  GreyOut25();  // Grey out illegal FECs
}


void Define_Menu26()
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
  char BandLabel[31];

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
  AddButtonStatus(button, "Exit", &DBlue);
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
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;

  if(CallingMenu == 301)
  {
    strcpy(MenuTitle[26], "Band Details Setting Menu (26)");
  }
  else
  {
    strcpy(MenuTitle[26], "Receiver LO Setting Menu (26)");
  }

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

void Define_Menu30()
{
  int button;
  color_t Green;
  color_t Blue;
  color_t LBlue;
  Green.r=0; Green.g=128; Green.b=0;
  Blue.r=0; Blue.g=0; Blue.b=128;
  LBlue.r=64; LBlue.g=64; LBlue.b=192;
  char BandLabel[31];

  strcpy(MenuTitle[30], "Comp Video Band Selection Menu (30)"); 

  // Bottom Row, Menu 30

  button = CreateButton(30, 0);
  AddButtonStatus(button, TabBandLabel[5], &Blue);
  AddButtonStatus(button, TabBandLabel[5], &Green);

  button = CreateButton(30, 1);
  AddButtonStatus(button, TabBandLabel[6], &Blue);
  AddButtonStatus(button, TabBandLabel[6], &Green);

  button = CreateButton(30, 2);
  AddButtonStatus(button, TabBandLabel[7], &Blue);
  AddButtonStatus(button, TabBandLabel[7], &Green);

  button = CreateButton(30, 3);
  strcpy(BandLabel, "Transvtr^");
  strcat(BandLabel, TabBandLabel[8]);
  AddButtonStatus(button, TabBandLabel[8], &Blue);
  AddButtonStatus(button, TabBandLabel[8], &Green);

  button = CreateButton(30, 4);
  AddButtonStatus(button, "Exit", &Blue);
  AddButtonStatus(button, "Exit", &LBlue);

  // 2nd Row, Menu 30

  button = CreateButton(30, 5);
  AddButtonStatus(button, TabBandLabel[0], &Blue);
  AddButtonStatus(button, TabBandLabel[0], &Green);

  button = CreateButton(30, 6);
  AddButtonStatus(button, TabBandLabel[1], &Blue);
  AddButtonStatus(button, TabBandLabel[1], &Green);

  button = CreateButton(30, 7);
  AddButtonStatus(button, TabBandLabel[2], &Blue);
  AddButtonStatus(button, TabBandLabel[2], &Green);

  button = CreateButton(30, 8);
  AddButtonStatus(button, TabBandLabel[3], &Blue);
  AddButtonStatus(button, TabBandLabel[3], &Green);

  button = CreateButton(30, 9);
  AddButtonStatus(button, TabBandLabel[4], &Blue);
  AddButtonStatus(button, TabBandLabel[4], &Green);
}

void Start_Highlights_Menu30()
{
  // Set Band for Comp Vid out Contest Captions

  printf("Entering Start Highlights Menu30\n");

  SelectInGroupOnMenu(CurrentMenu, 5, 9, CompVidBand + 5, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, CompVidBand + 5, 1);
  if (CompVidBand > 4) // Bottom row selected
  {
    SelectInGroupOnMenu(CurrentMenu, 0, 3, CompVidBand - 5, 1);
    SelectInGroupOnMenu(CurrentMenu, 5, 9, CompVidBand - 5, 1);
  }
}

void Define_Menu31()
{
  int button;
  color_t Blue;
  Blue.r=0; Blue.g=0; Blue.b=128;
  int i;
  int j;
  char Param[15];
  char DispName[10][20];

  strcpy(MenuTitle[31], "Amend Site Name and Locator (31)"); 
  
  for(i = 0; i < 10 ;i++)
  {
    sprintf(Param, "bcallsign%d", i);
    GetConfigParam(PATH_LOCATORS, Param, DispName[i]);
    DispName[i][8] = '\0';
    j = i + 5;
    if (i > 4)
    {
      j = i - 5;
    }
    button = CreateButton(31, j);
    AddButtonStatus(button, DispName[i], &Blue);
  }
}

void Start_Highlights_Menu31()
{
  //
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

  if (GetLinuxVer() == 9)
  {
    button = CreateButton(33, 0);
    AddButtonStatus(button, "Install^Lime Mini", &Blue);
    AddButtonStatus(button, "Install^Lime Mini", &Green);

    button = CreateButton(33, 1);
    AddButtonStatus(button, "Update^Lime FW", &Blue);
    AddButtonStatus(button, "Update^Lime FW", &Green);
  }

  button = CreateButton(33, 3);
  AddButtonStatus(button, "MPEG-2^License", &Blue);
  AddButtonStatus(button, "MPEG-2^License", &Green);

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
  color_t Green;
  Blue.r=0; Blue.g=0; Blue.b=128;
  Red.r=128; Red.g=0; Red.b=0;
  Grey.r=127; Grey.g=127; Grey.b=127;
  Green.r=0; Green.g=128; Green.b=0;

  if ((ButtonArray[ButtonNumber(CurrentMenu, 0)].IndexStatus > 0) && (CheckLimeInstalled() == 0))  // Lime was not installed, but now is
  {
    AmendButtonStatus(ButtonNumber(33, 0), 0, "Update^Lime Drvr", &Blue);
    AmendButtonStatus(ButtonNumber(33, 0), 1, "Update^Lime Drvr", &Green);
  }

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
  color_t Blue;
  color_t LBlue;
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
  button = CreateButton(41, 5);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
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
  char Commnd[255];

  strcpy(ModeInput, "DESKTOP"); // Set input so webcam reset script is not called
  TransmitStop();
  ReceiveStop();
  RTLstop();
  system("killall -9 omxplayer.bin >/dev/null 2>/dev/null");
  finish();
  DisplayHere("Touchscreen Process Stopped");
  printf("Terminate\n");
  sprintf(Commnd,"sudo killall express_server >/dev/null 2>/dev/null");
  system(Commnd);
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
    ProcessLeandvb2(); // For FrMenu and no 
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
  ReadRTLPresets();
  ReadRXPresets();
  ReadStreamPresets();


  // Initialise all the button Status Indexes to 0
  InitialiseButtons();

  // Define all the Menu buttons
  Define_Menu1();
  Define_Menu2();
  Define_Menu3();
  Define_Menu4();
  Define_Menu5();
  Define_Menu6();

  Define_Menu11();
  Define_Menu12();
  Define_Menu13();
  Define_Menu14();
  Define_Menu15();
  Define_Menu16();
  Define_Menu17();
  Define_Menu18();
  Define_Menu19();
  Define_Menu20();
  Define_Menu21();
  Define_Menu22();
  Define_Menu23();
  Define_Menu24();
  Define_Menu25();
  Define_Menu26();
  Define_Menu27();
  Define_Menu28();
  Define_Menu29();
  Define_Menu30();
  Define_Menu31();
  Define_Menu32();
  Define_Menu33();

  Define_Menu41();

  // Start the button Menu
  Start(wscreen,hscreen);

  // Check if DATV Express Server required and, if so, start it
  CheckExpress();

  // Check Lime connected if selected
  CheckLimeReady();

  // Set the Band (and filter) Switching
  // Must be done after (not before) starting DATV Express Server
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using portsdown_config.txt
  usleep(100000);

  // Start the receive downconverter LO if required
  ReceiveLOStart();

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
