#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#include "screen.h"
#include "graphics.h"
#include "font/font.h"

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
screen_pixel_t main_spectrum_buffer[MAIN_SPECTRUM_HEIGHT][MAIN_SPECTRUM_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));
#define MAIN_SPECTRUM_TIME_SMOOTH   0.8f
float main_spectrum_smooth_buffer[MAIN_SPECTRUM_WIDTH] = { 0 };

#define MAIN_SPECTRUM_POS_X     100
#define MAIN_SPECTRUM_POS_Y     (SCREEN_HEIGHT - MAIN_WATERFALL_HEIGHT - MAIN_SPECTRUM_HEIGHT - 70)

/** Frequency Display **/

#define FREQUENCY_WIDTH         256
#define FREQUENCY_HEIGHT        43
screen_pixel_t frequency_buffer[FREQUENCY_HEIGHT][FREQUENCY_WIDTH] __attribute__ ((aligned (NEON_ALIGNMENT)));

#define FREQUENCY_POS_X         544
#define FREQUENCY_POS_Y         8


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
    main_spectrum_smooth_buffer[i] = (((float)fft_data[i]) * (1.f - MAIN_SPECTRUM_TIME_SMOOTH)) + (main_spectrum_smooth_buffer[i] * MAIN_SPECTRUM_TIME_SMOOTH);
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
    screen_setPixelLine(MAIN_SPECTRUM_POS_X, MAIN_SPECTRUM_POS_Y + i, MAIN_SPECTRUM_WIDTH, main_spectrum_buffer[i]);
  }
}

//static void frequency_render_font_cb(int x, int y, screen_pixel_t *pixel_ptr)
//{
//  memcpy(&(frequency_buffer[y][x]), pixel_ptr, sizeof(screen_pixel_t));
//}

static void frequency_generate(void)
{
  uint32_t i, j;
  /* Clear buffer */
  for(i = 0; i < FREQUENCY_HEIGHT; i++)
  {
    for(j = 0; j <FREQUENCY_WIDTH; j++)
    {
      memcpy(&(frequency_buffer[i][j]), &graphics_black_pixel, sizeof(screen_pixel_t));
    }
  }

//  char *freq_string;
//  asprintf(&freq_string, ".%3lld.%03lld.%03lld", 
//    (selected_center_frequency / 1000000) % 1000,
//    (selected_center_frequency / 1000) % 1000,
//    selected_center_frequency % 1000);
//  font_render_string_with_callback(0, 0, &font_dejavu_sans_36, freq_string, frequency_render_font_cb);
//  free(freq_string);
}

static void frequency_render(void)
{
  /* Render Frequency display buffer */
  for(uint32_t i = 0; i < FREQUENCY_HEIGHT; i++)
  {
    screen_setPixelLine(FREQUENCY_POS_X, FREQUENCY_POS_Y + i, FREQUENCY_WIDTH, frequency_buffer[i]);
  }
}

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

void graphics_frequency_newdata(void)
{
  frequency_generate();
  frequency_render();
}
