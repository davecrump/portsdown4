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
