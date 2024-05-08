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
char *mfbp = 0;
char *cfbp = 0;
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

extern bool mouse_connected;

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
void setCursorPixel(int x, int y, int level);
void draw_cursor2(int new_x, int new_y);
void refreshMouseBackground(void);
void draw_cursor_foreground(int x, int y);
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

void setCursorPixel(int x, int y, int level)
{
  int p;  // Pixel Memory offset
  if ((x < 800) && (y < 480))
  {
    p=(x + screenXsize * y) * 4;

    memset(cfbp + p, level, 1);         // Blue
    memset(cfbp + p + 1, level, 1);     // Green
    memset(cfbp + p + 2, level, 1);     // Red
    memset(cfbp + p + 3, 0x80, 1);  // A
  }
  else
  {
    printf("Error: Trying to write pixel outside screen bounds.\n");
  }
}


int cursorLine0[3] = {110, 38, 124}; 
int cursorLine1[5] = {143, 36, 155, 22, 155}; 
int cursorLine2[5] = {82, 127, 255, 96, 110}; 
int cursorLine3[5] = {84, 128, 255, 100, 112};
int cursorLine4[5] = {85, 128, 255, 99, 116};
int cursorLine5[6] = {85, 127, 255, 103, 88, 216};
int cursorLine6[9] = {85, 127, 255, 114, 9, 15, 57, 212, 236};
int cursorLine7[12] = {85, 127, 255, 101, 104, 212, 80, 12, 35, 74, 197, 237};
int cursorLine8[14] = {85, 127, 255, 99, 15, 255, 180, 49, 207, 65, 6, 33, 76, 222};
int cursorLine9[17] = {231, 223, 255, 88, 127, 255, 97, 110, 255, 174, 52, 255, 190, 41, 208, 91, 61};
int cursorLine10[18] = {170, 32, 28, 100, 70, 131, 255, 105, 117, 255, 179, 64, 255, 186, 45, 255, 221, 38};
int cursorLine11[18] = {39, 148, 210, 47, 0, 141, 255, 234, 255, 255, 245, 230, 255, 187, 54, 255, 220, 42};
int cursorLine12[18] = {47, 145, 255, 207, 0, 137, 255, 255, 255, 255, 255, 255, 255, 242, 223, 255, 218, 42};
int cursorLine13[18] = {187,  13, 165, 255,  63, 132, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 213,  41};
int cursorLine14[17] = {     144,  14, 199, 231, 229, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 204,  43};
int cursorLine15[16] = {          110, 249, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 185,  54};
int cursorLine16[16] = {          245,  59,  99, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 144,  77};
int cursorLine17[15] = {               189,  11, 221, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  83, 124};
int cursorLine18[14] = {                     80, 101, 255, 255, 255, 255, 255, 255, 255, 255, 255, 237,  31, 199};
int cursorLine19[14] = {                    181,  39, 246, 255, 255, 255, 255, 255, 255, 255, 255, 170,  28, 247};
int cursorLine20[13] = {                    229,  30, 213, 255, 255, 255, 255, 255, 255, 255, 255, 103,  82};
int cursorLine21[13] = {                    254,  50, 167, 255, 255, 255, 255, 255, 255, 255, 253,  44, 165};
int cursorLine22[12] = {                          78, 121, 236, 232, 232, 232, 232, 232, 235, 199,  31, 219};
int cursorLine23[12] = {                         116,  24,  38,  37,  36,  36,  37,  36,  37,  29,  59, 247};


void draw_cursor2(int new_x, int new_y)
{
  int vert;
  int horiz;
  int xpixel = 0;
  int ypixel = 0;
  static int old_x;
  static int old_y;

  int cursor_height = 24;
  int cursor_width = 18;
  int cursor_spot_x = 6;

  int cursor_top; 
  int cursor_base; 
  int cursor_left; 
  int cursor_right;

  int box_top; 
  int box_base; 
  int box_left; 
  int box_right;

  int delta_x;
  int delta_y;

  int x_start;
  int x_width;
  int y_start;
  int y_height;

  // work out the area required to be copied across between framebuffers
  // mouse coords start at 0 bottom left
  // Do calcs with this reference and then correct at the end

  // New cursor area
  cursor_top = new_y;
  cursor_base = new_y - cursor_height;
  cursor_left = new_x - cursor_spot_x;
  cursor_right = cursor_left + cursor_width;

  // Calculate movement
  delta_x = new_x - old_x;
  delta_y = new_y - old_y;

  // calculate bounding box for old and new
  if (delta_x > 0) // mouse has moved right
  {
    box_left = cursor_left - delta_x;
    box_right = cursor_right;
  }
  else if (delta_x < 0)  // mouse has moved left
  {
    box_left = cursor_left;
    box_right = cursor_right - delta_x;
  }
  else                  // mouse has not moved in x
  {
    box_left = cursor_left;
    box_right = cursor_right;
  }

  if (delta_y > 0) // mouse has moved up
  {
    box_top = cursor_top;
    box_base = cursor_base - delta_y;
  }
  else if (delta_y < 0)  // mouse has moved down
  {
    box_top = cursor_top - delta_y;
    box_base = cursor_base;
  }
  else                  // mouse has not moved in y
  {
    box_top = cursor_top;
    box_base = cursor_base;
  }

  // Constrain to touchscreen
  if (box_left > 799)
  {
    box_left = 799;
  }
  if (box_left < 0)
  {
    box_left = 0;
  }
  if (box_right > 799)
  {
    box_right = 799;
  }
  if (box_right < 0)
  {
    box_right = 0;
  }

  if (box_top > 479)
  {
    box_top = 479;
  }
  if (box_top < 0)
  {
    box_top = 0;
  }
  if (box_base > 479)
  {
    box_base = 479;
  }
  if (box_base < 0)
  {
    box_base = 0;
  }

  // Calulate memory positions:
  x_start = 4 * box_left;
  x_width = 4 * (box_right - box_left);
  y_start = 479 - box_top;
  y_height = box_top - box_base;

  //printf ("oldx = %d, oldy = %d, newx = %d, newy = %d left = %d, right = %d, top = %d, base = %d\n", old_x, old_y, new_x, new_y, box_left, box_right, box_top, box_base);

  for(int cursor_redraw_line = y_start; cursor_redraw_line <= (y_start + y_height); cursor_redraw_line++)
  {
    // copy the mouse clean background buffer into the cursor buffer 
    memcpy(cfbp + x_start + (cursor_redraw_line * screenXsize * 4), mfbp + x_start + (cursor_redraw_line * screenXsize * 4), x_width);
  }

  //memcpy(cfbp, mfbp, screenSize);

  for (vert = 0; vert < 24; vert++)
  {
    ypixel = new_y - 24 + vert;
    if ((ypixel >= 0) && (ypixel < 480))
    {

      for (horiz = 0; horiz < 18; horiz++)
      {
        xpixel = new_x - 6  + horiz;
        if ((xpixel >= 0) && (xpixel < 800))
        {
          if ((vert == 23) && (horiz >= 5) && (horiz < 8))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine0[horiz - 5]);
          }
          else if ((vert == 22) && (horiz >= 4) && (horiz < 9))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine1[horiz - 4]);
          }
          else if ((vert == 21) && (horiz >= 4) && (horiz < 9))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine2[horiz - 4]);
          }
          else if ((vert == 20) && (horiz >= 4) && (horiz < 9))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine3[horiz - 4]);
          }
          else if ((vert == 19) && (horiz >= 4) && (horiz < 9))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine4[horiz - 4]);
          }
          else if ((vert == 18) && (horiz >= 4) && (horiz < 10))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine5[horiz - 4]);
          }
          else if ((vert == 17) && (horiz >= 4) && (horiz < 13))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine6[horiz - 4]);
          }
          else if ((vert == 16) && (horiz >= 4) && (horiz < 16))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine7[horiz - 4]);
          }
          else if ((vert == 15) && (horiz >= 4) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine8[horiz - 4]);
          }
          else if ((vert == 14) && (horiz >= 1) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine9[horiz - 1]);
          }
          else if ((vert == 13) && (horiz >= 0) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine10[horiz]);
          }
          else if ((vert == 12) && (horiz >= 0) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine11[horiz]);
          }
          else if ((vert == 11) && (horiz >= 0) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine12[horiz]);
          }
          else if ((vert == 10) && (horiz >= 0) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine13[horiz]);
          }
          else if ((vert == 9) && (horiz >= 1) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine14[horiz - 1]);
          }
          else if ((vert == 8) && (horiz >= 2) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine15[horiz - 2]);
          }
          else if ((vert == 7) && (horiz >= 2) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine16[horiz - 2]);
          }
          else if ((vert == 6) && (horiz >= 3) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine17[horiz - 3]);
          }
          else if ((vert == 5) && (horiz >= 4) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine18[horiz - 4]);
          }
          else if ((vert == 4) && (horiz >= 4) && (horiz < 18))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine19[horiz - 4]);
          }
          else if ((vert == 3) && (horiz >= 4) && (horiz < 17))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine20[horiz - 4]);
          }
          else if ((vert == 2) && (horiz >= 4) && (horiz < 17))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine21[horiz - 4]);
          }
          else if ((vert == 1) && (horiz >= 5) && (horiz < 17))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine22[horiz - 5]);
          }
          else if ((vert == 0) && (horiz >= 5) && (horiz < 17))
          {
            setCursorPixel(xpixel, 479 - ypixel, cursorLine23[horiz - 5]);
          }
        }
      }
    }
  }

  // Remember previous mouse position
  old_x = new_x;
  old_y = new_y;

  //memcpy(fbp, cfbp, screenSize);

  for(int cursor_redraw_line = y_start; cursor_redraw_line <= (y_start + y_height); cursor_redraw_line++)
  {
    // copy the cursor buffer into the display buffer 
    memcpy(fbp + x_start + (cursor_redraw_line * screenXsize * 4), cfbp + x_start + (cursor_redraw_line * screenXsize * 4), x_width);
  }
}


void refreshMouseBackground(void)
{
  if (mouse_connected == false)
  {
    return;
  }

  // Copies the active framebuffer into the mouse framebuffer
  // before the cursor is drawn.

  memcpy(mfbp, fbp, screenSize);
}


void draw_cursor_foreground(int x, int y)
{
  int vert;
  int horiz;
  int xpixel = 0;
  int ypixel = 0;

  if (mouse_connected == false)
  {
    return;
  }

  // Draws the cursor on the primary framebuffer

  y = 479 - y;

  for (vert = 0; vert < 24; vert++)
  {
    ypixel = y - 24 + vert;
    if ((ypixel >= 0) && (ypixel < 480))
    {

      for (horiz = 0; horiz < 18; horiz++)
      {
        xpixel = x - 6  + horiz;
        if ((xpixel >= 0) && (xpixel < 800))
        {
          if ((vert == 23) && (horiz >= 5) && (horiz < 8))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine0[horiz - 5], cursorLine0[horiz - 5], cursorLine0[horiz - 5]);
          }
          else if ((vert == 22) && (horiz >= 4) && (horiz < 9))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine1[horiz - 4], cursorLine1[horiz - 4], cursorLine1[horiz - 4]);
          }
          else if ((vert == 21) && (horiz >= 4) && (horiz < 9))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine2[horiz - 4], cursorLine2[horiz - 4], cursorLine2[horiz - 4]);
          }
          else if ((vert == 20) && (horiz >= 4) && (horiz < 9))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine3[horiz - 4], cursorLine3[horiz - 4], cursorLine3[horiz - 4]);
          }
          else if ((vert == 19) && (horiz >= 4) && (horiz < 9))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine4[horiz - 4], cursorLine4[horiz - 4], cursorLine4[horiz - 4]);
          }
          else if ((vert == 18) && (horiz >= 4) && (horiz < 10))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine5[horiz - 4], cursorLine5[horiz - 4], cursorLine5[horiz - 4]);
          }
          else if ((vert == 17) && (horiz >= 4) && (horiz < 13))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine6[horiz - 4], cursorLine6[horiz - 4], cursorLine6[horiz - 4]);
          }
          else if ((vert == 16) && (horiz >= 4) && (horiz < 16))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine7[horiz - 4], cursorLine7[horiz - 4], cursorLine7[horiz - 4]);
          }
          else if ((vert == 15) && (horiz >= 4) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine8[horiz - 4], cursorLine8[horiz - 4], cursorLine8[horiz - 4]);
          }
          else if ((vert == 14) && (horiz >= 1) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine9[horiz - 1], cursorLine9[horiz - 1], cursorLine9[horiz - 1]);
          }
          else if ((vert == 13) && (horiz >= 0) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine10[horiz], cursorLine10[horiz], cursorLine10[horiz]);
          }
          else if ((vert == 12) && (horiz >= 0) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine11[horiz], cursorLine11[horiz], cursorLine11[horiz]);
          }
          else if ((vert == 11) && (horiz >= 0) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine12[horiz], cursorLine12[horiz], cursorLine12[horiz]);
          }
          else if ((vert == 10) && (horiz >= 0) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine13[horiz], cursorLine13[horiz], cursorLine13[horiz]);
          }
          else if ((vert == 9) && (horiz >= 1) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine14[horiz - 1], cursorLine14[horiz - 1], cursorLine14[horiz - 1]);
          }
          else if ((vert == 8) && (horiz >= 2) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine15[horiz - 2], cursorLine15[horiz - 2], cursorLine15[horiz - 2]);
          }
          else if ((vert == 7) && (horiz >= 2) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine16[horiz - 2], cursorLine16[horiz - 2], cursorLine16[horiz - 2]);
          }
          else if ((vert == 6) && (horiz >= 3) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine17[horiz - 3], cursorLine17[horiz - 3], cursorLine17[horiz - 3]);
          }
          else if ((vert == 5) && (horiz >= 4) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine18[horiz - 4], cursorLine18[horiz - 4], cursorLine18[horiz - 4]);
          }
          else if ((vert == 4) && (horiz >= 4) && (horiz < 18))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine19[horiz - 4], cursorLine19[horiz - 4], cursorLine19[horiz - 4]);
          }
          else if ((vert == 3) && (horiz >= 4) && (horiz < 17))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine20[horiz - 4], cursorLine20[horiz - 4], cursorLine20[horiz - 4]);
          }
          else if ((vert == 2) && (horiz >= 4) && (horiz < 17))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine21[horiz - 4], cursorLine21[horiz - 4], cursorLine21[horiz - 4]);
          }
          else if ((vert == 1) && (horiz >= 5) && (horiz < 17))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine22[horiz - 5], cursorLine22[horiz - 5], cursorLine22[horiz - 5]);
          }
          else if ((vert == 0) && (horiz >= 5) && (horiz < 17))
          {
            setPixel(xpixel, 479 - ypixel, cursorLine23[horiz - 5], cursorLine23[horiz - 5], cursorLine23[horiz - 5]);
          }
        }
      }
    }
  }
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

  // Create frame store for use under mouse
  mfbp = malloc(screenSize);

  // Create frame store for use to build the cursor
  cfbp = (char*)malloc(screenSize);
                    
  if ((int)fbp == -1) 
  {
    return 0;
  }
  else 
  {
    return 1;
  }

}

