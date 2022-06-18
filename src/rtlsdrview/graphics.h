#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

void waterfall_render_fft(uint8_t *fft_data);

// void graphics_frequency_newdata(void);
// void graphics_if_fft_newdata(uint8_t *fft_data);

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
void setPixelNoA(int x, int y, int R, int G, int B);
void setPixelNoABlk(int x, int y);
void setPixelNoAGra(int x, int y);
void MarkerGrn(int markerx, int x, int y);
void setLargePixel(int x, int y, int size, int R, int G, int B);
void closeScreen(void);
int initScreen(void);
void HorizLine(int xpos, int ypos, int xsize, int r, int g, int b);
void VertLine(int xpos, int ypos, int ysize, int r, int g, int b);



#endif /* __GRAPHICS_H__ */

