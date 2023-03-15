#ifndef __FONT_H__
#define __FONT_H__

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


#include "dejavu_sans_18.h"
#include "dejavu_sans_20.h"
#include "dejavu_sans_22.h"
#include "dejavu_sans_24.h"
#include "dejavu_sans_26.h"
#include "dejavu_sans_28.h"
#include "dejavu_sans_30.h"
#include "dejavu_sans_32.h"
#include "dejavu_sans_36.h"


#ifndef __SCREEN_H__
  typedef void screen_pixel_t;
#endif


uint32_t font_render_string_with_callback(int x, int y, const font_t *font_ptr, char *string, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr));
uint32_t font_render_colour_string_with_callback(int x, int y, const font_t *font_ptr, const screen_pixel_t *pixel_background_ptr, const screen_pixel_t *pixel_foreground_ptr, char *string, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr));

uint32_t font_render_character_with_callback(int x, int y, const font_t *font_ptr, char c, const screen_pixel_t *pixel_background, const screen_pixel_t *pixel_foreground, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr));
//uint32_t font_width_string(const font_t *font_ptr, char *string);

#endif /* __FONT_H__ */

