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

// Note that this is customised for an 800x480 Screen


uint32_t font_width_string(const font_t *font_ptr, char *string);
void displayChar2(const font_t *font_ptr, char c);
void displayChar3(const font_t *font_ptr, char c, int r, int g, int b);
void displayLargeChar2(int sizeFactor, const font_t *font_ptr, char c);
void TextMid2 (int xpos, int ypos, char*s, const font_t *font_ptr);
void TextMid3 (int xpos, int ypos, char*s, const font_t *font_ptr, int r, int g, int b);
void Text2 (int xpos, int ypos, char*s, const font_t *font_ptr);
void LargeText2 (int xpos, int ypos, int sizeFactor, char*s, const font_t *font_ptr);
void rectangle(int xpos, int ypos, int xsize, int ysize, int r, int g, int b);
void gotoXY(int x, int y);
void setForeColour(int R,int G,int B);
void setBackColour(int R,int G,int B);
void clearScreen();
void wipeScreen(int R, int G, int B);
void setPixel(int x, int y, int R, int G, int B);
void setPixelNoA(int x, int y, int R, int G, int B);
void setPixelNoABlk(int x, int y);
void setPixelNoAGra(int x, int y);
void MarkerGrn(int markerx, int x, int y);
void setLargePixel(int x, int y, int size, int R, int G, int B);
void closeScreen(void);
int initScreen(void);


void HorizLine(int xpos, int ypos, int xsize, int r, int g, int b);
void VertLine(int xpos, int ypos, int ysize, int r, int g, int b);
void DrawLine(int x1, int y1, int x2, int y2, int r, int g, int b);
void DrawAALine(int x1, int y1, int x2, int y2, int rb, int gb, int bb, int rf, int gf, int bf);

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

void displayChar3(const font_t *font_ptr, char c, int r, int g, int b)
{
  // Draws the character based on currentX and currentY at the bottom line (descenders below)
  // r g b define the background colour
  int row;
  int col;

  int32_t y_offset; // height of top of character from baseline

  int32_t thisPixelR;  // Values for each pixel
  int32_t thisPixelB;
  int32_t thisPixelG;

  // Calculate scale from background clour to foreground colour
  const int32_t red_contrast = foreColourR - r;
  const int32_t green_contrast = foreColourG - g;
  const int32_t blue_contrast = foreColourB - b;

  // Calculate height from top of character to baseline
  y_offset = font_ptr->ascent;

  // For each row
  for(row = 0; row < font_ptr->characters[(uint8_t)c].height; row++)
  {
    // For each column in the row
    for(col = 0; col < font_ptr->characters[(uint8_t)c].width; col++)
    {
      // For each pixel
      thisPixelR = r + ((red_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelG = g + ((green_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
      thisPixelB = b + ((blue_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[col+(row*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);

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

void TextMid3 (int xpos, int ypos, char*s, const font_t *font_ptr, int r, int g, int b)
{
  // Displays a text string with a defined background colour 
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
    displayChar3(font_ptr, c, r, g, b);
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


void DrawLine(int x1, int y1, int x2, int y2, int r, int g, int b)
{
  float yIntersect;
  float xIntersect;

  int x;
  int y;
  int p;     // Pixel Memory offset

  // Check the line bounds first
  if (((x1 < 0) || (x1 > screenXsize))
   || ((x2 < 0) || (x2 > screenXsize))
   || ((y1 < 0) || (y1 > screenXsize))
   || ((y2 < 0) || (y2 > screenXsize)))
  {
    printf("DrawLine x1 %d y1 %d x2 %d y2 %d tried to write off-screen\n",x1, y1, x2, y2);
  }
  else
  {
    //printf("DrawLine x1 %d y1 %d x2 %d y2 %d tried to write\n",x1, y1, x2, y2);

    if ((abs(x2 - x1) == abs(y2 - y1)) && (y2 - y1 != 0))  // 45 degree line
    {
      if (x1 < x2)  // 45 degree line on right hand side
      {
        //printf("45 RHS x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x <= x2; x++)
        {
          y = y1 + (x - x1) * (y2 - y1)/abs(y2 - y1);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      else  // 45 degree line on left hand side
      {
        //printf("45 LHS x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x >= x2; x--)
        {
          y = y1 + (x1 - x) * (y2 - y1)/abs(y2 - y1);
          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
    }
     
    if (abs(x2 - x1) > abs(y2 - y1))  // Less than 45 degree gradient
    {
      if (x2 > x1)  // Rightwards line
      {
        //printf("Rightwards < 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x <= x2; x++)
        {

          yIntersect = (float)y1 + (float)(y2 - y1) * (float)(x - x1) / (float)(x2 - x1);
          y = (int)(yIntersect + 0.5);
          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      else if (x1 > x2) // Leftwards line
      {
        //printf("Leftwards < 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x >= x2; x--)
        {
          yIntersect = (float)y1 + (float)(y2 - y1) * (float)(x - x1) / (float)(x2 - x1);
          y = (int)(yIntersect + 0.5);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      else //  x1 = x2 so vertical line

      {
         printf("need to draw vertical\n");
      }
    }


    if (abs(x2 - x1) < abs(y2 - y1))  // Nearer the vertical
    {
      if (y2 > y1)  // Upwards line
      {
        //printf("Upwards > 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (y = y1; y <= y2; y++)
        {
          xIntersect = (float)x1 + (float)(x2 - x1) * (float)(y - y1) / (float)(y2 - y1);
          x = (int)(xIntersect + 0.5);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      else  // Downwards line
      {
        //printf("Downwards > 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (y = y1; y >= y2; y--)
        {
          xIntersect = (float)x1 + (float)(x2 - x1) * (float)(y - y1) / (float)(y2 - y1);
          x = (int)(xIntersect + 0.5);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
    }
  }
}


void DrawAALine(int x1, int y1, int x2, int y2, int rb, int gb, int bb, int rf, int gf, int bf)
{
  float yIntersect;
  float xIntersect;
  float yIntersect_offset;
  float xIntersect_offset;
  int x;
  int y;
  int r = 0;
  int g = 0;
  int b = 0;
  int r_contrast;
  int g_contrast;
  int b_contrast;
  int p;     // Pixel Memory offset

  // Check the line bounds first
  if (((x1 < 0) || (x1 > screenXsize))
   || ((x2 < 0) || (x2 > screenXsize))
   || ((y1 < 1) || (y1 > screenXsize - 1))
   || ((y2 < 1) || (y2 > screenXsize - 1)))
  {
    printf("DrawLine x1 %d y1 %d x2 %d y2 %d tried to write off-screen\n",x1, y1, x2, y2);
  }
  else
  {
    //printf("DrawLine x1 %d y1 %d x2 %d y2 %d tried to write\n",x1, y1, x2, y2);

    if ((abs(x2 - x1) == abs(y2 - y1)) && (y2 - y1 != 0))  // 45 degree line
    {
      // No smoothing required, so
      r = rf;
      g = gf;
      b = bf;
      if (x1 < x2)  // 45 degree line on right hand side
      {
        //printf("45 RHS x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x <= x2; x++)
        {
          y = y1 + (x - x1) * (y2 - y1)/abs(y2 - y1);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      else  // 45 degree line on left hand side
      {
        //printf("45 LHS x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x >= x2; x--)
        {
          y = y1 + (x1 - x) * (y2 - y1)/abs(y2 - y1);
          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A
        }
      }
      return; // 45 degree line drawn.  Nothing else to do
    }
     
    // Calculate contrast
    r_contrast = rf - rb;
    g_contrast = gf - gb;
    b_contrast = bf - bb;

    if (abs(x2 - x1) > abs(y2 - y1))  // Less than 45 degree gradient
    {
      if (x2 > x1)  // Rightwards line
      {
        //printf("Rightwards < 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x <= x2; x++)
        {

          yIntersect = (float)y1 + (float)(y2 - y1) * (float)(x - x1) / (float)(x2 - x1);
          y = (int)yIntersect;
          yIntersect_offset = yIntersect - (float)y;

          r = rf + (int)((1.0 - yIntersect_offset) * (float)r_contrast);
          g = gf + (int)((1.0 - yIntersect_offset) * (float)g_contrast);
          b = bf + (int)((1.0 - yIntersect_offset) * (float)b_contrast);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A

          if (yIntersect_offset > 0.004)
          {
            r = rf + (int)(yIntersect_offset * (float)r_contrast);
            g = gf + (int)(yIntersect_offset * (float)g_contrast);
            b = bf + (int)(yIntersect_offset * (float)b_contrast);

            y = y + 1;
            p = (x + screenXsize * (479 - y)) * 4;

            memset(p + fbp,     b, 1);     // Blue
            memset(p + fbp + 1, g, 1);     // Green
            memset(p + fbp + 2, r, 1);     // Red
            memset(p + fbp + 3, 0x80, 1);  // A
          }
        }

      }
      else if (x1 > x2) // Leftwards line
      {
        //printf("Leftwards < 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (x = x1; x >= x2; x--)
        {
          yIntersect = (float)y1 + (float)(y2 - y1) * (float)(x - x1) / (float)(x2 - x1);
          y = (int)yIntersect;
          yIntersect_offset = yIntersect - (float)y;

          r = rf + (int)((1.0 - yIntersect_offset) * (float)r_contrast);
          g = gf + (int)((1.0 - yIntersect_offset) * (float)g_contrast);
          b = bf + (int)((1.0 - yIntersect_offset) * (float)b_contrast);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A

          if (yIntersect_offset > 0.004)
          {
            r = rf + (int)(yIntersect_offset * (float)r_contrast);
            g = gf + (int)(yIntersect_offset * (float)g_contrast);
            b = bf + (int)(yIntersect_offset * (float)b_contrast);

            y = y + 1;
            p = (x + screenXsize * (479 - y)) * 4;

            memset(p + fbp,     b, 1);     // Blue
            memset(p + fbp + 1, g, 1);     // Green
            memset(p + fbp + 2, r, 1);     // Red
            memset(p + fbp + 3, 0x80, 1);  // A
          }
        }
      }
      else //  x1 = x2 so vertical line

      {
         printf("need to draw vertical\n");
      }
    }


    if (abs(x2 - x1) < abs(y2 - y1))  // Nearer the vertical
    {
      if (y2 > y1)  // Upwards line
      {
        //printf("Upwards > 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (y = y1; y <= y2; y++)
        {
          xIntersect = (float)x1 + (float)(x2 - x1) * (float)(y - y1) / (float)(y2 - y1);
          x = (int)xIntersect;
          xIntersect_offset = xIntersect - (float)x;

          r = rf + (int)((1.0 - xIntersect_offset) * (float)r_contrast);
          g = gf + (int)((1.0 - xIntersect_offset) * (float)g_contrast);
          b = bf + (int)((1.0 - xIntersect_offset) * (float)b_contrast);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A

          if (xIntersect_offset > 0.004)
          {
            r = rf + (int)(xIntersect_offset * (float)r_contrast);
            g = gf + (int)(xIntersect_offset * (float)g_contrast);
            b = bf + (int)(xIntersect_offset * (float)b_contrast);

            x = x + 1;
            p = (x + screenXsize * (479 - y)) * 4;

            memset(p + fbp,     b, 1);     // Blue
            memset(p + fbp + 1, g, 1);     // Green
            memset(p + fbp + 2, r, 1);     // Red
            memset(p + fbp + 3, 0x80, 1);  // A
          }
        }
      }
      else  // Downwards line
      {
        //printf("Downwards > 45 x %d y %d\n", x2 - x1, y2 - y1);
        for (y = y1; y >= y2; y--)
        {
          xIntersect = (float)x1 + (float)(x2 - x1) * (float)(y - y1) / (float)(y2 - y1);
          x = (int)xIntersect;
          xIntersect_offset = xIntersect - (float)x;

          r = rf + (int)((1.0 - xIntersect_offset) * (float)r_contrast);
          g = gf + (int)((1.0 - xIntersect_offset) * (float)g_contrast);
          b = bf + (int)((1.0 - xIntersect_offset) * (float)b_contrast);

          p = (x + screenXsize * (479 - y)) * 4;

          memset(p + fbp,     b, 1);     // Blue
          memset(p + fbp + 1, g, 1);     // Green
          memset(p + fbp + 2, r, 1);     // Red
          memset(p + fbp + 3, 0x80, 1);  // A

          if (xIntersect_offset > 0.004)
          {
            r = rf + (int)(xIntersect_offset * (float)r_contrast);
            g = gf + (int)(xIntersect_offset * (float)g_contrast);
            b = bf + (int)(xIntersect_offset * (float)b_contrast);

            x = x + 1;
            p = (x + screenXsize * (479 - y)) * 4;

            memset(p + fbp,     b, 1);     // Blue
            memset(p + fbp + 1, g, 1);     // Green
            memset(p + fbp + 2, r, 1);     // Red
            memset(p + fbp + 3, 0x80, 1);  // A
          }
        }
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

void wipeScreen(int R, int G, int B)
{
  int p;  // Pixel Memory offset
  int x;  // x pixel count, 0 - 799
  int y;  // y pixel count, 0 - 479

  for(y=0; y < screenYsize; y++)
  {
    for(x=0; x < screenXsize; x++)
    {
      p=(x + screenXsize * y) * 4;

      memset(fbp + p,     B,    1);     // Blue
      memset(fbp + p + 1, G,    1);     // Green
      memset(fbp + p + 2, R,    1);     // Red
      memset(fbp + p + 3, 0x80, 1);     // A
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