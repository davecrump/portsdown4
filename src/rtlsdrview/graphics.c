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

//int64_t lo_frequency = 9750000;
int64_t center_frequency = 10489850000;
int64_t span_frequency = 512000;

int64_t selected_span_frequency = 10240;
int64_t selected_center_frequency = 10489499950;

/** Main Waterfall Display **/

#define MAIN_WATERFALL_WIDTH    512
#define MAIN_WATERFALL_HEIGHT   230
screen_pixel_t main_waterfall_buffer[MAIN_WATERFALL_HEIGHT][MAIN_WATERFALL_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));

#define MAIN_WATERFALL_POS_X    100
#define MAIN_WATERFALL_POS_Y    (SCREEN_HEIGHT - MAIN_WATERFALL_HEIGHT - 70)

/** Main Spectrum Display **/

#define MAIN_SPECTRUM_WIDTH     512
#define MAIN_SPECTRUM_HEIGHT    170
//screen_pixel_t main_spectrum_buffer[MAIN_SPECTRUM_HEIGHT][MAIN_SPECTRUM_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));
#define MAIN_SPECTRUM_TIME_SMOOTH   0.8f
float main_spectrum_smooth_buffer[MAIN_SPECTRUM_WIDTH] = { 0 };

//#define MAIN_SPECTRUM_POS_X     100
//#define MAIN_SPECTRUM_POS_Y     (SCREEN_HEIGHT - MAIN_WATERFALL_HEIGHT - MAIN_SPECTRUM_HEIGHT - 70)

/** Frequency Display **/

#define FREQUENCY_WIDTH         256
#define FREQUENCY_HEIGHT        43
screen_pixel_t frequency_buffer[FREQUENCY_HEIGHT][FREQUENCY_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));

#define FREQUENCY_POS_X         544
#define FREQUENCY_POS_Y         8

//////// New bits:

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


/** Main Spectrum Display **/


screen_pixel_t2 main_spectrum_buffer[MAIN_SPECTRUM_HEIGHT][MAIN_SPECTRUM_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));
//#define MAIN_SPECTRUM_TIME_SMOOTH   0.8f
//float main_spectrum_smooth_buffer[MAIN_SPECTRUM_WIDTH] = { 0 };

#define MAIN_SPECTRUM_POS_X     100
#define MAIN_SPECTRUM_POS_Y     (SCREEN_HEIGHT - MAIN_WATERFALL_HEIGHT - MAIN_SPECTRUM_HEIGHT - 70)






//////// End new Bits


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

static void waterfall_cm_websdr(screen_pixel_t *pixel_ptr, uint8_t value)
{
  /* Raspberry Pi Display starts flickering the backlight below a certain intensity, ensure that we don't go below this (~70) */
  if(value < 64)
  {
    pixel_ptr->Red = 0;
    pixel_ptr->Green = 0;
    pixel_ptr->Blue = 70 + (1.5 * value);
  }
  else if(value < 128)
  {
    pixel_ptr->Red = (3 * value) - 192;
    pixel_ptr->Green = 0;
    pixel_ptr->Blue = 70 + (1.5 * value);
  }
  else if(value < 192)
  {
    pixel_ptr->Red = value + 64;
    pixel_ptr->Green = 256 * sqrt((value - 128) / 64);
    pixel_ptr->Blue = 512 - (2 * value);
  }
  else
  {
    pixel_ptr->Red = 255;
    pixel_ptr->Green = 255;
    pixel_ptr->Blue = 512 - (2 * value);
  }
}

static void waterfall_generate(uint32_t counter, uint8_t *fft_data)
{
  screen_pixel_t new_pixel;
  new_pixel.Alpha = 0x80;

  for(uint32_t i = 0; i < MAIN_WATERFALL_WIDTH; i++)
  {
    /* Greyscale */
    //new_pixel.Red = fft_data[i];
    //new_pixel.Green = fft_data[i];
    //new_pixel.Blue = fft_data[i];

    /* websdr colour map */
    waterfall_cm_websdr(&new_pixel, fft_data[i]);

    memcpy(&(main_waterfall_buffer[counter][i]), &new_pixel, sizeof(screen_pixel_t));
  }
}

static void waterfall_render(uint32_t counter)
{
  for(uint32_t i = 0; i < MAIN_WATERFALL_HEIGHT; i++)
  {
    screen_setPixelLine(MAIN_WATERFALL_POS_X, MAIN_WATERFALL_POS_Y + i, MAIN_WATERFALL_WIDTH, main_waterfall_buffer[(i + counter) % MAIN_WATERFALL_HEIGHT]);
  }
}


static void spectrum_generate(uint8_t *fft_data)
{
//  const screen_pixel_t selected_marker_pixel =
//  {
//    .Alpha = 0x80,
//    .Red = 0x50,
//    .Green = 0x50,
//    .Blue = 0x50
//  };

//  const screen_pixel_t selected_band_pixel =
//  {
//    .Alpha = 0x80,
//    .Red = 0x1A,
//    .Green = 0x1A,
//    .Blue = 0x1A
//  };

  uint32_t value;
  uint32_t i, j;

  // Make the spectrum buffer all black

  for(i = 0; i < MAIN_SPECTRUM_HEIGHT; i++)
  {
    for(j = 0; j < MAIN_SPECTRUM_WIDTH; j++)
    {
      memcpy(&(main_spectrum_buffer[i][j]), &graphics_black_pixel, sizeof(screen_pixel_t));
    }
  }


  // Draw FFT Spectrum whites upward in the spectrum buffer
  for(i = 0; i < MAIN_SPECTRUM_WIDTH; i++)
  {
    main_spectrum_smooth_buffer[i] = (((float)fft_data[i]) * (1.f - MAIN_SPECTRUM_TIME_SMOOTH)) 
      + (main_spectrum_smooth_buffer[i] * MAIN_SPECTRUM_TIME_SMOOTH);
    value = ((uint32_t)(main_spectrum_smooth_buffer[i] * MAIN_SPECTRUM_HEIGHT)) / 255;

    for(j = (MAIN_SPECTRUM_HEIGHT-1); j > MAIN_SPECTRUM_HEIGHT-value-1; j--)
    {
      memcpy(&(main_spectrum_buffer[j][i]), &graphics_white_pixel, sizeof(screen_pixel_t));
    }
  }
}

static void spectrum_render(void)
{
  for(uint32_t i = 0; i < MAIN_SPECTRUM_HEIGHT; i++)
  {
    //screen_setPixelLine(MAIN_SPECTRUM_POS_X, MAIN_SPECTRUM_POS_Y + i, MAIN_SPECTRUM_WIDTH, main_spectrum_buffer[i]);
  }
}

//static void frequency_render_font_cb(int x, int y, screen_pixel_t *pixel_ptr)
//{
//  memcpy(&(frequency_buffer[y][x]), pixel_ptr, sizeof(screen_pixel_t));
//}

//static void frequency_generate(void)
//{
//  uint32_t i, j;
  /* Clear buffer */
//  for(i = 0; i < FREQUENCY_HEIGHT; i++)
//  {
//    for(j = 0; j <FREQUENCY_WIDTH; j++)
//    {
//      memcpy(&(frequency_buffer[i][j]), &graphics_black_pixel, sizeof(screen_pixel_t));
//    }
//  }

//  char *freq_string;
//  asprintf(&freq_string, ".%3lld.%03lld.%03lld", 
//    (selected_center_frequency / 1000000) % 1000,
//    (selected_center_frequency / 1000) % 1000,
//    selected_center_frequency % 1000);
//  font_render_string_with_callback(0, 0, &font_dejavu_sans_36, freq_string, frequency_render_font_cb);
//  free(freq_string);
//}

//static void frequency_render(void)
//{
  /* Render Frequency display buffer */
//  for(uint32_t i = 0; i < FREQUENCY_HEIGHT; i++)
//  {
//    screen_setPixelLine(FREQUENCY_POS_X, FREQUENCY_POS_Y + i, FREQUENCY_WIDTH, frequency_buffer[i]);
//  }
//}

//void graphics_frequency_newdata(void)
//{
//  frequency_generate();
//  frequency_render();
//}

static uint32_t main_waterfall_counter = (MAIN_WATERFALL_HEIGHT-1);
/* Takes 512byte FFT */
void waterfall_render_fft(uint8_t *fft_data)
{
#if 0
  for(uint32_t i = 0; i < MAIN_SPECTRUM_WIDTH; i++)
  {
    printf("%d,", fft_data[i]);
  }
  printf("\n");
#endif

  waterfall_generate(main_waterfall_counter, fft_data);
  spectrum_generate(fft_data);

  waterfall_render(main_waterfall_counter);
  spectrum_render();

  if(main_waterfall_counter-- == 0) main_waterfall_counter = (MAIN_WATERFALL_HEIGHT-1);
}


/////////////////// Code from Graphics.h here: //////////////////////////////////


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
