#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../screen.h"
#include "font.h"

static const screen_pixel_t font_white_pixel =
{
    .Alpha = 0x80,
    .Red = 0xFF,
    .Green = 0xFF,
    .Blue = 0xFF
};

static const screen_pixel_t font_black_pixel =
{
    .Alpha = 0x80,
    .Red = 0x00,
    .Green = 0x00,
    .Blue = 0x00
};

// Returns Width of rendered character, 0 if not found 
uint32_t font_render_character_with_callback(int x, int y, const font_t *font_ptr, char c, const screen_pixel_t *pixel_background, const screen_pixel_t *pixel_foreground, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr))
{
    screen_pixel_t character_pixel;
    character_pixel.Alpha = 0x80;

    const int32_t red_contrast = pixel_foreground->Red - pixel_background->Red;
    const int32_t green_contrast = pixel_foreground->Green - pixel_background->Green;
    const int32_t blue_contrast = pixel_foreground->Blue - pixel_background->Blue;

    uint32_t y_offset = 0;
    if(font_ptr->characters[(uint8_t)c].height < font_ptr->ascent)
    {
        y_offset = font_ptr->ascent - font_ptr->characters[(uint8_t)c].height;
    }

    uint32_t i, j;
    /* For each Line */
    for(i = 0; i < font_ptr->characters[(uint8_t)c].height; i++)
    {
        /* For each Column */
        for(j = 0; j < font_ptr->characters[(uint8_t)c].width; j++)
        {
            character_pixel.Red = pixel_background->Red
                + ((red_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[j+(i*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
            character_pixel.Green = pixel_background->Green
                + ((green_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[j+(i*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);
            character_pixel.Blue = pixel_background->Blue
                + ((blue_contrast * (int32_t)font_ptr->characters[(uint8_t)c].map[j+(i*font_ptr->characters[(uint8_t)c].width)]) / 0xFF);

            render_cb(
                j + x,
                i + y + y_offset,
                &character_pixel
            );
        }
    }

    return font_ptr->characters[(uint8_t)c].render_width;
}

uint32_t font_render_string_with_callback(int x, int y, const font_t *font_ptr, char *string, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr))
{
    uint32_t string_length = strlen(string);
    for(uint32_t i = 0; i < string_length; i++)
    {
        x += font_render_character_with_callback(x, y, font_ptr, string[i], &font_black_pixel, &font_white_pixel, render_cb);
    }
    return x;
}

uint32_t font_render_colour_string_with_callback(int x, int y, const font_t *font_ptr, const screen_pixel_t *pixel_background_ptr, const screen_pixel_t *pixel_foreground_ptr, char *string, void (*render_cb)(int x, int y, screen_pixel_t *pixel_ptr))
{
    uint32_t string_length = strlen(string);
    for(uint32_t i = 0; i < string_length; i++)
    {
        x += font_render_character_with_callback(x, y, font_ptr, string[i], pixel_background_ptr, pixel_foreground_ptr, render_cb);
    }
    return x;
}

//uint32_t font_width_string(const font_t *font_ptr, char *string)
//{
//    uint32_t total_width = 0;
//    uint32_t string_length = strlen(string);
//    for(uint32_t i = 0; i < string_length; i++)
//    {
//        total_width += font_ptr->characters[(uint8_t)string[i]].render_width;
//    }
//    return total_width;
//}

