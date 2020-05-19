#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "Font.h"

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


void closeScreen(void);
int initScreen(void);
void setPixel(int x, int y, int R, int G, int B);
void clearScreen();
void displayChar(int ch);
void setLargePixel(int x, int y, int size, int R, int G, int B);
void gotoXY(int x, int y);
void setForeColour(int R,int G,int B);
void setBackColour(int R,int G,int B);
void displayStr(char*s);
void displayButton(char*s);
void BackgroundRGB(int R, int G, int B, int A);
void showButton(char*s, int xpos, int ypos, int xsize, int ysize, int r, int g, int b);
void Fill(int r, int g, int b, int a);
void Text(int xpos, int ypos, char*s, int pt);
void TextMid(int xpos, int ypos, char*s, int pt);
void BlankText(int xpos, int ypos, int spaces, int pt);


void displayStr(char*s)
{
  int p;
  p=0;

  do   // While not end of string
  {
    displayChar(s[p++]);
  }
  while(s[p] != 0);  // While not end of string
}

void displayChar(int ch)
{
  int row;
  int col;
  int pix;

  for(row = 0; row < 9; row++)
  {
    pix = font[ch][row];
    for(col=0;col<8;col++)
    {
      if((pix << col) & 0x80)
      {
        setLargePixel(currentX+col*textSize, currentY+row*textSize, textSize, foreColourR, foreColourG, foreColourB);
      }
      //else
      //{ 
      //  setLargePixel(currentX+col*textSize, currentY+row*textSize, textSize, backColourR, backColourG, backColourB);
      //}
    }
  }
  currentX = currentX + 8 * textSize;
}

void displayBlankChar()
{
  int row;
  int col;

  for(row = 0; row < 9; row++)
  {
    for(col = 0; col < 8; col++)
    {
      setLargePixel(currentX+col*textSize, currentY+row*textSize, textSize, backColourR, backColourG, backColourB);
    }
  }
  currentX = currentX + 8 * textSize;
}


void TextMid(int xpos, int ypos, char*s, int pt)
{
  int sx;
  int ysize = 1;
  int p;
  p=0;
  ypos = ypos + 30;

  sx = ((strlen(s) * 16) / 2);  
  gotoXY(xpos - sx, 480 - ypos);
  textSize=2;

  //printf("TextMid x %d, y %d, %s\n", xpos, ypos, s);

  do
  {
    displayChar(s[p++]);
  }
  while(s[p] != 0);  // While not end of string

}

void Text(int xpos, int ypos, char*s, int pt)
{
  // xpos = 30;
  //ypos = 100;
  ypos = ypos + 30;

  gotoXY(xpos, 480 - ypos);
  textSize=2;
  int p;
  p=0;
  
  //printf("TEXT x %d, y %d, %s\n", xpos, ypos, s);
  do
  {
    displayChar(s[p++]);
  }
  while(s[p] != 0);  // While not end of string
}

void BlankText(int xpos, int ypos, int spaces, int pt)
{
  ypos = ypos + 30;

  gotoXY(xpos, 480 - ypos);
  textSize=2;
  int p;
  for(p = 0; p <= spaces; p++)
  {
    displayBlankChar();
  }
}


void showButton(char*s, int xpos, int ypos, int xsize, int ysize, int r, int g, int b)
{
  int x;     // pixel count
  int y;     // pixel count
  int sx;    // text horizontal offset within button

  for(x = 0; x < xsize; x++)  // for each vertical slice along the x
  {
    for(y = 0; y < ysize; y++)  // Draw a vertical line
    {
      setPixel(xpos + x, 480 - (ypos + y), r, g, b);
    }
  }

  foreColourR = 255;
  foreColourG = 255;
  foreColourB = 255;
  //backColourR = r;
  //backColourG = g;
  //backColourB = b;

  char find = '^';                                  // Line separator is ^
  const char *ptr = strchr(s, find);            // pointer to ^ in string
  if((ptr) && (strlen(s) != 1))                     // if ^ found with other text
  {
    int index = ptr - s;                        // Position of ^ in string
    char line1[15];
    char line2[15];
    snprintf(line1, index+1, s);                // get text before ^
    snprintf(line2, strlen(s) - index, s + index + 1);  // and after ^
    sx= 50 - ((strlen(line1) * 8) / 2);  
    gotoXY(xpos + sx, 465 - ypos - ysize/2);
    textSize=1;
    displayStr(line1);  // Write the text
    sx= 50 - ((strlen(line2) * 8) / 2);  
    gotoXY(xpos + sx, 495 - ypos - ysize/2);
    displayStr(line2);  // Write the text
  }
  else
  {
    sx= 50 - ((strlen(s) * 8) / 2);  
    gotoXY(xpos + sx, 480 - ypos - ysize/2);
    textSize=1;
    displayStr(s);  // Write the text
  }
}


void displayButton(char*s)
{
  int saveX; // Button Reference position
  int saveY; // Button Reference position
  int x;     // pixel count
  int y;     // pixel count
  int sx;    // text horizontal offset within button

  saveX = currentX;
  saveY = currentY;

  for(x = 0; x < 100; x++)  // Draw and bottom lines
  {
    setPixel(currentX+x,currentY,foreColourR,foreColourG,foreColourB);
    setPixel(currentX+x,currentY+50,foreColourR,foreColourG,foreColourB);
  }
  for(y = 0; y < 50; y++)  // Draw side lines
  {
    setPixel(currentX,currentY+y,foreColourR,foreColourG,foreColourB);
    setPixel(currentX+100,currentY+y,foreColourR,foreColourG,foreColourB);
  }

  gotoXY(currentX + 1, currentY + 18);
  textSize = 2;
  displayStr("      "); // Blank out where text will be

  currentX = saveX;
  sx= 50 - ((strlen(s) * 16) / 2);  
  gotoXY(currentX + sx, currentY);
  textSize=2;
  displayStr(s);  // Write the text
  currentX = saveX + 105; // Next button position
  currentY = saveY;
}


void gotoXY(int x, int y)
{
  currentX = x;
  currentY = y;
}

void Fill(int r, int g, int b, int a)
{
  setForeColour(r, g, b);
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

void BackgroundRGB(int R, int G, int B, int A)
{
  setBackColour(R, G, B);
  clearScreen();
}


void clearScreen()
{
  for(int y=0;y<screenYsize;y++)
  {
    for(int x=0;x<screenXsize;x++)
    {
      setPixel(x,y,backColourR,backColourG,backColourB);
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