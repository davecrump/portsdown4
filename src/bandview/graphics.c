#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "screen.h"
#include "font/font.h"
#include "graphics.h"


#define NEON_ALIGNMENT (4*4*2) // From libcsdr


char *fbp = 0;
int fbfd = 0;
long int screenSize = 0;
int screenXsize=800;
int screenYsize=480;
int currentX=0;
int currentY=0;
int textSize=1;
int foreColourR=0;
int foreColourG=0;
int foreColourB=0;
int backColourR=0;
int backColourG=0;
int backColourB=0;

typedef struct {
  uint8_t Blue;
  uint8_t Green;
  uint8_t Red;
  uint8_t Alpha; // 0x80
} __attribute__((__packed__)) screen_pixel_t2;

uint8_t waterfall_newcolour[256][3] = 
{
{0,27,32},
{0,29,37},
{0,29,39},
{0,30,40},
{0,31,41},
{0,32,42},
{0,33,43},
{0,34,46},
{0,35,48},
{0,35,49},
{0,36,52},
{0,37,53},
{0,38,55},
{0,38,57},
{0,40,59},
{0,41,61},
{0,42,64},
{0,42,65},
{0,43,68},
{0,43,70},
{0,44,72},
{0,45,74},
{0,46,76},
{0,46,79},
{0,47,81},
{0,47,84},
{0,48,86},
{0,48,88},
{2,49,91},
{4,50,91},
{6,50,95},
{9,50,98},
{11,49,100},
{13,50,102},
{17,50,104},
{19,51,107},
{21,52,109},
{24,52,112},
{28,52,116},
{29,51,118},
{32,53,120},
{33,52,121},
{35,52,124},
{39,53,126},
{40,52,131},
{42,52,132},
{45,52,133},
{47,52,134},
{50,52,137},
{51,53,139},
{55,52,142},
{57,51,144},
{59,50,146},
{61,51,148},
{64,51,150},
{67,50,153},
{69,50,155},
{70,50,155},
{75,49,157},
{76,48,159},
{79,49,160},
{81,48,161},
{83,48,164},
{85,47,166},
{87,46,166},
{90,46,167},
{92,45,168},
{96,46,171},
{97,45,172},
{99,43,172},
{102,43,172},
{104,43,173},
{107,42,175},
{110,42,177},
{112,42,177},
{113,41,177},
{114,40,177},
{117,40,178},
{119,39,178},
{122,39,179},
{124,39,179},
{126,39,178},
{129,38,178},
{130,37,178},
{133,37,179},
{134,37,178},
{137,37,179},
{139,36,179},
{141,36,180},
{142,36,178},
{145,36,179},
{147,36,177},
{148,36,178},
{152,35,177},
{153,35,176},
{155,36,176},
{158,35,175},
{159,36,173},
{161,36,172},
{162,36,171},
{164,36,171},
{166,37,169},
{168,37,167},
{170,37,167},
{171,38,166},
{172,38,166},
{175,39,164},
{177,40,162},
{179,40,161},
{180,41,159},
{181,42,157},
{183,42,156},
{185,43,156},
{187,43,155},
{190,44,153},
{192,46,152},
{193,46,150},
{193,47,147},
{194,47,146},
{197,49,145},
{199,49,143},
{200,50,141},
{201,51,140},
{202,52,138},
{204,53,136},
{206,55,136},
{208,55,134},
{209,57,132},
{210,59,130},
{212,59,129},
{213,59,128},
{214,61,126},
{215,63,124},
{217,65,122},
{218,65,121},
{219,66,120},
{220,67,117},
{221,68,112},
{222,70,111},
{225,71,111},
{226,72,110},
{227,74,108},
{228,75,106},
{229,77,104},
{230,79,102},
{231,80,99},
{232,81,98},
{233,82,95},
{234,83,94},
{235,84,93},
{236,85,90},
{237,87,89},
{238,88,89},
{239,89,88},
{240,91,84},
{241,93,82},
{242,94,81},
{243,95,80},
{243,96,78},
{244,97,77},
{245,99,74},
{246,101,72},
{247,104,70},
{248,106,69},
{249,107,67},
{250,109,66},
{250,109,64},
{250,111,62},
{251,113,60},
{252,115,58},
{252,116,56},
{252,118,55},
{253,120,53},
{254,123,51},
{255,125,50},
{255,126,49},
{255,128,47},
{255,129,46},
{255,130,44},
{255,133,42},
{255,135,41},
{255,138,40},
{255,139,36},
{255,140,36},
{255,142,36},
{255,144,36},
{255,146,35},
{255,150,35},
{255,152,34},
{253,152,34},
{254,153,35},
{253,156,35},
{253,161,36},
{252,160,37},
{251,164,38},
{250,165,39},
{250,168,40},
{249,170,42},
{248,173,44},
{247,174,46},
{247,176,50},
{246,178,53},
{245,179,56},
{245,182,58},
{244,186,60},
{243,187,64},
{241,189,67},
{240,190,69},
{240,192,73},
{239,194,75},
{237,196,78},
{236,199,82},
{234,201,85},
{235,203,92},
{235,205,93},
{233,207,97},
{231,208,92},
{231,211,106},
{230,212,112},
{229,213,115},
{228,216,120},
{228,216,124},
{227,218,127},
{226,220,132},
{226,221,137},
{224,221,140},
{224,222,145},
{224,224,150},
{225,226,156},
{225,228,159},
{224,231,164},
{223,232,167},
{223,233,171},
{225,234,177},
{227,235,183},
{227,236,186},
{228,237,190},
{229,238,195},
{230,238,197},
{231,239,203},
{233,240,207},
{234,241,210},
{236,242,216},
{237,242,220},
{238,243,223},
{240,245,225},
{241,245,230},
{242,246,232},
{245,247,236},
{246,247,239},
{247,248,243},
{248,248,245},
{251,250,248},
{253,252,252},
{255,255,255},
{255,255,255}
};

const screen_pixel_t graphics_white_pixel =
{
  .Alpha = 0x80,
  .Red = 0xFF,
  .Green = 0xFF,
  .Blue = 0xFF
};

const screen_pixel_t graphics_black_pixel =
{
  .Alpha = 0x80,
  .Red = 0x00,
  .Green = 0x00,
  .Blue = 0x00
};

const screen_pixel_t graphics_red_pixel =
{
  .Alpha = 0x80,
  .Red = 0xFF,
  .Green = 0x00,
  .Blue = 0x00
};


screen_pixel_t waterfall_map(uint8_t value)
{
  screen_pixel_t result;

  result.Alpha = 0x80;
  result.Red = waterfall_newcolour[value][0];
  result.Green = waterfall_newcolour[value][1];
  result.Blue = waterfall_newcolour[value][2];

  return result;
}

uint32_t font_width_string(const font_t *font_ptr, char *string)
{
    uint32_t total_width = 0;
    uint32_t string_length = strlen(string);
    for(uint32_t i = 0; i < string_length; i++)
    {
        total_width += font_ptr->characters[(uint8_t)string[i]].render_width;
    }
    return total_width;
}

void displayChar2(const font_t *font_ptr, char c)
{
  // Draws the character based on currentX and currentY at the bottom line (descenders below)
  int row;
  int col;

  int32_t y_offset; // height of top of character from baseline

  int32_t thisPixelR;  // Values for each pixel
  int32_t thisPixelB;
  int32_t thisPixelG;

  // Calculate scale from background clour to foreground colour
  const int32_t red_contrast = foreColourR - backColourR;
  const int32_t green_contrast = foreColourG - backColourG;
  const int32_t blue_contrast = foreColourB - backColourB;

  // Calculate height from top of character to baseline
  y_offset = font_ptr->ascent;

  // For each row
  for(row = 0; row < font_ptr->characters[(uint8_t)c].height; row++)
  {
    // For each column in the row
    for(col = 0; col < font_ptr->characters[(uint8_t)c].width; col++)
    {
      // For each pixel
      thisPixelR = backColourR + ((red_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelG = backColourG + ((green_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelB = backColourB + ((blue_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);

      if ((currentX + col < 800) && (currentY + row - y_offset < 480) && (currentX + col >= 0) && (currentY + row - y_offset >= 0))
      {
         setPixel(currentX + col, currentY + row - y_offset, thisPixelR, thisPixelG, thisPixelB);
      }
      else
      {
        printf("Error: Trying to write pixel outside screen bounds.\n");
      }
    }
  }
  // Move position on by the character width
  currentX = currentX + font_ptr->characters[(uint8_t)c].render_width;
}

void displayLargeChar2(int sizeFactor, const font_t *font_ptr, char c)
{
  // Draws the character based on currentX and currentY at the bottom line (descenders below)
  // magnified by sizeFactor
  int row;
  int col;
  int extrarow;
  int extracol;
  int pixelX;
  int pixelY;

  int32_t y_offset; // height of top of character from baseline

  int32_t thisPixelR;  // Values for each pixel
  int32_t thisPixelB;
  int32_t thisPixelG;

  // Calculate scale from background colour to foreground colour
  const int32_t red_contrast = foreColourR - backColourR;
  const int32_t green_contrast = foreColourG - backColourG;
  const int32_t blue_contrast = foreColourB - backColourB;

  // Calculate height from top of character to baseline
  y_offset = font_ptr->ascent * sizeFactor;

  // For each row
  for(row = 0; row < font_ptr->characters[(uint8_t)c].height; row++)
  {
    // For each column in the row
    for(col = 0; col < font_ptr->characters[(uint8_t)c].width; col++)
    {
      // For each pixel
      thisPixelR = backColourR + ((red_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelG = backColourG + ((green_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelB = backColourB + ((blue_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);

      for(extrarow = 0; extrarow < sizeFactor; extrarow++)
      {
        for(extracol = 0; extracol < sizeFactor; extracol++)
        {
          pixelX = currentX + (col * sizeFactor) + extracol;
          pixelY = currentY + (row * sizeFactor) - y_offset + extrarow;
          if ((pixelX < 800) && (pixelY < 480) && (pixelX >= 0) && (pixelY >= 0))
          {
             setPixel(pixelX, pixelY, thisPixelR, thisPixelG, thisPixelB);
          }
          else
          {
            printf("Error: Trying to write pixel outside screen bounds.\n");
          }
        }
      }
    }
  }
  // Move position on by the character width
  currentX = currentX + (font_ptr->characters[(uint8_t)c].render_width * sizeFactor);
}


void TextMid2 (int xpos, int ypos, char*s, const font_t *font_ptr)
{
  int sx;   // String width in pixels
  int p;    // Character Counter
  p=0;

  // Calculate String Length and position string write position start
  sx = (font_width_string(font_ptr, s)) / 2;  
  gotoXY(xpos - sx, 480 - ypos);

  //printf("TextMid sx %d, x %d, y %d, %s\n", sx, xpos, ypos, s);

  // Display each character
  do
  {
    char c=s[p++];
    displayChar2(font_ptr, c);
  }
  while(s[p] != 0);  // While not end of string
}

void Text2 (int xpos, int ypos, char*s, const font_t *font_ptr)
{
  int p;    // Character Counter
  p=0;

  // Position string write position start

  gotoXY(xpos, 480 - ypos);

  //printf("TextMid x %d, y %d, %s\n", xpos, ypos, s);

  // Display each character
  do
  {
    char c=s[p++];
    displayChar2(font_ptr, c);
  }
  while(s[p] != 0);  // While not end of string
}

void LargeText2 (int xpos, int ypos, int sizeFactor, char*s, const font_t *font_ptr)
{
  int p;    // Character Counter
  p=0;

  // Position string write position start

  gotoXY(xpos, 480 - ypos);

  //printf("TextMid x %d, y %d, %s\n", xpos, ypos, s);

  // Display each character
  do
  {
    char c=s[p++];
    //displayChar2(font_ptr, c);
    displayLargeChar2(sizeFactor, font_ptr, c);
  }
  while(s[p] != 0);  // While not end of string
}


void rectangle(int xpos, int ypos, int xsize, int ysize, int r, int g, int b)
{
  // xpos and y pos are 0 to screensize (479 799).  (0, 0) is bottom left
  // xsize and ysize are 1 to 800 (x) or 480 (y)
  int x;     // pixel count
  int y;     // pixel count
  int p;     // Pixel Memory offset

  // Check rectangle bounds to save checking each pixel

  if (((xpos < 0) || (xpos > screenXsize))
    || ((xpos + xsize < 0) || (xpos + xsize > screenXsize))
    || ((ypos < 0) || (ypos > screenYsize))
    || ((ypos + ysize < 0) || (ypos + ysize > screenYsize)))
  {
    printf("Rectangle xpos %d xsize %d ypos %d ysize %d tried to write off-screen\n", xpos, xsize, ypos, ysize);
  }
  else
  {
    for(x = 0; x < xsize; x++)  // for each vertical slice along the x
    {
      for(y = 0; y < ysize; y++)  // Draw a vertical line
      {
        p = (xpos + x + screenXsize * (479 - (ypos + y))) * 4;

        memset(p + fbp,     b, 1);     // Blue
        memset(p + fbp + 1, g, 1);     // Green
        memset(p + fbp + 2, r, 1);     // Red
        memset(p + fbp + 3, 0x80, 1);  // A
      }
    }
  }
}

void HorizLine(int xpos, int ypos, int xsize, int r, int g, int b)
{
  // xpos and y pos are 0 to screensize (479 799).  (0, 0) is bottom left
  // xsize is 1 to 800
  int x;     // pixel count
  int p;     // Pixel Memory offset

  // Check rectangle bounds to save checking each pixel

  if (((xpos < 0) || (xpos > screenXsize))
    || ((xpos + xsize < 0) || (xpos + xsize > screenXsize))
    || ((ypos < 0) || (ypos > screenYsize)))
  {
    printf("HorizLine xpos %d xsize %d ypos %d tried to write off-screen\n", xpos, xsize, ypos);
  }
  else
  {
    for(x = 0; x < xsize; x++)  // for each pixel along the x
    {
      // Draw a pixel
      p = (xpos + x + screenXsize * (479 - (ypos))) * 4;
      memset(p + fbp,     b, 1);     // Blue
      memset(p + fbp + 1, g, 1);     // Green
      memset(p + fbp + 2, r, 1);     // Red
      memset(p + fbp + 3, 0x80, 1);  // A
    }
  }
}


void VertLine(int xpos, int ypos, int ysize, int r, int g, int b)
{
  // xpos and y pos are 0 to screensize (479 799).  (0, 0) is bottom left
  // ysize is 1 to 480 
  int y;     // pixel count
  int p;     // Pixel Memory offset

  // Check rectangle bounds to save checking each pixel

  if (((xpos < 0) || (xpos > screenXsize))
    || ((ypos < 0) || (ypos > screenYsize))
    || ((ypos + ysize < 0) || (ypos + ysize > screenYsize)))
  {
    printf("VertLine xpos %d ypos %d ysize %d tried to write off-screen\n", xpos, ypos, ysize);
  }
  else
  {
      for(y = 0; y < ysize; y++)  // Draw a vertical line
      {
        p = (xpos + screenXsize * (479 - (ypos + y))) * 4;

        memset(p + fbp,     b, 1);     // Blue
        memset(p + fbp + 1, g, 1);     // Green
        memset(p + fbp + 2, r, 1);     // Red
        memset(p + fbp + 3, 0x80, 1);  // A
      }
  }
}


void gotoXY(int x, int y)
{
  currentX = x;
  currentY = y;
}

void setForeColour(int R,int G,int B)
{
  foreColourR = R;
  foreColourG = G;
  foreColourB = B;
}

void setBackColour(int R,int G,int B)
{
  backColourR = R;
  backColourG = G;
  backColourB = B;
}


void clearScreen()
{
  int p;  // Pixel Memory offset
  int x;  // x pixel count, 0 - 799
  int y;  // y pixel count, 0 - 479

  for(y=0; y < screenYsize; y++)
  {
    for(x=0; x < screenXsize; x++)
    {
      p=(x + screenXsize * y) * 4;

      memset(fbp + p,     backColourB, 1);     // Blue
      memset(fbp + p + 1, backColourG, 1);     // Green
      memset(fbp + p + 2, backColourR, 1);     // Red
      memset(fbp + p + 3, 0x80,        1);     // A
    }
  }
}

void setPixel(int x, int y, int R, int G, int B)
{
  int p;  // Pixel Memory offset
  if ((x < 800) && (y < 480))
  {
    p=(x + screenXsize * y) * 4;

    memset(fbp + p, B, 1);         // Blue
    memset(fbp + p + 1, G, 1);     // Green
    memset(fbp + p + 2, R, 1);     // Red
    memset(fbp + p + 3, 0x80, 1);  // A
  }
  else
  {
    printf("Error: Trying to write pixel outside screen bounds.\n");
  }
}

void setPixelNoA(int x, int y, int R, int G, int B)
{
  int p;  // Pixel Memory offset

  p=(x + screenXsize * y) * 4;

  memset(fbp + p, B, 1);         // Blue
  memset(fbp + p + 1, G, 1);     // Green
  memset(fbp + p + 2, R, 1);     // Red

}

void setPixelNoABlk(int x, int y)
{
  int p;  // Pixel Memory offset

  p=(x + screenXsize * y) * 4;

  memset(fbp + p, 0, 3);         // BGR = 0
}

void setPixelNoAGra(int x, int y)
{
  int p;   // Pixel Memory offset

  p=(x + screenXsize * y) * 4;

  memset(fbp + p, 64, 3);         // BGR = 64

}

void MarkerGrn(int markerx, int x, int y)
{
  int p;  // Pixel Memory offset
  int row; // Marker Row
  int disp; // displacement from marker#

  disp = x - markerx;
  if ((disp > -11 ) && (disp < 11))
  {
    if (disp <= 0)  // left of marker
    {
      for ( row = (11 + disp); row > 0; row--)
      {
        if ((y - 11 + row) > 10)  // on screen
        {
          p = (x + screenXsize * (y - 11 + row)) * 4;
          memset(fbp + p + 1, 255, 1);
        }
      }
    }
    else // right of marker
    {
      for ( row = 1; row < (12 - disp); row++)
      {
        if ((y - 11 + row) > 10)  // on screen
        {
          p = (x + screenXsize * (y - 11 + row)) * 4;
          memset(fbp + p + 1, 255, 1);
        }
      }
    }
  }
}


void setLargePixel(int x, int y, int size, int R, int G, int B)
{
  int px; // Horizontal pixel counter within large pixel
  int py; // Vertical pixel counter within large pixel

  for (px = 0; px < size; px++)
  {
    for (py=0; py < size; py++)
    {
      setPixel(x + px, y + py, R, G, B);
    }
  }
}

void closeScreen(void)
{
  munmap(fbp, screenSize);
  close(fbfd);
}


int initScreen(void)
{

  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
 
  fbfd = open("/dev/fb0", O_RDWR);
  if (!fbfd) 
  {
    printf("Error: cannot open framebuffer device.\n");
    return(0);
  }

  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) 
  {
    printf("Error reading fixed information.\n");
    return(0);
  }

  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) 
  {
    printf("Error reading variable information.\n");
    return(0);
  }
  
  screenXsize=vinfo.xres;
  screenYsize=vinfo.yres;
  
  screenSize = finfo.smem_len;
  fbp = (char*)mmap(0, screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
                    
  if ((int)fbp == -1) 
  {
    return 0;
  }
  else 
  {
    return 1;
  }
}
