#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include "font/font.h"


char *fbp = 0;
int fbfd = 0;
long int screenSize = 0;
int screenXsize=0;
int screenYsize=0;
int currentX=0;
int currentY=0;
int textSize=1;
int foreColourR=0;
int foreColourG=0;
int foreColourB=0;
int backColourR=0;
int backColourG=0;
int backColourB=0;



uint32_t font_width_string(const font_t *font_ptr, char *string);
void displayChar2(const font_t *font_ptr, char c);
void displayLargeChar2(int sizeFactor, const font_t *font_ptr, char c);
void TextMid2 (int xpos, int ypos, char*s, const font_t *font_ptr);
void Text2 (int xpos, int ypos, char*s, const font_t *font_ptr);
void LargeText2 (int xpos, int ypos, int sizeFactor, char*s, const font_t *font_ptr);
void rectangle(int xpos, int ypos, int xsize, int ysize, int r, int g, int b);
void gotoXY(int x, int y);
void setForeColour(int R,int G,int B);
void setBackColour(int R,int G,int B);
void clearScreen();
void setPixel(int x, int y, int R, int G, int B);
void setLargePixel(int x, int y, int size, int R, int G, int B);
void closeScreen(void);
int initScreen(void);


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

      if ((currentX + col < 800) && (currentY + row - y_offset < 480))
      {
         setPixel(currentX + col, currentY + row - y_offset, thisPixelR, thisPixelG, thisPixelB);
      }
      //else
      //{
      //  printf("Error: Trying to write pixel outside screen bounds.\n");
      //}
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
          if ((pixelX < 800) && (pixelY < 480))
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
  char c;

  // Position string write position start

  gotoXY(xpos, 480 - ypos);

  //printf("TextMid x %d, y %d, %s\n", xpos, ypos, s);

  // Display each character
  for (p = 0; p < strlen(s); p++)
  {
    c = s[p];
    displayChar2(font_ptr, c);
  }
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