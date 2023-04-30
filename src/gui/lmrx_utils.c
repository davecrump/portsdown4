// lmrx_utils.c
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

Initial code Dave, G8GKQ
*/

#include "lmrx_utils.h"

typedef struct
{
  int reg_val;
  int level;
} agc_levels;

// Look-up tables at https://wiki.batc.org.uk/MiniTiouner_Power_Level_Indication#Tabulated_Results
static agc_levels agc1_levels[] =
{
  {1,     -70},
  {10,    -69},
  {21800, -68},
  {25100, -67},
  {27100, -66},
  {28100, -65},
  {28900, -64},
  {29600, -63},
  {30100, -62},
  {30550, -61},
  {31000, -60},
  {31350, -59},
  {31700, -58},
  {32050, -57},
  {32400, -56},
  {32700, -55},
  {33000, -54},
  {33300, -53},
  {33600, -52},
  {33900, -51},
  {34200, -50},
  {34500, -49},
  {34750, -48},
  {35000, -47},
  {35250, -46},
  {35500, -45},
  {35750, -44},
  {36000, -43},
  {36200, -42},
  {36400, -41},
  {36600, -40},
  {36800, -39},
  {37000, -38},
  {37200, -37},
  {37400, -36},
  {37600, -35},
  {40000, -34},
};

static agc_levels agc2_levels[] =
{
  {182,   -71},
  {200,   -72},
  {225,   -73},
  {255,   -74},
  {290,   -75},
  {325,   -76},
  {360,   -77},
  {400,   -78},
  {450,   -79},
  {500,   -80},
  {560,   -81},
  {625,   -82},
  {700,   -83},
  {780,   -84},
  {880,   -85},
  {1000,  -86},
  {1140,  -87},
  {1300,  -88},
  {1480,  -89},
  {1660,  -90},
  {1840,  -91},
  {2020,  -92},
  {2740,  -93},
  {3200,  -94},
  {16000, -95},
};

// Check array sizes
enum { NUM_AGC1_LEVELS = sizeof(agc1_levels) / sizeof(agc1_levels[0]) };
enum { NUM_AGC2_LEVELS = sizeof(agc2_levels) / sizeof(agc2_levels[0]) };

int CalcInputPwr(uint16_t AGC1, uint16_t AGC2)
{
  int i;
  int InputPwr = 0;

  // First see if we are using AGC1 or AGC2
  if (AGC1 > 0) // use AGC1
  {
    for (i = 0; i < NUM_AGC1_LEVELS; i++)
    {
      if (AGC1 <= agc1_levels[i].reg_val)
      {
        InputPwr = agc1_levels[i].level;
        break;
      }
    }
  }
  else  // use AGC2  
  {
    for (i = 0; i < NUM_AGC2_LEVELS; i++)
    {
      if (AGC2 <= agc2_levels[i].reg_val)
      {
        InputPwr = agc2_levels[i].level;
        break;
      }
    }
  }
  return InputPwr;
}

void chopN(char *str, size_t n)  // note - duplicated in main!!
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


int LMDecoderState(char *stat_string, char *STATEtext)
{
  char STATEnumbers[63];
  int STATE = 0;

  strcpy(STATEnumbers, stat_string);
  chopN(STATEnumbers, 2);
  STATE = atoi(STATEnumbers);
  switch(STATE)
  {
    case 0:
      strcpy(STATEtext, "Initialising");
      break;
    case 1:
      strcpy(STATEtext, "Searching");
      break;
    case 2:
      strcpy(STATEtext, "Found Headers");
      break;
    case 3:
      strcpy(STATEtext, "DVB-S Lock");
      break;
    case 4:
      strcpy(STATEtext, "DVB-S2 Lock");
      break;
    default:
    snprintf(STATEtext, 10, "%d", STATE);
  }
  return STATE;
}



int LMLookupVidEncoding(char *stat_string, char *VEncodingtext)
{
  char LocalEncodingtext[63];
  int EncodingCode = 0;
  int IsVideo = 0;

  strcpy(LocalEncodingtext, stat_string);
  chopN(LocalEncodingtext, 3);
  EncodingCode = atoi(LocalEncodingtext);
  switch(EncodingCode)
  {
    case 2:
      strcpy(VEncodingtext, "MPEG-2");
    break;
    case 16:
      strcpy(VEncodingtext, "H263");
    break;
    case 27:
      strcpy(VEncodingtext, "H264");
    break;
    case 36:
      strcpy(VEncodingtext, "H265");
    break;
    case 51:
      strcpy(VEncodingtext, "H266");
    break;
    default:
      IsVideo = 1;
    break;
  }
  return IsVideo;
}


int LMLookupAudEncoding(char *stat_string, char *AEncodingtext)
{
  char LocalEncodingtext[63];
  int EncodingCode = 0;
  int IsAudio = 0;

  strcpy(LocalEncodingtext, stat_string);
  chopN(LocalEncodingtext, 3);
  EncodingCode = atoi(LocalEncodingtext);
  switch(EncodingCode)
  {
    case 3:
      strcpy(AEncodingtext, " MPA");
    break;
    case 4:
      strcpy(AEncodingtext, " MPA");
    break;
    case 15:
      strcpy(AEncodingtext, " AAC");
    break;
    case 32:
      strcpy(AEncodingtext, " MPA");
    break;
    case 129:
      strcpy(AEncodingtext, " AC3");
    break;
    default:
      IsAudio = 1;
    break;
  }
  return IsAudio;
}


float LMLookupMODCOD(char *stat_string, int State, char *Modtext, char *MCtext)
{
  char LocalMCtext[63];
  int MC;
  float MERThresh;

  strcpy(LocalMCtext, stat_string);
  chopN(LocalMCtext, 3);
  MC = atoi(LocalMCtext);

  if (State == 3)                     // QPSK
  {
    strcpy(Modtext, "QPSK");
    switch(MC)
    {
      case 0:
        strcpy(MCtext, "FEC 1/2");
        MERThresh = 1.7; //
       break;
       case 1:
         strcpy(MCtext, "FEC 2/3");
         MERThresh = 3.3; //
       break;
       case 2:
         strcpy(MCtext, "FEC 3/4");
         MERThresh = 4.2; //
       break;
       case 3:
         strcpy(MCtext, "FEC 5/6");
         MERThresh = 5.1; //
       break;
       case 4:
         strcpy(MCtext, "FEC 6/7");
         MERThresh = 5.5; //
       break;
       case 5:
         strcpy(MCtext, "FEC 7/8");
         MERThresh = 5.8; //
       break;
       default:
         strcpy(MCtext, "FEC -");
         MERThresh = 0; //
         strcat(MCtext, LocalMCtext);
       break;
    }
  }

  if (State == 4)                                        // DVB-S2
  {
    switch(MC)
    {
      case 1:
        strcpy(MCtext, "FEC 1/4");
        MERThresh = -2.3; //
      break;
      case 2:
        strcpy(MCtext, "FEC 1/3");
        MERThresh = -1.2; //
      break;
      case 3:
         strcpy(MCtext, "FEC 2/5");
         MERThresh = -0.3; //
       break;
       case 4:
         strcpy(MCtext, "FEC 1/2");
         MERThresh = 1.0; //
       break;
       case 5:
         strcpy(MCtext, "FEC 3/5");
         MERThresh = 2.3; //
       break;
       case 6:
         strcpy(MCtext, "FEC 2/3");
         MERThresh = 3.1; //
        break;
        case 7:
          strcpy(MCtext, "FEC 3/4");
          MERThresh = 4.1; //
        break;
        case 8:
          strcpy(MCtext, "FEC 4/5");
          MERThresh = 4.7; //
        break;
        case 9:
          strcpy(MCtext, "FEC 5/6");
          MERThresh = 5.2; //
        break;
        case 10:
          strcpy(MCtext, "FEC 8/9");
          MERThresh = 6.2; //
        break;
        case 11:
          strcpy(MCtext, "FEC 9/10");
          MERThresh = 6.5; //
        break;
        case 12:
          strcpy(MCtext, "FEC 3/5");
          MERThresh = 5.5; //
        break;
        case 13:
          strcpy(MCtext, "FEC 2/3");
          MERThresh = 6.6; //
        break;
        case 14:
          strcpy(MCtext, "FEC 3/4");
          MERThresh = 7.9; //
        break;
        case 15:
           strcpy(MCtext, "FEC 5/6");
           MERThresh = 9.4; //
        break;
        case 16:
          strcpy(MCtext, "FEC 8/9");
          MERThresh = 10.7; //
        break;
        case 17:
          strcpy(MCtext, "FEC 9/10");
          MERThresh = 11.0; //
        break;
        case 18:
          strcpy(MCtext, "FEC 2/3");
          MERThresh = 9.0; //
        break;
        case 19:
          strcpy(MCtext, "FEC 3/4");
          MERThresh = 10.2; //
        break;
        case 20:
          strcpy(MCtext, "FEC 4/5");
          MERThresh = 11.0; //
        break;
        case 21:
          strcpy(MCtext, "FEC 5/6");
          MERThresh = 11.6; //
        break;
        case 22:
          strcpy(MCtext, "FEC 8/9");
          MERThresh = 12.9; //
        break;
        case 23:
          strcpy(MCtext, "FEC 9/10");
          MERThresh = 13.2; //
        break;
        case 24:
           strcpy(MCtext, "FEC 3/4");
           MERThresh = 12.8; //
        break;
        case 25:
          strcpy(MCtext, "FEC 4/5");
          MERThresh = 13.7; //
        break;
        case 26:
          strcpy(MCtext, "FEC 5/6");
          MERThresh = 14.3; //
        break;
        case 27:
          strcpy(MCtext, "FEC 8/9");
          MERThresh = 15.7; //
        break;
        case 28:
          strcpy(MCtext, "FEC 9/10");
          MERThresh = 16.1; //
        break;
        default:
          strcpy(MCtext, "FEC -");
          MERThresh = 0; //
        break;
    }

    if ((MC >= 1) && (MC <= 11 ))
    {
      strcpy(Modtext, "QPSK");
    }
    else if ((MC >= 12) && (MC <= 17 ))
    {
      strcpy(Modtext, "8PSK");
    }
    else if ((MC >= 18) && (MC <= 23 ))
    {
      strcpy(Modtext, "16APSK");
    }
    else if ((MC >= 24) && (MC <= 28 ))
    {
      strcpy(Modtext, "32APSK");
    }
    else
    {
      strcpy(Modtext, " ");
    }
  }
  //printf("Mod: %s, FEC: %s, threshold %f\n", Modtext, MCtext, MERThresh);
  return MERThresh;
}

