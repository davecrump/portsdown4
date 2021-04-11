#ifndef __FONT_H__
#define __FONT_H__

#ifndef __SCREEN_H__
  typedef void screen_pixel_t;
#endif



typedef struct {
    const uint32_t height;
    const uint32_t width;
    const uint32_t render_width;
    const uint8_t *map;
} font_character_t;

typedef struct {
    const uint32_t height;
    const uint32_t ascent;
    const font_character_t *characters;
} font_t;

//uint32_t font_width_string(const font_t *font_ptr, char *string);
//void displayChar2(const font_t *font_ptr, char*c);
//void TextMid2 (int xpos, int ypos, char*s, const font_t *font_ptr);
//void Text2 (int xpos, int ypos, char*s, const font_t *font_ptr);


#include "dejavu_sans_18.h"
#include "dejavu_sans_20.h"
#include "dejavu_sans_22.h"
#include "dejavu_sans_24.h"
#include "dejavu_sans_26.h"
#include "dejavu_sans_28.h"
#include "dejavu_sans_30.h"
#include "dejavu_sans_32.h"
#include "dejavu_sans_36.h"
#include "dejavu_sans_72.h"

#endif /* __FONT_H__ */