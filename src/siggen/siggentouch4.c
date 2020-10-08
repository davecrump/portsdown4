// siggentouch4.c
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

Rewitten by Dave, G8GKQ
Thanks to Alberto Ferraris IU1KVL For his plutotx and to Robin Getz 
for his comments.  The Pluto SigGen would not have been possible
without them 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fftw3.h>
#include <getopt.h>
#include <linux/input.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <wiringPi.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <time.h>

#include "font/font.h"
#include "touch.h"
#include "Graphics.h"
#include "/home/pi/libiio/iio.h"  // for Pluto

#define KWHT  "\x1B[37m"
#define KYEL  "\x1B[33m"

#define PATH_PCONFIG "/home/pi/rpidatv/scripts/portsdown_config.txt"
#define PATH_PPRESETS "/home/pi/rpidatv/scripts/portsdown_presets.txt"
#define PATH_LIME_CAL "/home/pi/rpidatv/scripts/limecalfreq.txt"
#define PATH_SGCONFIG "/home/pi/rpidatv/src/siggen/siggenconfig.txt"
#define PATH_CAL "/home/pi/rpidatv/src/siggen/siggencal.txt"
#define PATH_ATTEN "/home/pi/rpidatv/bin/set_attenuator "

#define PI 3.14159265358979323846
#define deg2rad(DEG) ((DEG)*((PI)/(180.0)))
#define rad2deg(RAD) ((RAD)*180/PI)
#define DELIM "."

int fd=0;
int wscreen, hscreen;
float scaleXvalue, scaleYvalue; // Coeff ratio from Screen/TouchArea
int wbuttonsize;
int hbuttonsize;
int swbuttonsize;
int shbuttonsize;

typedef struct
{
  int r, g, b;
} color_t;

typedef struct
{
  char Text[255];
  color_t  Color;
} status_t;

#define MAX_STATUS 5
typedef struct
{
  int x, y, w, h;                // Position and size of button
  status_t Status[MAX_STATUS];   // Array of text and required colour for each status
  int IndexStatus;               // The number of valid status definitions.  0 = do not display
  int NoStatus;                  // This is the active status (colour and text)
} button_t;

// Set the Colours up front
color_t Black = {.r = 0  , .g = 0  , .b = 0  };
color_t Green = {.r = 0  , .g = 128, .b = 0  };
color_t Blue  = {.r = 0  , .g = 0  , .b = 128};
color_t LBlue = {.r = 64 , .g = 64 , .b = 192};
color_t DBlue = {.r = 0  , .g = 0  , .b = 64 };
color_t Grey  = {.r = 127, .g = 127, .b = 127};
color_t Red   = {.r = 255, .g = 0  , .b = 0  };

#define MAX_BUTTON 350
int IndexButtonInArray=0;
button_t ButtonArray[MAX_BUTTON];
#define TIME_ANTI_BOUNCE 500
int CurrentMenu = 1;
int CallingMenu = 1;

//GLOBAL PARAMETERS

// Sig Gen

char osc[15] = "pluto";      // Output device
char osc_text[15] = "Pluto"; // Output device display text
// pluto   Pluto
// adf4351 ADF4351
// adf5355 ADF5355
// elcom   Elcom
// express DATV Express
// lime    Lime Mini

int64_t DisplayFreq = 437000000;  // Input freq and display freq are the same
int64_t OutputFreq = 0;           // Calculated output frequency
int DisplayLevel = 987;           // calculated for display from (LO) level, atten and freq
int OutputStatus = 0;             // 0 for off, 1 for on
int ModOn = 0;                    // 0 for carrier, 1 for QPSK
int level = 0;                    // current LO level.  Raw data 0-3 for ADF, 0 - 50 for Exp
float atten = 31.5;               // current atten level.  Raw data (0 - 31.75) 0.25 dB steps
int AttenIn = 0;                  // 0 = No attenuator, 1 = attenuator in circuit
char AttenType[255] = "NONE";  // or PE4302 (0.5 dB steps) or HMC1119 (0.25dB) or NONE

char ref_freq_4351[63] = "25000000";        // read on startup from siggenconfig.txt
char ref_freq_5355[63] = "26000000";        // read on startup from siggenconfig.txt
uint64_t SourceUpperFreq = 13600000000;     // set every time an oscillator is selected
uint64_t SourceLowerFreq = 54000000;        // set every time an oscillator is selected

uint64_t CalFreq[50];
int CalLevel[50];
int CalPoints;

char UpdateStatus[31] = "NotAvailable";

char LinuxCommand[511];
int  scaledX, scaledY;

// Wiring Pi numbers for GPIOs
int GPIO_PTT = 29;
int GPIO_SPI_CLK = 21;
int GPIO_SPI_DATA = 22;
int GPIO_4351_LE = 23;   // Changed for RPi 4
int GPIO_Atten_LE = 16;
int GPIO_5355_LE = 15;   // Also Elcom LE
int GPIO_Band_LSB = 26;  // Band D0, Changed for RPi 4
int GPIO_Band_MSB = 24;  // Band D1
int GPIO_Tverter = 7;    // Band D2
int GPIO_SD_LED = 2;
int debug_level = 0; // 0 minimum, 1 medium, 2 max

char MenuTitle[50][127];
char KeyboardReturn[64];

// Lime Control
float LimeCalFreq = 0;  // -2 cal never, -1 = cal every time, 0 = cal next time, freq = no cal if no change
int LimeRFEState = 0;   // 0 = disabled, 1 = enabled

// Langstone Integration variables
char PlutoIP[16];             // Pluto IP address

// Touch display variables
int PresetStoreTrigger = 0;   //Set to 1 if awaiting preset being stored
int FinishedButton = 0;       // Used to indicate screentouch during TX or RX
int touch_response = 0;       // set to 1 on touch and used to reboot display if it locks up

// Function Prototypes

void Start_Highlights_Menu1();
void Start_Highlights_Menu2();
void Start_Highlights_Menu3();
void Start_Highlights_Menu4();
void Start_Highlights_Menu11();
void Start_Highlights_Menu12();

void MsgBox(char *);
void MsgBox2(char *, char *);
void MsgBox4(char *, char *, char *, char *);
void wait_touch();
void waituntil(int, int);
void Keyboard(char *, char *, int);
void DisplayLogo();
int getTouchSample(int *, int *, int *);
void TransformTouchMap(int, int);
int ButtonNumber(int, int);
int CheckLimeMiniConnect();
int CheckLimeUSBConnect();
static void cleanexit(int);
int LimeGWRev();
void ImposeBounds();
void ShowTitle();
void UpdateWindow();
void adf4351On(int);
void ElcomOn();
int plutotx(bool);

// PLUTO Constants and declarations

#define REFTXPWR 10
#define FBANDWIDTH 4000000
#define FSAMPLING 4000000
#define FCW 1000000

struct iio_channel *tx0_i, *tx0_q, *tx1_i, *tx1_q;
struct iio_context *ctx;
struct iio_device *phy;
struct iio_device *dds_core_lpc;
struct iio_channel *tx_chain;
struct iio_channel *tx_lo;
bool PlutoCalValid = false;

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
    if (debug_level == 2)
    {
      printf("Get Config reads %s for %s and returns %s\n", PathConfigFile, Param, Value);
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

  if (debug_level == 2)
  {
    printf("Set Config called %s %s %s\n", PathConfigFile , ParamWithEquals, Value);
  }

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
 * @brief safely copies n characters of instring to outstring without overflow
 *
 * @param *outstring
 * @param *instring
 * @param n int number of characters to copy.  Max value is the outstring array size -1
 *
 * @return void
*******************************************************************************/
void strcpyn(char *outstring, char *instring, int n)
{
  //printf("\ninstring= -%s-, instring length = %d, desired length = %d\n", instring, strlen(instring), strnlen(instring, n));
  
  n = strnlen(instring, n);
  int i;
  for (i = 0; i < n; i = i + 1)
  {
    //printf("i = %d input character = %c\n", i, instring[i]);
    outstring[i] = instring[i];
  }
  outstring[n] = '\0'; // Terminate the outstring
  //printf("i = %d input character = %c\n", n, instring[n]);
  //printf("i = %d input character = %c\n\n", (n + 1), instring[n + 1]);

  //for (i = 0; i < n; i = i + 1)
  //{
  //  printf("i = %d output character = %c\n", i, outstring[i]);
  //}  
  //printf("i = %d output character = %c\n", n, outstring[n]);
  //printf("i = %d output character = %c\n", (n + 1), outstring[n + 1]);

  //printf("outstring= -%s-, length = %d\n\n", outstring, strlen(outstring));
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
 * @brief int is_valid_ip(char *ip_str) Checks whether an IP address is valid
 *
 * @param char *ip_str (which gets mangled)
 *
 * @return 1 if IP string is valid, else return 0
*******************************************************************************/

/* return 1 if string contain only digits, else return 0 */
int valid_digit(char *ip_str) 
{ 
  while (*ip_str) 
  { 
    if (*ip_str >= '0' && *ip_str <= '9')
    { 
      ++ip_str; 
    }
    else
    {
      return 0; 
    }
  } 
  return 1; 
} 

int is_valid_ip(char *ip_str) 
{ 
  int num, dots = 0; 
  char *ptr; 
  
  if (ip_str == NULL) 
  {
    return 0; 
  }

  ptr = strtok(ip_str, DELIM); 
  if (ptr == NULL) 
  {
    return 0; 
  }
  
  while (ptr)
  { 
    // after parsing string, it must contain only digits
    if (!valid_digit(ptr)) 
    {
      return 0; 
    }  
    num = atoi(ptr); 
  
    // check for valid numbers
    if (num >= 0 && num <= 255)
    { 
      // parse remaining string
      ptr = strtok(NULL, DELIM); 
      if (ptr != NULL)
      {
        ++dots; 
      }
    }
    else
    {
      return 0; 
    }
  }

  // valid IP string must contain 3 dots
  if (dots != 3)
  { 
    return 0;
  }
  return 1;
} 

/***************************************************************************//**
 * @brief Performs Lime firmware update and checks GW revision
 *
 * @param nil
 *
 * @return void
*******************************************************************************/

void LimeFWUpdate(int button)
{
  // Portsdown 4 and selectable FW.  0 = 1.29. 1 = 1.30, 2 = Custom
  if (CheckLimeUSBConnect() == 0)
  {
    MsgBox4("Upgrading Lime USB", "To latest standard", "Using LimeUtil 19.04", "Please Wait");
    system("LimeUtil --update");
    usleep(250000);
    MsgBox4("Upgrade Complete", " ", "Touch Screen to Continue" ," ");
  }
  else if (CheckLimeMiniConnect() == 0)
  {
    switch (button)
    {
    case 0:
      MsgBox4("Upgrading Lime Firmware", "to 1.29", " ", " ");
      system("sudo LimeUtil --fpga=/home/pi/.local/share/LimeSuite/images/19.01/LimeSDR-Mini_HW_1.2_r1.29.rpd");
      if (LimeGWRev() == 29)
      {
        MsgBox4("Firmware Upgrade Successful", "Now at Gateware 1.29", "Touch Screen to Continue" ," ");
      }
      else
      {
        MsgBox4("Firmware Upgrade Unsuccessful", "Further Investigation required", "Touch Screen to Continue" ," ");
      }
      break;
    case 1:
      MsgBox4("Upgrading Lime Firmware", "to 1.30", " ", " ");
      system("sudo LimeUtil --fpga=/home/pi/.local/share/LimeSuite/images/19.04/LimeSDR-Mini_HW_1.2_r1.30.rpd");
      if (LimeGWRev() == 30)
      {
        MsgBox4("Firmware Upgrade Successful", "Now at Gateware 1.30", "Touch Screen to Continue" ," ");
      }
      else
      {
        MsgBox4("Firmware Upgrade Unsuccessful", "Further Investigation required", "Touch Screen to Continue" ," ");
      }
      break;
    case 2:
      MsgBox4("Upgrading Lime Firmware", "to Custom DVB", " ", " ");
      system("sudo LimeUtil --force --fpga=/home/pi/.local/share/LimeSuite/images/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd");

      MsgBox4("Firmware Upgrade Complete", "DVB", "Touch Screen to Continue" ," ");
      break;
    default:
      printf("Lime Update button selection error\n");
      break;
    }
  }
  else
  {
    MsgBox4("No LimeSDR Detected", " ", "Touch Screen to Continue", " ");
  }
  wait_touch();
}


/***************************************************************************//**
 * @brief Checks if a file exists
 *
 * @param nil
 *
 * @return 0 if exists, 1 if not
*******************************************************************************/

int file_exist (char *filename)
{
  if ( access( filename, R_OK ) != -1 ) 
  {
    // file exists
    return 0;
  }
  else
  {
    // file doesn't exist
    return 1;
  }
}


/***************************************************************************//**
 * @brief Reads the current saved state from siggenconfig.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void ReadSavedState()
{
  char value[63] = "0";

  strcpy(osc, "pluto");
  GetConfigParam(PATH_SGCONFIG, "osc", osc);

  GetConfigParam(PATH_SGCONFIG, "freq", value);
  DisplayFreq = strtoll(value, 0, 0);

  strcpy(value, "0");
  GetConfigParam(PATH_SGCONFIG,"level", value);
  level = atoi(value);

  strcpy(value, "0");
  GetConfigParam(PATH_SGCONFIG,"attenlevel", value);
  atten = atof(value);

  strcpy(AttenType, "NONE");
  GetConfigParam(PATH_SGCONFIG,"attenuator", AttenType);

  // ref_freq_4351 is initialised to 25000000 and stays as a string
  GetConfigParam(PATH_SGCONFIG,"adf4351ref", ref_freq_4351);

  // ref_freq_5355 is initialised to 26000000 and stays as a string
  GetConfigParam(PATH_SGCONFIG,"adf5355ref", ref_freq_5355);

  // Read Pluto IP
  GetConfigParam(PATH_PCONFIG,"plutoip", PlutoIP);
}

/***************************************************************************//**
 * @brief Saves the current state to siggenconfig.txt
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void SaveState()
{
  char value[63] = "0";

  SetConfigParam(PATH_SGCONFIG, "osc", osc);

  snprintf(value, 12, "%lld", DisplayFreq);
  SetConfigParam(PATH_SGCONFIG, "freq", value);

  snprintf(value, 4, "%d", level);
  SetConfigParam(PATH_SGCONFIG, "level", value);

  snprintf(value, 6, "%.2f", atten);
  SetConfigParam(PATH_SGCONFIG,"attenlevel", value);

  SetConfigParam(PATH_SGCONFIG,"attenuator", AttenType);
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


/***************************************************************************//**
 * @brief Checks for the presence of an FTDI Device
 *        
 * @param None
 *
 * @return 0 if present, 1 if not present
*******************************************************************************/

int CheckFTDI()
{
  char FTDIStatus[256];
  FILE *fp;
  int ftdistat = 1;

  /* Open the command for reading. */
  fp = popen("/home/pi/rpidatv/scripts/check_ftdi.sh", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time - output it
  while (fgets(FTDIStatus, sizeof(FTDIStatus)-1, fp) != NULL)
  {
    if (FTDIStatus[0] == '0')
    {
      printf("FTDI Detected\n" );
      ftdistat = 0;
    }
    else
    {
      printf("No FTDI Detected\n" );
    }
  }
  pclose(fp);
  return(ftdistat);
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
    MsgBox4("Please wait 5 seconds", "while the DATV Express firmware", "is loaded", " ");
    usleep(5000000);
    responseint = CheckExpressRunning();
    if (responseint == 0)  // Running OK
    {
      MsgBox4(" ", " ", " ", "DATV Express Firmware Loaded");
      usleep(1000000);
    }
    else
    {
      MsgBox4("Failed to load", "DATV Express firmware.", "Please check connections", "and try again");
      wait_touch();
    }
    setBackColour(0, 0, 0);
    clearScreen();
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
  if (strcmp(osc, "express") == 0)  // Startup mode is DATV Express
  {
    if (CheckExpressConnect() == 1)   // Not connected
    {
      MsgBox2("DATV Express Not connected", "Connect it now or select another mode");
      wait_touch();
      setBackColour(0, 0, 0);
      clearScreen();
    }
    if (CheckExpressConnect() != 1)   // Connected
    {
      StartExpressServer();
    }
  }
}


/***************************************************************************//**
 * @brief Checks whether a Lime Mini is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckLimeMiniConnect()
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
 * @brief Checks whether a Lime USB is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckLimeUSBConnect()
{
  FILE *fp;
  char response[255];
  int responseint;

  /* Open the command for reading. */
  fp = popen("lsusb | grep -q '1d50:6108' ; echo $?", "r");
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
 * @brief Checks whether a Pluto is connected
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckPlutoConnect()
{
  FILE *fp;
  char response[127];

  /* Open the command for reading. */
  fp = popen("timeout 0.2 ping pluto.local -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");
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
 * @brief Checks whether a Pluto is connected on the IP address in the Config File
 *
 * @param 
 *
 * @return 0 if present, 1 if absent
*******************************************************************************/

int CheckPlutoIPConnect()
{
  FILE *fp;
  char response[127];
  char plutoping[127];

  strcpy(plutoping, "timeout 0.2 ping ");
  strcat(plutoping, PlutoIP);
  strcat(plutoping, " -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11");

  /* Open the command for reading. */
  // fp = popen("timeout 0.2 ping pluto.local -c1 | head -n 5 | tail -n 1 | grep -o \"1 received,\" | head -c 11", "r");
  fp = popen(plutoping, "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(response, 12, fp) != NULL)
  {
    //printf("%s", response);
  }

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
 * @brief Checks whether a Lime Mini or Lime USB is connected if selected
 *        and displays error message if not
 * @param 
 *
 * @return void
*******************************************************************************/

void CheckLimeReady()
{
  if (strcmp(osc, "lime") == 0)  // Lime mini selected
  {
    if (CheckLimeMiniConnect() == 1)
    {
      if (CheckLimeUSBConnect() == 0)
      {
        MsgBox2("LimeUSB Detected, LimeMini Selected", "Please select LimeMini");
      }
      else
      {
        MsgBox2("No LimeMini Detected", "Check Connections");
      }
      wait_touch();
    }
  }
}

/***************************************************************************//**
 * @brief Displays Info about a connected Lime
 *        
 * @param 
 *
 * @return void
*******************************************************************************/

void LimeInfo()
{
  MsgBox4("Please wait", " ", " ", " ");

  // Initialise and calculate the text display
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_22;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  FILE *fp;
  char response[255];
  int line = 0;

  /* Open the command for reading. */
  fp = popen("LimeUtil --make", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  clearScreen();
  Text2(wscreen/12, hscreen - 1 * linepitch, "Lime Firmware Information", font_ptr);

  /* Read the output a line at a time - output it. */
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      Text2(wscreen/12, hscreen - (1.2 * line + 2) * linepitch, response, font_ptr);
    }
    line = line + 1;
  }

  /* close */
  pclose(fp);
  Text2(wscreen/12, 1.2 * linepitch, "Touch Screen to Continue", font_ptr);
}

/***************************************************************************//**
 * @brief Returns Lime Gateware Revision
 *        
 * @param 
 *
 * @return int Lime Gateware revision 26, 27 or 28
*******************************************************************************/

int LimeGWRev()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int GWRev = 0;

  // Open the command for reading
  fp = popen("LimeUtil --make", "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[19] = '\0';
      if (strcmp(test_string, "  Gateware revision") == 0)
      {
        strncpy(test_string, &response[21], strlen(response));
        test_string[strlen(response)-21] = '\0';
        GWRev = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return GWRev;
}

/***************************************************************************//**
 * @brief Returns Lime Gateware Version
 *        
 * @param 
 *
 * @return int Lime Gateware Version 1?
*******************************************************************************/
int LimeGWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int GWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Gateware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        GWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return GWVer;
}

/***************************************************************************//**
 * @brief Returns Lime Firmware Version
 *        
 * @param 
 *
 * @return int Lime Firmware version 5?
*******************************************************************************/
int LimeFWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int FWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Firmware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        FWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return FWVer;
}

/***************************************************************************//**
 * @brief Returns Lime Hardware version
 *        
 * @param 
 *
 * @return int Lime Hardware version 1 or 2
*******************************************************************************/
int LimeHWVer()
{
  FILE *fp;
  char response[255];
  char test_string[255];
  int line = 0;
  int HWVer = 0;

  fp = popen("LimeUtil --make", "r");
  if (fp == NULL)
  {
    printf("Failed to run command\n" );
    exit(1);
  }

  // Read the output a line at a time
  while (fgets(response, 50, fp) != NULL)
  {
    if (line > 0)    //skip first line
    {
      strcpy(test_string, response);
      test_string[18] = '\0';
      if (strcmp(test_string, "  Hardware version") == 0)
      {
        // Copy the text to the right of the deleted character
        strncpy(test_string, &response[20], strlen(response));
        test_string[strlen(response)-20] = '\0';
        HWVer = atoi(test_string);
      }
    }
    line = line + 1;
  }

  pclose(fp);
  return HWVer;
}


/***************************************************************************//**
 * @brief Stops the graphics system and displays the Portdown Logo
 *        
 * @param nil
 *
 * @return void
*******************************************************************************/

void DisplayLogo()
{
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");
}


void TransformTouchMap(int x, int y)
{
  // This function takes the raw (0 - 4095 on each axis) touch data x and y
  // and transforms it to approx 0 - wscreen and 0 - hscreen in globals scaledX 
  // and scaledY prior to final correction by CorrectTouchMap  

  scaledX = x / scaleXvalue;
  scaledY = hscreen - y / scaleYvalue;
}


int IsMenuButtonPushed(int x, int y)
{
  int  i, NbButton, cmo, cmsize;
  NbButton = -1;
  int margin=10;  // was 20
  cmo = ButtonNumber(CurrentMenu, 0); // Current Menu Button number Offset
  cmsize = ButtonNumber(CurrentMenu + 1, 0) - ButtonNumber(CurrentMenu, 0);
  TransformTouchMap(x,y);       // Sorts out orientation and approx scaling of the touch map

  //printf("x=%d y=%d scaledx %d scaledy %d sxv %f syv %f Button %d\n",x,y,scaledX,scaledY,scaleXvalue,scaleYvalue, NbButton);

  // For each button in the current Menu, check if it has been pushed.
  // If it has been pushed, return the button number.  If nothing valid has been pushed return -1
  // If it has been pushed, do something with the last event time

  for (i = 0; i <cmsize; i++)
  {
    if (ButtonArray[i + cmo].IndexStatus > 0)  // If button has been defined
    {
      //printf("Button %d, ButtonX = %d, ButtonY = %d\n", i, ButtonArray[i + cmo].x, ButtonArray[i + cmo].y);

      if  ((scaledX <= (ButtonArray[i + cmo].x + ButtonArray[i + cmo].w - margin))
        && (scaledX >= ButtonArray[i + cmo].x + margin)
        && (scaledY <= (ButtonArray[i + cmo].y + ButtonArray[i + cmo].h - margin))
        && (scaledY >= ButtonArray[i + cmo].y + margin))  // and touched
      {
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

int AddButton(int x, int y, int w, int h)
{
  button_t *NewButton=&(ButtonArray[IndexButtonInArray]);
  NewButton->x = x;
  NewButton->y = y;
  NewButton->w = w;
  NewButton->h = h;
  NewButton->NoStatus = 0;
  NewButton->IndexStatus = 0;
  return IndexButtonInArray++;
}

int ButtonNumber(int MenuIndex, int Button)
{
  // Returns the Button Number (0 - 349) from the Menu number and the button position
  int ButtonNumb = 0;

  if (MenuIndex <= 10)  // 10 x 25-button menus
  {
    ButtonNumb = (MenuIndex - 1) * 25 + Button;
  }
  if ((MenuIndex >= 11) && (MenuIndex <= 12))  // 2 x 50-button menus (control and keyboard)
  {
    ButtonNumb = 250 + (MenuIndex - 11) * 50 + Button;
  }
  return ButtonNumb;
}

int CreateButton(int MenuIndex, int ButtonPosition)
{
  // Provide Menu number (int 1 - 12), Button Position (0 bottom left, 23 top right)
  // return button number

  // Menus 1 - 10 are classic 25-button menus
  // Menu 11 is the 50-button control menu
  // Menu 12 is the keyboard

  int ButtonIndex;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  swbuttonsize = (wscreen - 25) / 12;  // width of small button
  shbuttonsize = hscreen / 10;         // height of small button


  ButtonIndex = ButtonNumber(MenuIndex, ButtonPosition);

  if (MenuIndex <= 10)  // All except contro, and keyboard
  {
    if (ButtonPosition < 20)  // Bottom 4 rows
    {
      x = (ButtonPosition % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 0.9;
    }
    else if (ButtonPosition == 20)  // TX button
    {
      x = (ButtonPosition % 5) * wbuttonsize *1.7 + 20;    // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 1.2;
      h = hbuttonsize * 1.2;
    }
    else if ((ButtonPosition == 21) || (ButtonPosition == 22) || (ButtonPosition == 23)) // RX/M1, M2 and M3 buttons
    {
      x = ((ButtonPosition + 1) % 5) * wbuttonsize + 20;  // % operator gives the remainder of the division
      y = (ButtonPosition / 5) * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 1.2;
    }
  }
  else if (MenuIndex == 11)  // Control Menu - bespoke
  {
    if (ButtonPosition <= 2)  // Bottom row: subtract 10s, units and tenths of a dB
    {
      x = (ButtonPosition + 1) * swbuttonsize + 20;  
      y = 0 * shbuttonsize + 20;
      w = swbuttonsize * 0.9;
      h = shbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 3) && (ButtonPosition <= 4))  // Save and Mod Buttons
    {
      x = (ButtonPosition) * wbuttonsize + 20;
      y = 0.5 * hbuttonsize + 20;
      w = wbuttonsize * 0.9;
      h = hbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 5) && (ButtonPosition <= 7))  // Bottom row: add 10s, units and tenths of a dB
    {
      x = (ButtonPosition - 4) * swbuttonsize + 20;  
      y = 2 * shbuttonsize + 20;
      w = swbuttonsize * 0.9;
      h = shbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 8) && (ButtonPosition <= 18))  // Decrement frequency: 11 digits 10 GHz to 1 Hz
    {
      if ((ButtonPosition >= 8) && (ButtonPosition <= 9))  // GHz
      {
        x = (0.4 + (ButtonPosition - 8)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 10) && (ButtonPosition <= 12))  // MHz
      {
        x = (0.6 + (ButtonPosition - 8)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 13) && (ButtonPosition <= 15))  // kHz
      {
        x = (0.8 + (ButtonPosition - 8)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 16) && (ButtonPosition <= 18))  // Hz
      {
        x = (1.0 + (ButtonPosition - 8)) * swbuttonsize + 20;
      }
      y = 3.5 * shbuttonsize + 20;
      w = swbuttonsize * 0.9;
      h = shbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 19) && (ButtonPosition <= 29))  // Increment frequency: 11 digits 10 GHz to 1 Hz
    {
      if ((ButtonPosition >= 19) && (ButtonPosition <= 20))  // GHz
      {
        x = (0.4 + (ButtonPosition - 19)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 21) && (ButtonPosition <= 23))  // MHz
      {
        x = (0.6 + (ButtonPosition - 19)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 24) && (ButtonPosition <= 26))  // kHz
      {
        x = (0.8 + (ButtonPosition - 19)) * swbuttonsize + 20;
      }
      if ((ButtonPosition >= 27) && (ButtonPosition <= 29))  // Hz
      {
        x = (1.0 + (ButtonPosition - 19)) * swbuttonsize + 20;
      }
      y = 5.5 * shbuttonsize + 20;
      w = swbuttonsize * 0.9;
      h = shbuttonsize * 0.9;
    }
    if ((ButtonPosition >= 30) && (ButtonPosition <= 33)) // ON, OFF, EXIT, CTRL buttons
    {
      x = (ButtonPosition - 30) * 1.25 * wbuttonsize + 20;
      y = 4 * hbuttonsize + 20;
      w = wbuttonsize * 1.1;
      h = hbuttonsize * 1.2;
    }
  }
  else  // Keyboard
  {
    w = 65;
    h = 59;

    if (ButtonPosition <= 9)  // Space bar and < > - Enter, Bkspc
    {
      switch (ButtonPosition)
      {
      case 0:                   // Space Bar
          y = 0;
          x = 165; // wscreen * 5 / 24;
          w = 362; // wscreen * 11 /24;
          break;
      case 1:                  // Not used
          y = 0;
          x = 528; //wscreen * 8 / 12;
          break;
      case 2:                  // <
          y = 0;
          x = 594; //wscreen * 9 / 12;
          break;
      case 3:                  // >
          y = 0;
          x = 660; // wscreen * 10 / 12;
          break;
      case 4:                  // -
          y = 0;
          x = 726; // wscreen * 11 / 12;
          break;
      case 5:                  // Clear
          y = 0;
          x = 0;
          w = 131; // 2 * wscreen/12;
          break;
      case 6:                 // Left Shift
          y = hscreen/8;
          x = 0;
          break;
      case 7:                 // Right Shift
          y = hscreen/8;
          x = 726; // wscreen * 11 / 12;
          break;
      case 8:                 // Enter
          y = 2 * hscreen/8;
          x = 660; // wscreen * 10 / 12;
          w = 131; // 2 * wscreen/12;
          h = 119;
          break;
      case 9:                 // Backspace
          y = 4 * hscreen / 8;
          x = 693; // wscreen * 21 / 24;
          w = 98; // 3 * wscreen/24;
          break;
      }
    }
    if ((ButtonPosition >= 10) && (ButtonPosition <= 19))  // ZXCVBNM,./
    {
      y = hscreen/8;
      x = (ButtonPosition - 9) * 66;
    }
    if ((ButtonPosition >= 20) && (ButtonPosition <= 29))  // ASDFGHJKL
    {
      y = 2 * hscreen / 8;
      x = ((ButtonPosition - 19) * 66) - 33;
    }
    if ((ButtonPosition >= 30) && (ButtonPosition <= 39))  // QWERTYUIOP
    {
      y = 3 * hscreen / 8;
      x = (ButtonPosition - 30) * 66;
    }
    if ((ButtonPosition >= 40) && (ButtonPosition <= 49))  // 1234567890
    {
      y = 4 * hscreen / 8;
      x = ((ButtonPosition - 39) * 66) - 33;
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

int AddButtonStatus(int ButtonIndex, char *Text, color_t *Color)
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
  char label[255];
  char line1[15];
  char line2[15];

  // Look up the label
  strcpy(label, Button->Status[Button->NoStatus].Text);

  // Draw the basic button
  rectangle(Button->x, Button->y + 1, Button->w, Button->h, 
    Button->Status[Button->NoStatus].Color.r,
    Button->Status[Button->NoStatus].Color.g,
    Button->Status[Button->NoStatus].Color.b);

  // Set text and background colours
  setForeColour(255, 255, 255);				   // White text
  setBackColour(Button->Status[Button->NoStatus].Color.r,
                Button->Status[Button->NoStatus].Color.g,
                Button->Status[Button->NoStatus].Color.b);



  // Separate button text into 2 lines if required  
  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(label, find);            // pointer to ^ in string

  if((ptr) && (CurrentMenu != 12))                  // if ^ found then
  {                                                 // 2 lines
    int index = ptr - label;                        // Position of ^ in string
    snprintf(line1, index+1, label);                // get text before ^
    snprintf(line2, strlen(label) - index, label + index + 1);  // and after ^

    // Display the text on the button
    TextMid2(Button->x+Button->w/2, Button->y+Button->h*11/16, line1, &font_dejavu_sans_20);	
    TextMid2(Button->x+Button->w/2, Button->y+Button->h*3/16, line2, &font_dejavu_sans_20);	
  
    // Draw green overlay half-button.  Menus 1 or 4, 2 lines and Button status = 0 only
    if (((CurrentMenu == 1) || (CurrentMenu == 4)) && (Button->NoStatus == 0))
    {
      // Draw the green box
      rectangle(Button->x, Button->y + 1, Button->w, Button->h/2,
        Button->Status[1].Color.r,
        Button->Status[1].Color.g,
        Button->Status[1].Color.b);

      // Set the background colour and then display the text
      setBackColour(Button->Status[1].Color.r,
                    Button->Status[1].Color.g,
                    Button->Status[1].Color.b);
      TextMid2(Button->x+Button->w/2, Button->y+Button->h*3/16, line2, &font_dejavu_sans_20);	
    }
  }
  else                                              // One line only
  {
    if ((CurrentMenu <= 9) && (CurrentMenu != 4))
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_28);
    }
    else if (CurrentMenu == 41)  // Keyboard
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2 - hscreen / 64, label, &font_dejavu_sans_28);
    }
    else // fix text size at 20
    {
      TextMid2(Button->x+Button->w/2, Button->y+Button->h/2, label, &font_dejavu_sans_20);
    }
  }
}

void SetButtonStatus(int ButtonIndex, int Status)
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

int getTouchScreenDetails(int *screenXmin, int *screenXmax, int *screenYmin, int *screenYmax)
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

int getTouchSample(int *rawX, int *rawY, int *rawPressure)
{
  int i;       // how many bytes were read
  size_t rb;   // the events (up to 64 at once)
  struct input_event ev[64];
  //static int Last_event=0; //not used?
  rb=read(fd,ev,sizeof(struct input_event)*64);
  *rawX=-1;*rawY=-1;
  int StartTouch=0;

  for (i = 0;  i <  (rb / sizeof(struct input_event)); i++)
  {
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
    else if (ev[i].type == EV_ABS && ev[i].code == 0 && ev[i].value > 0)
    {
      //printf("Event type is %s%s%s & Event code is %sX(0)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
      *rawX = ev[i].value;
    }
    else if (ev[i].type == EV_ABS  && ev[i].code == 1 && ev[i].value > 0)
    {
      //printf("Event type is %s%s%s & Event code is %sY(1)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
      *rawY = ev[i].value;
    }
    else if (ev[i].type == EV_ABS  && ev[i].code == 24 && ev[i].value > 0)
    {
      //printf("Event type is %s%s%s & Event code is %sPressure(24)%s & Event value is %s%d%s\n", KYEL,events[ev[i].type],KWHT,KYEL,KWHT,KYEL,ev[i].value,KWHT);
      *rawPressure = ev[i].value;
    }
    if((*rawX!=-1)&&(*rawY!=-1)&&(StartTouch==1))
    {
      return 1;
    }

  }
  return 0;
}

void ShowFreq(uint64_t DisplayFreq)
{
  // Displays the current frequency with leading zeros blanked
  const font_t *font_ptr = &font_dejavu_sans_72;

  float vpos = 0.49;
  uint64_t RemFreq;
  uint64_t df01, df02, df03, df04, df05, df06, df07, df08, df09, df10, df11;
  char df01text[16], df02text[16], df03text[16], df04text[16] ,df05text[16];
  char df06text[16], df07text[16], df08text[16], df09text[16] ,df10text[16];
  char df11text[16];

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
    Text3(0.06*wscreen, vpos*hscreen, df01text, font_ptr);
  }
  if (DisplayFreq >= 1000000000)
  {
    Text3(0.14*wscreen, vpos*hscreen, df02text, font_ptr);
    Text3(0.20*wscreen, vpos*hscreen, ",", font_ptr);
  }
  if (DisplayFreq >= 100000000)
  {
    Text3(0.24*wscreen, vpos*hscreen, df03text, font_ptr);
  }
  if (DisplayFreq >= 10000000)
  {
    Text3(0.32*wscreen, vpos*hscreen, df04text, font_ptr);
  }
  if (DisplayFreq >= 1000000)
  {
    Text3(0.40*wscreen, vpos*hscreen, df05text, font_ptr);
    Text3(0.458*wscreen, vpos*hscreen, ".", font_ptr);
  }
  if (DisplayFreq >= 100000)
  {
    Text3(0.495*wscreen, vpos*hscreen, df06text, font_ptr);
  }
  if (DisplayFreq >= 10000)
  {
    Text3(0.575*wscreen, vpos*hscreen, df07text, font_ptr);
  }
  if (DisplayFreq >= 1000)
  {
    Text3(0.655*wscreen, vpos*hscreen, df08text, font_ptr);
    Text3(0.715*wscreen, vpos*hscreen, ",", font_ptr);
  }
  if (DisplayFreq >= 100)
  {
    Text3(0.75*wscreen, vpos*hscreen, df09text, font_ptr);
  }
  Text3(0.83*wscreen, vpos*hscreen, df10text, font_ptr);
  Text3(0.91*wscreen, vpos*hscreen, df11text, font_ptr);
}

void ShowLevel(int DisplayLevel)
{
  // DisplayLevel is a signed integer in the range +999 to - 999 tenths of dBm
  const font_t *font_ptr = &font_dejavu_sans_72;
  float vpos;
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
    vpos = 0.14;
    Text3(0.02*wscreen, vpos*hscreen, dl01text, font_ptr);
    if ((DisplayLevel <= -100) || (DisplayLevel >= 100))
    {
      Text3(0.11*wscreen, vpos*hscreen, dl02text, font_ptr);
    }
    Text3(0.19*wscreen, vpos*hscreen, dl03text, font_ptr);
    Text3(0.25*wscreen, vpos*hscreen, ".", font_ptr);
    Text3(0.29*wscreen, vpos*hscreen, dl04text, font_ptr);
    Text3(0.37*wscreen, vpos*hscreen, "dBm", font_ptr);
}


void AdjustFreq(int button)
{
  char ExpressCommand[255];
  char FreqText[255];
  button = button - 8;
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
    if (strcmp(osc, "express") == 0)
    {
      strcpy(ExpressCommand, "echo \"set freq ");
      snprintf(FreqText, 12, "%lld", DisplayFreq);
      strcat(ExpressCommand, FreqText);
      strcat(ExpressCommand, "\" >> /tmp/expctrl" );
      system(ExpressCommand);
    }

    if (strcmp(osc, "pluto") == 0)
    {
      plutotx(false);
    }

    if (strcmp(osc, "adf4351") == 0)
    {
      adf4351On(level); // change adf freq at set level
    }

    if (strcmp(osc, "elcom")==0)
    {
      ElcomOn(); // Change Elcom Freq
    }
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
  //system(AttenCmd);  
}


void AdjustLevel(int Button)
{
  char ExpressCommand[255];
  char LevelText[255];

  // Deal with DATV Express Levels
  if (strcmp(osc, "express")==0)
  {
    if (Button == 0)  // decrement level by 10
    {
      level = level - 10;
    }
    if (Button == 1)  // decrement level by 1
    {
      level = level - 1;
    }
    if (Button == 5)  // increment level by 10
    {
      level = level + 10;
    }
    if (Button == 6)  // increment level by 1
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
  else if (strcmp(osc, "pluto") == 0)
  {
    if (Button == 0)  // decrement level by 10
    {
      level = level - 10;
    }
    if (Button == 1)  // decrement level by 1
    {
      level = level - 1;
    }
    if (Button == 5)  // increment level by 10
    {
      level = level + 10;
    }
    if (Button == 6)  // increment level by 1
    {
      level = level + 1;
    }
    if (level > 10 )
    {
      level = 10;
    }
    if (level < -69 )
    {
      level = -69;
    }
    if (OutputStatus == 1)  // Change on Pluto
    {
      plutotx(false);
    }
  }

  else
  {
    if (AttenIn == 0) // No attenuator
    {
      if (strcmp(osc, "adf4351")==0)
      {
        if (Button == 1)  // decrement level
        {
          level = level - 1;
          if (level < 0 )
          {
            level = 0;
          }
        }
        if (Button == 6)  // increment level
        {
          level = level + 1;
          if (level > 3 )
          {
            level = 3;
          }
        }
        adf4351On(level); // change adf level
      }
      if (strcmp(osc, "adf5355")==0)
      {
        ;  // adf5355 behaviour tbd
      }
    }
    else                         // With attenuator
    {
      if ((strcmp(osc, "adf4351")==0) || (strcmp(osc, "adf5355")==0))
      // set adf4351 or adf5355 level
      {
        level = 3;
      }
      if (strcmp(osc, "adf4351") == 0)
      // adf4351 attenuator behaviour here
      {
        if (Button == 0)  // decrement level by 10 dB
        {
          atten = atten + 10.0;
        }
        if (Button == 1)  // decrement level by 1 dB
        {
          atten = atten + 1.0;
        }
        if (Button == 2)  // decrement level by .25 or .5 dB
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
        if (Button == 5)  // increment level by 10 dB
        {
          atten = atten - 10.0;
        }
        if (Button == 5)  // increment level by 1 dB
        {
          atten = atten - 1.0;
        }
        if (Button == 5)  // increment level by .25 or .5 dB
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
    n = n + 1;
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

  if (strcmp(osc, "adf4351")==0)
  {
    DisplayLevel = DisplayLevel + 30 * level;
  }

  if (strcmp(osc, "express")==0)
  {
    DisplayLevel = DisplayLevel + 10 * level;  // assumes 1 dB steps  Could use look-up table for more accuracy
  }

  if (strcmp(osc, "pluto")==0)
  {
    DisplayLevel = DisplayLevel + 10 * (level);  // assumes 1 dB steps  Could use look-up table for more accuracy
  }

  if (strcmp(osc, "adf5355")==0)
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
  if ((ModOn == 1) && (strcmp(osc, "express")==0))
  {
    DisplayLevel = DisplayLevel - 2;   // Correction for DATV Express mod power (-0.2 dB)
  }
}


void ShowAtten()
{
  // Display the attenuator setting, Pluto or DATV Express Level on Menu 12
  char LevelText[255];
  char AttenSet[255];
  const font_t *font_ptr = &font_dejavu_sans_24;
  float vpos = 0.04;
  float hpos = 0.39;
  
  if ((strcmp(osc, "express") != 0) && (strcmp(osc, "pluto") != 0) && (AttenIn == 1))  // do Attenuator text first
  {
    strcpy(LevelText, "Attenuator Set to -");
    snprintf(AttenSet, 7, " %.2f", atten);
    strcat(LevelText, AttenSet);
    strcat(LevelText, " dB");
    Text2(hpos*wscreen, vpos*hscreen, LevelText, font_ptr);
  }
  if (strcmp(osc, "adf4351") == 0)                // adf4351 only Text
  {
    strcpy(LevelText, "ADF4351 Level = ");
    snprintf(AttenSet, 3, "%d", level);
    strcat(LevelText, AttenSet);
    Text2(hpos*wscreen, vpos*hscreen, LevelText, font_ptr);
  }

  if (strcmp(osc, "express") == 0)                // DATV Express Text
  {
    strcpy(LevelText, "Express Level = ");
    snprintf(AttenSet, 3, "%d", level);
    strcat(LevelText, AttenSet);
    Text2(hpos*wscreen, vpos*hscreen, LevelText, font_ptr);
  }
  if (strcmp(osc, "pluto") == 0)                // Pluto Text
  {
    strcpy(LevelText, "Pluto Level = ");
    snprintf(AttenSet, 4, "%d", level);
    strcat(LevelText, AttenSet);
    Text2(hpos*wscreen, vpos*hscreen, LevelText, font_ptr);
  }
}


void ShowOPFreq()
{
  // Display the calculated output freq
  char OPFreqText[255];
  char FreqString[255];
  const font_t *font_ptr = &font_dejavu_sans_24;
  float vpos = 0.31;
  float hpos = 0.39;
  
  if ((strcmp(osc, "elcom") == 0) && (CurrentMenu == 11))
  {
      strcpy(OPFreqText, "Output Freq = ");
      snprintf(FreqString, 12, "%lld", OutputFreq);
      strcat(OPFreqText, FreqString);
      Text2(hpos*wscreen, vpos*hscreen, OPFreqText, font_ptr);
  }
}


void stderrandexit(const char *msg, int errcode, int line)
{
  char line1[63];
  char line2[63];

  if(errcode < 0)
  {
    snprintf(line1, 63, "Pluto Error:%d,", errcode);
    snprintf(line2, 63, "program terminated (line:%d)", line);
    MsgBox4(line1, line2, " ", "Touch Screen to Restart");
  }
  else
  {
    snprintf(line1, 63, "%s,", msg);
    snprintf(line2, 63, "program terminated (line:%d)", line);
    MsgBox4(line1, line2, "Pluto error", "Touch Screen to Restart");
  }
  wait_touch();

  setBackColour(0, 0, 0);  // Now do a clean exit without OscStop (which would fail)
  clearScreen();
  printf("Restarting SigGen after Pluto Error\n");
  char Commnd[255];
  sprintf(Commnd, "stty echo");
  system(Commnd);
  exit(130);
}

void CWOnOff(int onoff)
{
  int rc;

  if((rc = iio_channel_attr_write_bool(tx0_i, "raw", onoff)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }
  if((rc = iio_channel_attr_write_bool(tx0_q, "raw", onoff)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }
}

int plutotx(bool cal)
{
  const char *value;
  long long freq;
  double dBm;
  int rc;
  char URIPLUTO[20] = "ip:";
  
  freq = DisplayFreq;
  dBm = level;
  strcat(URIPLUTO, PlutoIP);

  ctx = iio_create_context_from_uri(URIPLUTO);

  if(ctx == NULL)
  {
    stderrandexit("Connection failed", 0, __LINE__);
  }

  if((value = iio_context_get_attr_value(ctx, "ad9361-phy,model")) != NULL)
  {
    if(strcmp(value, "ad9364"))
    {
      stderrandexit("Pluto is not expanded", 0, __LINE__);
    }
  }
  else
  {
    stderrandexit("Error retrieving phy model", 0, __LINE__);
  }

  phy = iio_context_find_device(ctx, "ad9361-phy");
  dds_core_lpc = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");  
  tx0_i = iio_device_find_channel(dds_core_lpc, "altvoltage0", true);
  tx0_q = iio_device_find_channel(dds_core_lpc, "altvoltage2", true);
  tx1_i = iio_device_find_channel(dds_core_lpc, "altvoltage1", true);
  tx1_q = iio_device_find_channel(dds_core_lpc, "altvoltage3", true);
  tx_chain=iio_device_find_channel(phy, "voltage0", true);
  tx_lo=iio_device_find_channel(phy, "altvoltage1", true);

  if(!phy || !dds_core_lpc || !tx0_i || !tx0_q || !tx_chain || !tx_lo)
  {
    stderrandexit("Error finding device or channel", 0, __LINE__);
  }

  //enable internal TX local oscillator
  if((rc = iio_channel_attr_write_bool(tx_lo, "external", false)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  //disable fastlock feature of TX local oscillator
  if((rc = iio_channel_attr_write_bool(tx_lo, "fastlock_store", false)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  //power on TX local oscillator
  if((rc = iio_channel_attr_write_bool(tx_lo, "powerdown", false)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  //full duplex mode
  if((rc = iio_device_attr_write(phy, "ensm_mode", "fdd")) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  if (cal)
  {
    //calibration mode to auto
    if((rc = iio_device_attr_write(phy, "calib_mode", "auto")) < 0)
    {
      stderrandexit(NULL, rc, __LINE__);
    }
  }
  else
  {
    //calibration mode to manual
    if((rc = iio_device_attr_write(phy, "calib_mode", "manual")) < 0)
    {
      stderrandexit(NULL, rc, __LINE__);
    }
  }

  // Set the hardware gain
  if((rc = iio_channel_attr_write_double(tx_chain, "hardwaregain", dBm - REFTXPWR)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the TX bandwidth (to 4 MHz)
  if((rc = iio_channel_attr_write_longlong(tx_chain, "rf_bandwidth", FBANDWIDTH)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  if (cal)
  {
    // Set the sampling frequency (to 4 MHz), produces spike and is non-volatile so only do for Cal
    if((rc = iio_channel_attr_write_longlong(tx_chain, "sampling_frequency", FSAMPLING) ) < 0)
    {
      stderrandexit(NULL, rc, __LINE__);
    }
  }

  // Set the I channel 0 DDS scale  0.8 reduces the LSB 3rd harmonic (was 1.0)
  if((rc = iio_channel_attr_write_double(tx0_i, "scale", 0.8)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the Q channel 0 DDS scale
  if((rc = iio_channel_attr_write_double(tx0_q, "scale", 0.8)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Disable the DDSs on Channel 1
  if((rc = iio_channel_attr_write_double(tx1_i, "scale", 0.0)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }
  if((rc = iio_channel_attr_write_double(tx1_q, "scale", 0.0)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the I channel stimulus frequency (1 MHz)
  if((rc = iio_channel_attr_write_longlong(tx0_i, "frequency", FCW)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the Q channel stimulus frequency (1 MHz)
  if((rc = iio_channel_attr_write_longlong(tx0_q, "frequency", FCW)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the I channel phase to 90 degrees for USB
  if((rc = iio_channel_attr_write_longlong(tx0_i, "phase", 90000)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the Q channel phase to 0 degrees for USB
  if((rc = iio_channel_attr_write_longlong(tx0_q, "phase", 0)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  // Set the LO frequncy for USB operation
  if((rc = iio_channel_attr_write_longlong(tx_lo, "frequency", freq - FCW)) < 0)
  {
   stderrandexit(NULL, rc, __LINE__);
  }

  CWOnOff(1); //Turn the output on

  return 0;
}


int PlutoOff()
{
  const char *value;
  int rc;
  char URIPLUTO[20] = "ip:";
  strcat(URIPLUTO, PlutoIP);

  ctx = iio_create_context_from_uri(URIPLUTO);

  if(ctx==NULL)
  {
    stderrandexit("Connection failed", 0, __LINE__);
  }

  if((value = iio_context_get_attr_value(ctx, "ad9361-phy,model"))!=NULL)
  {
    if(strcmp(value,"ad9364"))
    {
      stderrandexit("Pluto is not expanded",0,__LINE__);
    }
  }
  else
  {
    stderrandexit("Error retrieving phy model",0,__LINE__);
  }

  phy = iio_context_find_device(ctx, "ad9361-phy");
  dds_core_lpc = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");  
  tx0_i = iio_device_find_channel(dds_core_lpc, "altvoltage0", true);
  tx0_q = iio_device_find_channel(dds_core_lpc, "altvoltage2", true);
  tx_chain=iio_device_find_channel(phy, "voltage0", true);
  tx_lo=iio_device_find_channel(phy, "altvoltage1", true);

  if(!phy || !dds_core_lpc || !tx0_i || !tx0_q || !tx_chain || !tx_lo)
  {
    stderrandexit("Error finding device or channel", 0, __LINE__);
  }

  //power off TX local oscillator
  if((rc = iio_channel_attr_write_bool(tx_lo, "powerdown", true)) < 0)
  {
    stderrandexit(NULL, rc, __LINE__);
  }

  CWOnOff(0);  // Make sure I and Q channels are off

  iio_context_destroy(ctx);

  return 0;
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

  strcpy(ExpressCommand, "/home/pi/rpidatv/bin/avc2ts -b 372783 -m 537044 ");
  strcat(ExpressCommand, "-x 720 -y 576 -f 25 -i 100 -o videots -t 3 -p 255 -s SigGen &");
  system(ExpressCommand);
}

void adf4351On(int adflevel)
{
  char transfer[255];
  char Startadf4351[255] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  double freqmhz;
  freqmhz=(double)DisplayFreq/1000000;
  snprintf(transfer, 13, "%.6f", freqmhz);
  strcat(Startadf4351, transfer);
  strcat(Startadf4351, " ");
  strcat(Startadf4351, ref_freq_4351);
  snprintf(transfer, 3, " %d", adflevel);
  strcat(Startadf4351, transfer);                    // Level from input parameter
  printf("Starting ADF4351 output %s\n", Startadf4351);
  system(Startadf4351);
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
  printf("Output Freq * 3 = %lld\n", OutputFreq);
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
  if (strcmp(osc, "express")==0)
  {
    printf("Starting DATV Express\n");
    //MsgBox4("Please wait","Loading Firmware to DATV Express", "", "");
    //system("/home/pi/rpidatv/src/siggen/startexpresssvr.sh");
    n = StartExpressServer();
    printf("Response from StartExpressServer was %d\n", n);
  }
  else
  {
    strcpy(KillExpressSvr, "echo \"set kill\" >> /tmp/expctrl");
    system(KillExpressSvr);
  }
  
  // Turn off attenuator if not compatible with mode
  if ((strcmp(osc, "pluto") == 0) || (strcmp(osc, "elcom") == 0)
    ||(strcmp(osc, "express") == 0) || (strcmp(osc, "lime") == 0))
  {
    AttenIn = 0;
    SetAtten(0);
  }

  // Turn off modulation if not compatible with mode
  if ((strcmp(osc, "pluto") == 0) || (strcmp(osc, "elcom") == 0)
    || (strcmp(osc, "adf4351") == 0) || (strcmp(osc, "adf5355") == 0))
  {
    ModOn = 0;
  }

  // Set adf4351 level correctly for attenuator
  if ((strcmp(osc, "adf4351") == 0) && (AttenIn == 1))
  {
    level = 3;
  }

  // Read in amplitude Cal table
  strcpy(Param, osc);
  strcat(Param, "points");
  GetConfigParam(PATH_CAL, Param, Value);
  CalPoints = atoi(Value);
  //printf("CalPoints= %d \n", CalPoints);
  for ( n = 1; n <= CalPoints; n = n + 1 )
  {
    snprintf(PointNumber, 4, "%d", n);

    strcpy(Param, osc);
    strcat(Param, "freq");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL, Param, Value);
    CalFreq[n] = strtoull(Value, 0, 0);
    //printf(" %lld \n", CalFreq[n]);

    strcpy(Param, osc);
    strcat(Param, "lev");
    strcat(Param, PointNumber);
    GetConfigParam(PATH_CAL, Param, Value);
    CalLevel[n] = atoi(Value);
    //printf("CalLevel= %d \n", CalLevel[n]);
  }

  // Hide unused buttons
  // First make them all visible
  for (n = 0; n < 3; n = n + 1)
  {
    SetButtonStatus(ButtonNumber(11, n), 0);         // Show all level decrement
    SetButtonStatus(ButtonNumber(11, n + 5), 0);     // Show all level increment
  }
  for (n = 8; n < 30; n = n + 1)
  {
    SetButtonStatus(ButtonNumber(11, n), 0);         // Show all freq inc/decrement
  }

  // Hide the unused frequency increment/decrement buttons
  if ((strcmp(osc, "adf4351") == 0) || (strcmp(osc, "lime") == 0) 
    || (strcmp(osc, "pluto") == 0) || (strcmp(osc, "express") == 0))
  {
    SetButtonStatus(ButtonNumber(11, 8), 1);         //hide frequency decrement above 9.99 GHz
    SetButtonStatus(ButtonNumber(11, 19), 1);        //hide frequency increment above 9.99 GHz
  }

  if (strcmp(osc, "elcom") == 0)
  {
    for (n = 13; n < 19; n = n + 1)
    {
      SetButtonStatus(ButtonNumber(11, n), 1);         // Hide all freq decrement < 1 MHz
      SetButtonStatus(ButtonNumber(11, n + 11), 1);    // Hide all freq increment < 1 MHz
    }
  }

  // Hide the unused level buttons
  if (AttenIn == 0)
  {
    if ((strcmp(osc, "adf4351") == 0) || (strcmp(osc, "adf5355") == 0))
    {
      SetButtonStatus(ButtonNumber(11, 0), 1);         // Hide decrement 10s
      SetButtonStatus(ButtonNumber(11, 2), 1);         // Hide decrement 10ths
      SetButtonStatus(ButtonNumber(11, 5), 1);         // Hide increment 10s
      SetButtonStatus(ButtonNumber(11, 7), 1);         // Hide increment 10ths
    }
    if (strcmp(osc, "elcom") == 0)
    {
      SetButtonStatus(ButtonNumber(11, 0), 1);         // Hide decrement 10s
      SetButtonStatus(ButtonNumber(11, 1), 1);         // Hide decrement 1s
      SetButtonStatus(ButtonNumber(11, 2), 1);         // Hide decrement 10ths
      SetButtonStatus(ButtonNumber(11, 5), 1);         // Hide increment 10s
      SetButtonStatus(ButtonNumber(11, 6), 1);         // Hide decrement 1s
      SetButtonStatus(ButtonNumber(11, 7), 1);         // Hide increment 10ths
    }
    if ((strcmp(osc, "express") == 0) || (strcmp(osc, "pluto") == 0))
    {
      SetButtonStatus(ButtonNumber(11, 2), 1);         // Hide decrement 10ths
      SetButtonStatus(ButtonNumber(11, 7), 1);         // Hide increment 10ths
    }
  }
  CalcOPLevel();
}


void ShowTitle()
{
  // Set the colours
  if (CurrentMenu == 1)               // Main menu
  {
    setForeColour(0, 0, 0);          // Black text
    setBackColour(255, 255, 255);    // on White
  }
  else                               // All others
  {
    setForeColour(255, 255, 255);    // White text
    setBackColour(0, 0, 0);          // on Black
  }

  const font_t *font_ptr = &font_dejavu_sans_28;
  int txtht =  font_ptr->ascent;
  int linepitch = (12 * txtht) / 10;
  int linenumber = 1;

  // Display Text
  TextMid2(wscreen / 2.0, hscreen - linenumber * linepitch, MenuTitle[CurrentMenu], &font_dejavu_sans_28);
}

void UpdateWindow()
// Paint each defined button and the title on the current Menu
{
  int i;
  int first;
  int last;

  // Set the background colour
  if (CurrentMenu == 1)           // Main Menu White
  {
    setBackColour(255, 255, 255);
  }
  else                            // All others Black
  {
    setBackColour(0, 0, 0);
  }
  
  if (CurrentMenu != 12)  // If not the keyboard
  {
    clearScreen();
  }
  // Draw the backgrounds for the smaller menus
  if ((CurrentMenu >= 2) && (CurrentMenu <= 4))  // 10 button menus
  {
    rectangle(10, 12, wscreen - 18, hscreen * 2 / 6 + 12, 127, 127, 127);
  }

  // Draw each button in turn
  first = ButtonNumber(CurrentMenu, 0);
  last = ButtonNumber(CurrentMenu + 1 , 0) - 1;

  for(i = first; i <= last; i++)
  {
    if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
    {
      DrawButton(i);                     // Draw the button
    }
  }

  // Show the title and any required text
  ShowTitle();
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


void SelectOsc(int NoButton)      // Output Oscillator
{
  // Stop or reset DATV Express Server if required
  if (strcmp(osc, "express") == 1)  // mode was DATV Express
  {
    system("sudo killall express_server >/dev/null 2>/dev/null");
    system("sudo rm /tmp/expctrl >/dev/null 2>/dev/null");
  }

  SelectInGroupOnMenu(CurrentMenu, 5, 9, NoButton, 1);
  SelectInGroupOnMenu(CurrentMenu, 0, 3, NoButton, 1);

  switch(NoButton)
  {
  case 0:
    strcpy(osc, "pluto");
    strcpy(osc_text, "Pluto");  
    break;
  case 1:
    // Start DATV Express if required
    StartExpressServer();
    strcpy(osc, "express");
    strcpy(osc_text, "DATV Express");  
    break;
  case 2:
    strcpy(osc, "elcom");
    strcpy(osc_text, "Elcom");  
    break;
  case 3:
    CheckLimeReady();
    strcpy(osc, "lime");
    strcpy(osc_text, "Lime Mini");  
    break;
  case 5:
    strcpy(osc, "adf4351");
    strcpy(osc_text, "ADF4351");  
    break;
  case 6:
    strcpy(osc, "adf5355");
    strcpy(osc_text, "ADF5355");  
    break;
  }
  SetConfigParam(PATH_SGCONFIG, "osc", osc);
  InitOsc();
}


void ImposeBounds()  // Constrain DisplayFreq and level to physical limits
{
  if (strcmp(osc, "pluto")==0)
  {
    SourceUpperFreq = 6000000000;
    SourceLowerFreq =   50000000;
    strcpy(osc_text, "Pluto");  
    if (level > 10)
    {
      level = 10;
    }
    if (level < -69)
    {
      level = -69;
    }
  }

  if (strcmp(osc, "express")==0)
  {
    SourceUpperFreq = 2450000000;
    SourceLowerFreq =   70000000;
    strcpy(osc_text, "DATV Express");  
    if (level > 47)
    {
      level = 47;
    }
    if (level < 0)
    {
      level = 0;
    }
  }

  if (strcmp(osc, "elcom")==0)
  {
    strcpy(osc_text, "Elcom"); 
    SourceUpperFreq = 14000000000;
    SourceLowerFreq = 10000000000;
  }

  if (strcmp(osc, "lime")==0)
  {
    strcpy(osc_text, "Lime Mini"); 
    SourceUpperFreq = 3500000000;
    SourceLowerFreq =   70000000;
  }

  if (strcmp(osc, "adf4351")==0)
  {
    SourceUpperFreq = 4400000000;
    SourceLowerFreq =   35000000;
    strcpy(osc_text, "ADF4351");
    if (level > 3)
    {
      level = 3;
    }
    if (level < 0)
    {
      level = 0;
    }
  }

  if (strcmp(osc, "adf5355")==0)
  {
    SourceUpperFreq = 13600000000;
    SourceLowerFreq = 54000000;
    strcpy(osc_text, "ADF5355"); 
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
  char Startadf4351[255] = "sudo /home/pi/rpidatv/bin/adf4351 ";
  char transfer[255];
  double freqmhz;
  int adf4351_lev = level; // 0 to 3

  if (strcmp(osc, "adf4351") == 0)
  {
    freqmhz = (double) DisplayFreq / 1000000;
    printf("Demanded Frequency = %.6f\n", freqmhz);
    snprintf(transfer, 13, "%.6f", freqmhz);
    strcat(Startadf4351, transfer);
    strcat(Startadf4351, " ");
    strcat(Startadf4351, ref_freq_4351);
    strcat(Startadf4351, " ");
    snprintf(transfer, 2, "%d", adf4351_lev);
    strcat(Startadf4351, transfer);
    printf(Startadf4351);
    printf("\nStarting ADF4351 Output\n");
    system(Startadf4351);
  }

  if (strcmp(osc, "express")==0)
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

  if (strcmp(osc, "pluto")==0)
  {
    plutotx(false);
  }

  if (strcmp(osc, "elcom")==0)
  {
    printf("\nStarting Elcom Output\n");
    ElcomOn();
  }
  SetButtonStatus(ButtonNumber(11, 30), 1);
  OutputStatus = 1;
}


void OscStop()
{
  char expressrx[255];
  printf("Oscillator Stop\n");

  if (strcmp(osc, "adf4351") == 0)
  {
    system("sudo /home/pi/rpidatv/bin/adf4351 off");    
    printf("\nStopping adf4351 output\n");
  }

  if ((strcmp(osc, "elcom") == 0) && (CurrentMenu == 11))
  {
    MsgBox4("Elcom oscillator will keep running", "until powered off", " ", "Touch screen to continue");
    wait_touch();
  }

  if (strcmp(osc, "express") == 0)
  {
    strcpy( expressrx, "echo \"set ptt rx\" >> /tmp/expctrl" );
    system(expressrx);
    //strcpy( expressrx, "echo \"set car off\" >> /tmp/expctrl" );
    //system(expressrx);
    system("sudo killall netcat >/dev/null 2>/dev/null");
    printf("\nStopping Express output\n");
  }

  if (strcmp(osc, "pluto") == 0)
  {
    PlutoOff();
    printf("\nStopping Pluto output\n");
  }

  if (strcmp(osc, "adf5355")==0)
  {
    printf("\nStopping ADF5355 output\n");
  }

  // Kill the key processes as nicely as possible

  system("sudo killall ffmpeg >/dev/null 2>/dev/null");
  system("sudo killall tcanim >/dev/null 2>/dev/null");
  system("sudo killall avc2ts >/dev/null 2>/dev/null");
  system("sudo killall netcat >/dev/null 2>/dev/null");

  // Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  usleep(1000);
  system("sudo killall -9 avc2ts >/dev/null 2>/dev/null");
  SetButtonStatus(ButtonNumber(11, 30), 0);
  OutputStatus = 0;
}


void SelectAtten(int NoButton)  // Attenuator Type
{
  SelectInGroupOnMenu(CurrentMenu, 5, 8, NoButton, 1);
  switch(NoButton)
  {
    case 5:
    strcpy(AttenType, "NONE");
    break;
    case 6:
    strcpy(AttenType, "PE4312");
    break;
    case 7:
    strcpy(AttenType, "PE43713");
    break;
    case 8:
    strcpy(AttenType, "HMC1119");
    break;
  }
  SetConfigParam(PATH_SGCONFIG, "attenuator", AttenType);
}


void *WaitButtonEvent(void * arg)
{
  int rawX, rawY, rawPressure;
  while(getTouchSample(&rawX, &rawY, &rawPressure)==0);
  FinishedButton=1;
  return NULL;
}


void chopN(char *str, size_t n)
{
  size_t len = strlen(str);
  if ((n > len) || (n == 0))
  {
    return;
  }
  else
  {
    memmove(str, str+n, len - n + 1);
  }
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


void MsgBox(char *message)
{
  // Display a one-line message and wait for touch
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;

  clearScreen();
  TextMid2(wscreen / 2, hscreen /2, message, font_ptr);
  TextMid2(wscreen / 2, 20, "Touch Screen to Continue", font_ptr);

  printf("MsgBox called and waiting for touch\n");
}


void MsgBox2(char *message1, char *message2)
{
  // Display a 2-line message and wait for touch
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  clearScreen();
  TextMid2(wscreen / 2, hscreen / 2 + linepitch, message1, font_ptr);
  TextMid2(wscreen / 2, hscreen / 2 - linepitch, message2, font_ptr);
  TextMid2(wscreen / 2, 20, "Touch Screen to Continue", font_ptr);

  printf("MsgBox2 called and waiting for touch\n");
}


void MsgBox4(char *message1, char *message2, char *message3, char *message4)
{
  // Display a 4-line message
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  const font_t *font_ptr = &font_dejavu_sans_32;
  int txtht =  font_ptr->ascent;
  int linepitch = (14 * txtht) / 10;

  clearScreen();
  TextMid2(wscreen / 2, hscreen - (linepitch * 2), message1, font_ptr);
  TextMid2(wscreen / 2, hscreen - 2 * (linepitch * 2), message2, font_ptr);
  TextMid2(wscreen / 2, hscreen - 3 * (linepitch * 2), message3, font_ptr);
  TextMid2(wscreen / 2, hscreen - 4 * (linepitch * 2), message4, font_ptr);

  // printf("MsgBox4 called\n");
}


static void cleanexit(int exit_code)
{
  OscStop();
  setBackColour(0, 0, 0);
  clearScreen();
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
  bool refreshed;
  
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

  // On initial call set Menu to 12
  CurrentMenu = 12;
  int rawX, rawY, rawPressure;
  refreshed = false;

  // Set up text
  setForeColour(255, 255, 255);    // White text
  setBackColour(0, 0, 0);          // on Black
  clearScreen();
  const font_t *font_ptr = &font_dejavu_sans_28;

  // Look up pitch for W (87), the widest caharcter
  int charPitch = font_ptr->characters[87].render_width;

  for (;;)
  {
    if (!refreshed)
    {
      // Sort Shift changes
      ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
      for (i = ButtonNumber(CurrentMenu, 0); i <= ButtonNumber(CurrentMenu, 49); i = i + 1)
      {
        if (ButtonArray[i].IndexStatus > 0)  // If button needs to be drawn
        {
          SetButtonStatus(i, ShiftStatus);
        }
      }  

      // Display the keyboard here as it would overwrite the text later
      UpdateWindow();

      // Display Instruction Text
      setForeColour(255, 255, 255);    // White text
      setBackColour(0, 0, 0);          // on Black
      Text2(10, 420 , RequestText, font_ptr);

      // Display Text for Editing
      for (i = 1; i <= strlen(EditText); i = i + 1)
      {
        strncpy(thischar, &EditText[i-1], 1);
        thischar[1] = '\0';
        if (i > MaxLength)
        {
          setForeColour(255, 63, 63);    // Red text
        }
        else
        {
          setForeColour(255, 255, 255);    // White text
        }
        TextMid2(i * charPitch, 330, thischar, font_ptr);
      }

      // Cover deleted text with background colour
      setBackColour(0, 0, 0);          // on Black
      for (i = strlen(EditText); i <= 25; i = i + 1)
      {
        rectangle(charPitch * i + (charPitch / 2), 320, charPitch, 40, 0, 0, 0);
      }

      // Draw the cursor and erase cursors either side
      rectangle(12 + charPitch * CursorPos, 320, 2, 40, 255, 255, 255);
      rectangle(12 + charPitch * (CursorPos + 1), 320, 2, 40, 0, 0, 0);
      if (CursorPos >= 1)
      {
        rectangle(12 + charPitch * (CursorPos - 1), 320, 2, 40, 0, 0, 0);
      }
      refreshed = true;
    }

    // Wait for key press
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;
    refreshed = false;

    token = IsMenuButtonPushed(rawX, rawY);
    printf("Keyboard Token %d\n", token);

    // Highlight special keys when touched
    if ((token == 5) || (token == 8) || (token == 9) || (token == 2) || (token == 3)) // Clear, Enter, backspace, L and R arrows
    {
        SetButtonStatus(ButtonNumber(12, token), 1);
        DrawButton(ButtonNumber(12, token));
        UpdateWindow();
        usleep(300000);
        SetButtonStatus(ButtonNumber(12, token), 0);
        DrawButton(ButtonNumber(12, token));
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
        SetButtonStatus(ButtonNumber(12, token), ShiftStatus);
        DrawButton(ButtonNumber(12, token));
        //UpdateWindow();
        usleep(300000);
        ShiftStatus = 2 - (2 * KeyboardShift); // 0 = Upper, 2 = lower
        SetButtonStatus(ButtonNumber(12, token), ShiftStatus);
        DrawButton(ButtonNumber(12, token));
        //UpdateWindow();

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


void ChangePlutoIP()
{
  char RequestText[64];
  char InitText[64];
  bool IsValid = FALSE;
  char PlutoIPCopy[31];

  while (IsValid == FALSE)
  {
    strcpy(RequestText, "Enter IP address for Pluto");
    strcpyn(InitText, PlutoIP, 17);
    Keyboard(RequestText, InitText, 17);
  
    strcpy(PlutoIPCopy, KeyboardReturn);
    if(is_valid_ip(PlutoIPCopy) == 1)
    {
      IsValid = TRUE;
    }
  }
  printf("Pluto IP set to: %s\n", KeyboardReturn);

  // Save IP to config file
  SetConfigParam(PATH_PCONFIG, "plutoip", KeyboardReturn);
  strcpy(PlutoIP, KeyboardReturn);
}


void RebootPluto()
{
  int test = 1;
  int count = 0;
  // int rawX, rawY, rawPressure;

  system("/home/pi/rpidatv/scripts/reboot_pluto.sh");
  MsgBox4("Pluto Rebooting", "Wait for reconnection", " ", " ");

  while(test == 1)
  {
    //if (getTouchSample(&rawX, &rawY, &rawPressure) != 0)
    //{
    //  return;
    //}
    usleep(1000000);
    test = CheckPlutoConnect();
    count = count + 1;
    if (count > 29)
    {
      MsgBox4("Failed to Reconnect","to Pluto", " ", "Touch Screen to Continue");
      wait_touch();
      return;
    }
  }
  MsgBox4("Pluto Rebooted"," ", " ", "Touch Screen to Continue");
  wait_touch();
}

/*
void ChangeADFRef(int NoButton)
{
  char RequestText[64];
  char InitText[64];
  char Param[64];
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
    snprintf(RequestText, 45, "Enter new ADF4351 Reference Frequncy %d in Hz", NoButton + 1);
    //snprintf(InitText, 10, "%s", ADFRef[NoButton]);
    strcpyn(InitText, ADFRef[NoButton], 10);
    while (Spaces >= 1)
    {
      Keyboard(RequestText, InitText, 9);
  
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
    snprintf(Param, 8, "adfref%d", NoButton + 1);
    SetConfigParam(PATH_PPRESETS, Param, KeyboardReturn);
    strcpy(CurrentADFRef, KeyboardReturn);
    SetConfigParam(PATH_PCONFIG, "adfref", KeyboardReturn);
    break;
  case 3:
    snprintf(RequestText, 45, "Enter new ADF5355 Reference Frequncy in Hz");
    //snprintf(InitText, 10, "%s", ADF5355Ref);
    strcpyn(InitText, ADF5355Ref, 10);
    while (Spaces >= 1)
    {
      Keyboard(RequestText, InitText, 9);
  
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
    strcpy(ADF5355Ref, KeyboardReturn);
    SetConfigParam(PATH_SGCONFIG, "adf5355ref", KeyboardReturn);
    break;
  default:
    break;
  }
  printf("ADF4351Ref set to: %s\n", CurrentADFRef);
  printf("ADF5355Ref set to: %s\n", ADF5355Ref);
}
*/

void waituntil(int w, int h)
{
  // Wait for a screen touch and act on its position

  int rawX, rawY, rawPressure, i;
  rawX = 0;
  rawY = 0;
  // printf("Entering WaitUntil\n");
  // Start the main loop for the Touchscreen
  for (;;)
  {
    if (getTouchSample(&rawX, &rawY, &rawPressure)==0) continue;

    // Screen has been touched
    printf("x=%d y=%d\n", rawX, rawY);

      // Check which button has been pressed (Returns 0 - 23)

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

        switch (i)
        {
        case 0:
          printf("MENU 2 \n");       // Output Device Menu
          CurrentMenu=2;
          Start_Highlights_Menu2();
          UpdateWindow();
          break;
        //case 1:
        //  printf("MENU 3 \n");       // Attenuator Menu
        //  CurrentMenu=3;
        //  Start_Highlights_Menu3();
        //  UpdateWindow();
        //  break;
        //case 2:
        //  printf("MENU 4 \n");       // References Menu
        //  CurrentMenu=4;
        //  //setBackColour(0, 0, 0);
       //   Start_Highlights_Menu4();
        //  UpdateWindow();
        //  break;
        case 20:                       // Run
          printf("MENU 11 \n");
          CurrentMenu=11;
          Start_Highlights_Menu11();
          UpdateWindow();
          ShowFreq(DisplayFreq);
          ShowLevel(DisplayLevel);
          ShowAtten();
          ShowOPFreq();
          PlutoCalValid = false;
          break;
        case 22:                      // Exit
          printf("Exit from Sig-gen\n");
          clearScreen();
          cleanexit(129); //exit to Portsdown
          break;
        default:
          printf("Menu 1 Error\n");
        }
        continue;  // Completed Menu 1 action, go and wait for touch
      }

      if (CurrentMenu == 2)  // Menu 2 Output Device Menu
      {
        printf("Button Event %d, Entering Menu 2 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set Output Device Cancel\n");
          break;
        case 0:   // Pluto
        case 1:   // DATV Express
        case 2:   // Elcom
        case 5:   // ADF4351
        //case 6:   // ADF5355
          SelectOsc(i); 
          break;
        default:
          printf("Menu 2 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 2\n");
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 2 action, go and wait for touch
      }

      if (CurrentMenu == 3)  // Menu 3 Attenuator Type
      {
        printf("Button Event %d, Entering Menu 3 Case Statement\n",i);
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
          printf("Menu 3 Error\n");
        }
        UpdateWindow();
        usleep(500000);
        SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
        printf("Returning to MENU 1 from Menu 3\n");
        CurrentMenu = 1;
        Start_Highlights_Menu1();
        UpdateWindow();
        continue;   // Completed Menu 3 action, go and wait for touch
      }

      if (CurrentMenu == 4)  // Menu 4 Select and Change ADF Reference Frequency
      {
        printf("Button Event %d, Entering Menu 4 Case Statement\n",i);
        switch (i)
        {
        case 4:                               // Cancel
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          printf("Set or change ADFRef\n");
          UpdateWindow();
          usleep(500000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0); // Reset cancel (even if not selected)
          printf("Returning to MENU 1 from Menu 32\n");
          CurrentMenu = 1;
          Start_Highlights_Menu1();
          UpdateWindow();
          break;
        case 0:                               // Use ADF4351 Ref 1
        case 1:                               // Use ADF4351 Ref 2
        case 2:                               // Use ADF4351 Ref 3
        case 3:                               // Set ADF5355 Ref
        case 5:                               // Set ADF4351 Ref 1
        case 6:                               // Set ADF4351 Ref 2
        case 7:                               // Set ADF4351 Ref 3
          printf("Changing ADFRef\n");
          //ChangeADFRef(i);
          CurrentMenu=32;
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu4();
          UpdateWindow();
          break;
        case 9:                               // Set RTL-SDR ppm offset
          //ChangeRTLppm();
          setBackColour(0, 0, 0);
          clearScreen();
          Start_Highlights_Menu3();          // Refresh button labels
          UpdateWindow();
          break;
        default:
          printf("Menu 4 Error\n");
        }
        // stay in Menu 4 if parameter changed
        continue;   // Completed Menu 4 action, go and wait for touch
      }

      if (CurrentMenu == 11)  // Menu 11 TX RF Output Mode
      {
        printf("Button Event %d, Entering Menu 11 Case Statement\n",i);
        switch (i)
        {
        case 0:                                                // Adjust Level
        case 1:
        case 2:
        case 5:
        case 6:
        case 7:
           AdjustLevel(i);
           CalcOPLevel();
           break;
        case 3:                                                // Save
          SelectInGroupOnMenu(CurrentMenu, 3, 3, 3, 1);
          UpdateWindow();
          SaveState();
          usleep(250000);
          SelectInGroupOnMenu(CurrentMenu, 3, 3, 3, 0);
          printf("Save Preset\n");
          break;
        case 4:                                                // Recall
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 1);
          UpdateWindow();
          OscStop();
          ReadSavedState();
          InitOsc();
          usleep(250000);
          SelectInGroupOnMenu(CurrentMenu, 4, 4, 4, 0);
          printf("Recall Preset\n");
          break;
        case 8:                                                // Adjust Frequency
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
          AdjustFreq(i);
          CalcOPLevel();
          break;
        case 30:                                   // On
          if (OutputStatus == 1)  // Oscillator already running
          {
            OscStop();
          }
          else
          {
            if ((strcmp(osc, "pluto") != 0) || PlutoCalValid)
            {
              OscStart();
            }
          }
          break;
        case 31:                                   // Off
          OscStop();
          break;
        case 32:                                   // Modulation or Cal
          if (strcmp(osc, "express") == 0)
          {
            if (ModOn == 0)
            {
              ModOn = 1;
              if (OutputStatus == 1)  // Oscillator already running
              {
                OscStop();
                OscStart();
              }
              SelectInGroupOnMenu(CurrentMenu, 32, 32, 32, 1);
            }
            else
            {
              ModOn = 0;
              if (OutputStatus == 1)  // Oscillator already running
              {
                OscStop();
                OscStart();
              }
              SelectInGroupOnMenu(CurrentMenu, 32, 32, 32, 0);
            }
            CalcOPLevel();
          }
          if (strcmp(osc, "pluto") == 0)
          {
            plutotx(true);
            SetButtonStatus(ButtonNumber(11, 32), 4);
            Start_Highlights_Menu11();
            UpdateWindow();
            usleep(500000);
            PlutoOff();
            PlutoCalValid = true;
            SetButtonStatus(ButtonNumber(11, 32), 3);
          }
          break;
        case 33:                                   // Exit
          OscStop();
          printf("MENU 1 \n"); 
          CurrentMenu=1;
          Start_Highlights_Menu1();
          UpdateWindow();
          PlutoCalValid = false;
          break;
        default:
          printf("Menu 11 Error\n");
        }
        if (i != 33)  // Don't show the parameters if exiting to the start menu
        {
          Start_Highlights_Menu11();
          UpdateWindow();
          ShowLevel(DisplayLevel);
          ShowFreq(DisplayFreq);
          ShowAtten();
          ShowOPFreq();
        }
        continue;   // Completed Menu 11 action, go and wait for touch
      }

      if (CurrentMenu == 12)  // Menu 12 Keyboard (should not get here)
      {
        printf("Error: Flow at Menu 12 in WaitUntil\n");
      }
  }
}

void Define_Menu1()
{
  int button = 0;
  strcpy(MenuTitle[1], "BATC Portsdown 4 Signal Generator Main Menu"); 

  // Frequency - Bottom Row, Menu 1

  button = CreateButton(1, 0);
  AddButtonStatus(button, "Output to^", &Blue);
  AddButtonStatus(button, "Output to^", &Green);

//  button = CreateButton(1, 1);
//  AddButtonStatus(button, "Attenuator^", &Blue);
//  AddButtonStatus(button, "Attenuator^", &Green);

//  button = CreateButton(1, 2);
//  AddButtonStatus(button, "Set Refs", &Blue);
//  AddButtonStatus(button, "Set Refs", &Green);

  button = CreateButton(1, 20);
  AddButtonStatus(button,"CONTROL",&Blue);
  AddButtonStatus(button,"CONTROL",&Red);

  button = CreateButton(1, 22);
  AddButtonStatus(button,"EXIT",&Blue);
  AddButtonStatus(button,"EXIT",&Green);
}

void Start_Highlights_Menu1()
{
  printf("Entering Start_Highlights_Menu1\n");

  char Presettext[63];

  strcpy(Presettext, "Output to^");
  strcat(Presettext, osc_text);
  AmendButtonStatus(0, 0, Presettext, &Blue);

  strcpy(Presettext, "Attenuator^");
  strcat(Presettext, AttenType);
  AmendButtonStatus(1, 0, Presettext, &Blue);

  //strcpy(Presettext, "Set^Ref Freqs");
  //AmendButtonStatus(2, 0, Presettext, &Blue);
}

void Define_Menu2()
{
  int button;

  strcpy(MenuTitle[2], "Output Device Menu (2)"); 

  // Bottom Row, Menu 2

// pluto Pluto
// adf4351 ADF4351
// adf4355 ADF4355
// elcom  Elcom
// express DATV Express
// lime Lime Mini


  button = CreateButton(2, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  button = CreateButton(2, 0);
  AddButtonStatus(button, "Pluto", &Blue);
  AddButtonStatus(button, "Pluto", &Green);

  button = CreateButton(2, 1);
  AddButtonStatus(button, "DATV^Express", &Blue);
  AddButtonStatus(button, "DATV^Express", &Green);

  button = CreateButton(2, 2);
  AddButtonStatus(button, "Elcom", &Blue);  
  AddButtonStatus(button, "Elcom", &Green);

  //button = CreateButton(2, 3);                         //Lime
  //AddButtonStatus(button, "Lime Mini", &Blue);
  //AddButtonStatus(button, "Lime Mini", &Green);

  // 2nd Row, Menu 2

  button = CreateButton(2, 5);
  AddButtonStatus(button, "ADF4351", &Blue);
  AddButtonStatus(button, "ADF4351", &Green);

  //button = CreateButton(2, 6);
  //AddButtonStatus(button, "ADF5355", &Blue);
  //AddButtonStatus(button, "ADF5355", &Green);
}

void Start_Highlights_Menu2()
{

}


void Define_Menu3()
{
  int button;

  strcpy(MenuTitle[3], "Attenuator Selection Menu (3)"); 

  // Bottom Row, Menu 3

  button = CreateButton(3, 4);
  AddButtonStatus(button, "Cancel", &DBlue);
  AddButtonStatus(button, "Cancel", &LBlue);

  // 2nd Row, Menu 3

  button = CreateButton(3, 5);
  AddButtonStatus(button,"None^Fitted",&Blue);
  AddButtonStatus(button,"None^Fitted",&Green);

  button = CreateButton(3, 6);
  AddButtonStatus(button,"PE4312",&Blue);
  AddButtonStatus(button,"PE4312",&Green);

  button = CreateButton(3, 7);
  AddButtonStatus(button,"PE43713",&Blue);
  AddButtonStatus(button,"PE43713",&Green);

  button = CreateButton(3, 8);
  AddButtonStatus(button,"HMC1119",&Blue);
  AddButtonStatus(button,"HMC1119",&Green);
}

void Start_Highlights_Menu3()
{
  // Audio
  ReadSavedState();
  if (strcmp(AttenType, "NONE") == 0)
  {
    SelectInGroupOnMenu(3, 5, 9, 5, 1);
  }
  else if (strcmp(AttenType, "PE4312") == 0)
  {
    SelectInGroupOnMenu(3, 5, 9, 6, 1);
  }
  else if (strcmp(AttenType, "PE43713") == 0)
  {
    SelectInGroupOnMenu(3, 5, 9, 7, 1);
  }
  else if (strcmp(AttenType, "HMC1119") == 0)
  {
    SelectInGroupOnMenu(3, 5, 9, 8, 1);
  }
  else
  {
    SelectInGroupOnMenu(3, 5, 9, 5, 1);
  }
}

void Define_Menu4()
{
}

void Start_Highlights_Menu4()
{
}

void Define_Menu11()  // Control Panel
{
  int button;
  int i;

  strcpy(MenuTitle[11], "Portsdown Signal Generator Control Panel");

  for (i = 0; i <= 2; i++)                      // Three decrement power buttons
  {
    button = CreateButton(11, i);
    AddButtonStatus(button, "_", &Blue);
    AddButtonStatus(button, " ", &Black);
  }

  button = CreateButton(11, 3);
  AddButtonStatus(button,"Save",&Blue);
  AddButtonStatus(button,"Save",&Green);

  button = CreateButton(11, 4);
  AddButtonStatus(button,"Recall",&Blue);
  AddButtonStatus(button,"Recall",&Green);

  for (i = 5; i <= 7; i++)                      // Three increment power buttons
  {
    button = CreateButton(11, i);
    AddButtonStatus(button, "+", &Blue);
    AddButtonStatus(button, " ", &Black);
  }

  for (i = 8; i <= 18; i++)                      // Eleven decrement freq buttons
  {
    button = CreateButton(11, i);
    AddButtonStatus(button, "_", &Blue);
    AddButtonStatus(button, " ", &Black);
  }

  for (i = 19; i <= 29; i++)                      // Eleven increment freq buttons
  {
    button = CreateButton(11, i);
    AddButtonStatus(button, "+", &Blue);
    AddButtonStatus(button, " ", &Black);
  }

  button = CreateButton(11, 30);
  AddButtonStatus(button, "START", &Blue);
  AddButtonStatus(button, "ON", &Red);
  AddButtonStatus(button, "START", &Grey);

  button = CreateButton(11, 31);
  AddButtonStatus(button, "OFF", &Blue);
  AddButtonStatus(button, "OFF", &Blue);

  button = CreateButton(11, 32);
  AddButtonStatus(button, "QPSK Mod", &Blue);
  AddButtonStatus(button, "QPSK Mod", &Red);
  AddButtonStatus(button, "QPSK Mod", &Grey);
  AddButtonStatus(button, "Cal Pluto", &Blue);
  AddButtonStatus(button, "Cal Pluto" ,&Red);

  button = CreateButton(11, 33);
  AddButtonStatus(button, "Exit", &Blue);
  AddButtonStatus(button, "Exit", &Green);
}


void Start_Highlights_Menu11()
{
  if ((strcmp(osc, "pluto") != 0) || PlutoCalValid)
  {
    SetButtonStatus(ButtonNumber(11, 30), OutputStatus); // Off (blue), On (red)
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 30), 2); // Grey because Pluto Cal required
  }
  
  if (strcmp(osc, "express") == 0)                     // QPSK or Pluto Cal
  {
    SetButtonStatus(ButtonNumber(11, 32), ModOn);
  }
  else if (strcmp(osc, "pluto") == 0)
  {
    if (GetButtonStatus(ButtonNumber(11, 32)) != 4)
    {
      SetButtonStatus(ButtonNumber(11, 32), 3);
    }
  }
  else
  {
    SetButtonStatus(ButtonNumber(11, 32), 2);
  }
}


void Define_Menu12()  // Keyboard
{
  int button;

  strcpy(MenuTitle[12], " "); 

  button = CreateButton(12, 0);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  AddButtonStatus(button, "SPACE", &Blue);
  AddButtonStatus(button, "SPACE", &LBlue);
  //button = CreateButton(12, 1);
  //AddButtonStatus(button, "X", &Blue);
  //AddButtonStatus(button, "X", &LBlue);
  button = CreateButton(12, 2);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  AddButtonStatus(button, "<", &Blue);
  AddButtonStatus(button, "<", &LBlue);
  button = CreateButton(12, 3);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  AddButtonStatus(button, ">", &Blue);
  AddButtonStatus(button, ">", &LBlue);
  button = CreateButton(12, 4);
  AddButtonStatus(button, "-", &Blue);
  AddButtonStatus(button, "-", &LBlue);
  AddButtonStatus(button, "_", &Blue);
  AddButtonStatus(button, "_", &LBlue);
  button = CreateButton(12, 5);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
  AddButtonStatus(button, "Clear", &Blue);
  AddButtonStatus(button, "Clear", &LBlue);
  button = CreateButton(12, 6);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(12, 7);
  AddButtonStatus(button, "^", &LBlue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &Blue);
  AddButtonStatus(button, "^", &LBlue);
  button = CreateButton(12, 8);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  AddButtonStatus(button, "Enter", &Blue);
  AddButtonStatus(button, "Enter", &LBlue);
  button = CreateButton(12, 9);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  AddButtonStatus(button, "<=", &Blue);
  AddButtonStatus(button, "<=", &LBlue);
  button = CreateButton(12, 10);
  AddButtonStatus(button, "Z", &Blue);
  AddButtonStatus(button, "Z", &LBlue);
  AddButtonStatus(button, "z", &Blue);
  AddButtonStatus(button, "z", &LBlue);
  button = CreateButton(12, 11);
  AddButtonStatus(button, "X", &Blue);
  AddButtonStatus(button, "X", &LBlue);
  AddButtonStatus(button, "x", &Blue);
  AddButtonStatus(button, "x", &LBlue);
  button = CreateButton(12, 12);
  AddButtonStatus(button, "C", &Blue);
  AddButtonStatus(button, "C", &LBlue);
  AddButtonStatus(button, "c", &Blue);
  AddButtonStatus(button, "c", &LBlue);
  button = CreateButton(12, 13);
  AddButtonStatus(button, "V", &Blue);
  AddButtonStatus(button, "V", &LBlue);
  AddButtonStatus(button, "v", &Blue);
  AddButtonStatus(button, "v", &LBlue);
  button = CreateButton(12, 14);
  AddButtonStatus(button, "B", &Blue);
  AddButtonStatus(button, "B", &LBlue);
  AddButtonStatus(button, "b", &Blue);
  AddButtonStatus(button, "b", &LBlue);
  button = CreateButton(12, 15);
  AddButtonStatus(button, "N", &Blue);
  AddButtonStatus(button, "N", &LBlue);
  AddButtonStatus(button, "n", &Blue);
  AddButtonStatus(button, "n", &LBlue);
  button = CreateButton(12, 16);
  AddButtonStatus(button, "M", &Blue);
  AddButtonStatus(button, "M", &LBlue);
  AddButtonStatus(button, "m", &Blue);
  AddButtonStatus(button, "m", &LBlue);
  button = CreateButton(12, 17);
  AddButtonStatus(button, ",", &Blue);
  AddButtonStatus(button, ",", &LBlue);
  AddButtonStatus(button, "!", &Blue);
  AddButtonStatus(button, "!", &LBlue);
  button = CreateButton(12, 18);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  AddButtonStatus(button, ".", &Blue);
  AddButtonStatus(button, ".", &LBlue);
  button = CreateButton(12, 19);
  AddButtonStatus(button, "/", &Blue);
  AddButtonStatus(button, "/", &LBlue);
  AddButtonStatus(button, "?", &Blue);
  AddButtonStatus(button, "?", &LBlue);
  button = CreateButton(12, 20);
  AddButtonStatus(button, "A", &Blue);
  AddButtonStatus(button, "A", &LBlue);
  AddButtonStatus(button, "a", &Blue);
  AddButtonStatus(button, "a", &LBlue);
  button = CreateButton(12, 21);
  AddButtonStatus(button, "S", &Blue);
  AddButtonStatus(button, "S", &LBlue);
  AddButtonStatus(button, "s", &Blue);
  AddButtonStatus(button, "s", &LBlue);
  button = CreateButton(12, 22);
  AddButtonStatus(button, "D", &Blue);
  AddButtonStatus(button, "D", &LBlue);
  AddButtonStatus(button, "d", &Blue);
  AddButtonStatus(button, "d", &LBlue);
  button = CreateButton(12, 23);
  AddButtonStatus(button, "F", &Blue);
  AddButtonStatus(button, "F", &LBlue);
  AddButtonStatus(button, "f", &Blue);
  AddButtonStatus(button, "f", &LBlue);
  button = CreateButton(12, 24);
  AddButtonStatus(button, "G", &Blue);
  AddButtonStatus(button, "G", &LBlue);
  AddButtonStatus(button, "g", &Blue);
  AddButtonStatus(button, "g", &LBlue);
  button = CreateButton(12, 25);
  AddButtonStatus(button, "H", &Blue);
  AddButtonStatus(button, "H", &LBlue);
  AddButtonStatus(button, "h", &Blue);
  AddButtonStatus(button, "h", &LBlue);
  button = CreateButton(12, 26);
  AddButtonStatus(button, "J", &Blue);
  AddButtonStatus(button, "J", &LBlue);
  AddButtonStatus(button, "j", &Blue);
  AddButtonStatus(button, "j", &LBlue);
  button = CreateButton(12, 27);
  AddButtonStatus(button, "K", &Blue);
  AddButtonStatus(button, "K", &LBlue);
  AddButtonStatus(button, "k", &Blue);
  AddButtonStatus(button, "k", &LBlue);
  button = CreateButton(12, 28);
  AddButtonStatus(button, "L", &Blue);
  AddButtonStatus(button, "L", &LBlue);
  AddButtonStatus(button, "l", &Blue);
  AddButtonStatus(button, "l", &LBlue);
  //button = CreateButton(12, 29);
  //AddButtonStatus(button, "/", &Blue);
  //AddButtonStatus(button, "/", &LBlue);
  button = CreateButton(12, 30);
  AddButtonStatus(button, "Q", &Blue);
  AddButtonStatus(button, "Q", &LBlue);
  AddButtonStatus(button, "q", &Blue);
  AddButtonStatus(button, "q", &LBlue);
  button = CreateButton(12, 31);
  AddButtonStatus(button, "W", &Blue);
  AddButtonStatus(button, "W", &LBlue);
  AddButtonStatus(button, "w", &Blue);
  AddButtonStatus(button, "w", &LBlue);
  button = CreateButton(12, 32);
  AddButtonStatus(button, "E", &Blue);
  AddButtonStatus(button, "E", &LBlue);
  AddButtonStatus(button, "e", &Blue);
  AddButtonStatus(button, "e", &LBlue);
  button = CreateButton(12, 33);
  AddButtonStatus(button, "R", &Blue);
  AddButtonStatus(button, "R", &LBlue);
  AddButtonStatus(button, "r", &Blue);
  AddButtonStatus(button, "r", &LBlue);
  button = CreateButton(12, 34);
  AddButtonStatus(button, "T", &Blue);
  AddButtonStatus(button, "T", &LBlue);
  AddButtonStatus(button, "t", &Blue);
  AddButtonStatus(button, "t", &LBlue);
  button = CreateButton(12, 35);
  AddButtonStatus(button, "Y", &Blue);
  AddButtonStatus(button, "Y", &LBlue);
  AddButtonStatus(button, "y", &Blue);
  AddButtonStatus(button, "y", &LBlue);
  button = CreateButton(12, 36);
  AddButtonStatus(button, "U", &Blue);
  AddButtonStatus(button, "U", &LBlue);
  AddButtonStatus(button, "u", &Blue);
  AddButtonStatus(button, "u", &LBlue);
  button = CreateButton(12, 37);
  AddButtonStatus(button, "I", &Blue);
  AddButtonStatus(button, "I", &LBlue);
  AddButtonStatus(button, "i", &Blue);
  AddButtonStatus(button, "i", &LBlue);
  button = CreateButton(12, 38);
  AddButtonStatus(button, "O", &Blue);
  AddButtonStatus(button, "O", &LBlue);
  AddButtonStatus(button, "o", &Blue);
  AddButtonStatus(button, "o", &LBlue);
  button = CreateButton(12, 39);
  AddButtonStatus(button, "P", &Blue);
  AddButtonStatus(button, "P", &LBlue);
  AddButtonStatus(button, "p", &Blue);
  AddButtonStatus(button, "p", &LBlue);

  button = CreateButton(12, 40);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  AddButtonStatus(button, "1", &Blue);
  AddButtonStatus(button, "1", &LBlue);
  button = CreateButton(12, 41);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  AddButtonStatus(button, "2", &Blue);
  AddButtonStatus(button, "2", &LBlue);
  button = CreateButton(12, 42);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  AddButtonStatus(button, "3", &Blue);
  AddButtonStatus(button, "3", &LBlue);
  button = CreateButton(12, 43);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  AddButtonStatus(button, "4", &Blue);
  AddButtonStatus(button, "4", &LBlue);
  button = CreateButton(12, 44);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  AddButtonStatus(button, "5", &Blue);
  AddButtonStatus(button, "5", &LBlue);
  button = CreateButton(12, 45);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  AddButtonStatus(button, "6", &Blue);
  AddButtonStatus(button, "6", &LBlue);
  button = CreateButton(12, 46);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  AddButtonStatus(button, "7", &Blue);
  AddButtonStatus(button, "7", &LBlue);
  button = CreateButton(12, 47);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  AddButtonStatus(button, "8", &Blue);
  AddButtonStatus(button, "8", &LBlue);
  button = CreateButton(12, 48);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  AddButtonStatus(button, "9", &Blue);
  AddButtonStatus(button, "9", &LBlue);
  button = CreateButton(12, 49);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
  AddButtonStatus(button, "0", &Blue);
  AddButtonStatus(button, "0", &LBlue);
}

static void terminate(int dummy)
{
  char Commnd[255];
  OscStop();
  printf("Terminate\n");
  setBackColour(0, 0, 0);
  clearScreen();
  closeScreen();
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
  wscreen = 800;
  hscreen = 480;
  int screenXmax, screenXmin;
  int screenYmax, screenYmin;
  int i;
  //char Param[255];
  //char Value[255];
  //char vcoding[256];

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

  // Check for presence of touchscreen
  for(NoDeviceEvent = 0; NoDeviceEvent < 7; NoDeviceEvent++)
  {
    if (openTouchScreen(NoDeviceEvent) == 1)
    {
      if(getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax)==1) break;
    }
  }
  if(NoDeviceEvent == 7) 
  {
    perror("No Touchscreen found");
    exit(1);
  }

  // Show Portsdown Logo
  system("sudo fbi -T 1 -noverbose -a \"/home/pi/rpidatv/scripts/images/BATC_Black.png\" >/dev/null 2>/dev/null");
  system("(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &");

  // Calculate screen parameters
  scaleXvalue = ((float)screenXmax-screenXmin) / wscreen;
  printf ("X Scale Factor = %f\n", scaleXvalue);
  printf ("wscreen = %d\n", wscreen);
  printf ("screenXmax = %d\n", screenXmax);
  printf ("screenXmim = %d\n", screenXmin);
  scaleYvalue = ((float)screenYmax-screenYmin) / hscreen;
  printf ("Y Scale Factor = %f\n", scaleYvalue);
  printf ("hscreen = %d\n", hscreen);

  // Define button grid
  // -25 keeps right hand side symmetrical with left hand side
  wbuttonsize = (wscreen - 25) / 5;
  hbuttonsize = hscreen / 6;

  // Initialise all the button Status Indexes to 0
  InitialiseButtons();

  // Define all the Menu buttons
  Define_Menu1();
  Define_Menu2();
  Define_Menu3();
  Define_Menu4();

  Define_Menu11();
  Define_Menu12();

  // Initialise direct access to the 7 inch screen
  initScreen();

  ReadSavedState();
  InitOsc(); 

  // Check if DATV Express Server required and, if so, start it
  CheckExpress();

  // Check Lime connected if selected
  CheckLimeReady();

  // Set the Band (and filter) Switching
  // Must be done after (not before) starting DATV Express Server
  system ("sudo /home/pi/rpidatv/scripts/ctlfilter.sh");
  // and wait for it to finish using portsdown_config.txt
  usleep(100000);

  // Clear the screen ready for Menu 1
  //setBackColour(255, 255, 255);          // White background
  //clearScreen();

  // Determine button highlights
  Start_Highlights_Menu1();
  printf("Entering Update Window\n");  
  UpdateWindow();

  // Go and wait for the screen to be touched
  waituntil(wscreen,hscreen);

  // Not sure that the program flow ever gets here

  return 0;
}
