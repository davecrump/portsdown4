#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include "screen.h"
#include "graphics.h"
#include "timing.h"
#include "font/font.h"
#include "font/Font.h"

static char *screen_fb_ptr = 0;
static int screen_fb_fd = 0;

/* Only supporting the Official 7" touchscreen */
#define SCREEN_SIZE_X   800
#define SCREEN_SIZE_Y   480
#define SCREEN_PIXEL_COUNT  (SCREEN_SIZE_X*SCREEN_SIZE_Y)

#define NEON_ALIGNMENT (4*4*2) // From libcsdr

pthread_mutex_t screen_backbuffer_mutex = PTHREAD_MUTEX_INITIALIZER;
screen_pixel_t screen_backbuffer[SCREEN_PIXEL_COUNT] __attribute__ ((aligned (NEON_ALIGNMENT)));

static screen_pixel_t screen_pixel_empty = {
  .Green = 0x00,
  .Red = 0x00,
  .Blue = 0x00,
  .Alpha = 0x80
};
static screen_pixel_t screen_pixel_empty_array[SCREEN_PIXEL_COUNT] __attribute__ ((aligned (NEON_ALIGNMENT)));

void screen_setPixel(int x, int y, screen_pixel_t *pixel_ptr)
{
  pthread_mutex_lock(&screen_backbuffer_mutex);

  memcpy(&screen_backbuffer[x + (y * SCREEN_SIZE_X)], (void *)pixel_ptr, sizeof(screen_pixel_t));

  pthread_mutex_unlock(&screen_backbuffer_mutex);
}

void screen_setPixelLine(int x, int y, int length, screen_pixel_t *pixel_array_ptr)
{
  pthread_mutex_lock(&screen_backbuffer_mutex);

  memcpy(&screen_backbuffer[x + (y * SCREEN_SIZE_X)], (void *)pixel_array_ptr, length * sizeof(screen_pixel_t));

  pthread_mutex_unlock(&screen_backbuffer_mutex);
}

void screen_clear(void)
{
  pthread_mutex_lock(&screen_backbuffer_mutex);
  
  memcpy(screen_backbuffer, (void *)screen_pixel_empty_array, SCREEN_PIXEL_COUNT * sizeof(screen_pixel_t));

  pthread_mutex_unlock(&screen_backbuffer_mutex);
}

void screen_render(int signum)
{
  (void)signum;

  pthread_mutex_lock(&screen_backbuffer_mutex);

  memcpy(screen_fb_ptr, (void *)&screen_backbuffer, SCREEN_PIXEL_COUNT * sizeof(screen_pixel_t));

  pthread_mutex_unlock(&screen_backbuffer_mutex);
}

//static void screen_splash_font_cb(int x, int y, screen_pixel_t *pixel_ptr)
//{
//  memcpy(&(screen_backbuffer[x + (y * SCREEN_SIZE_X)]), pixel_ptr, sizeof(screen_pixel_t));
//}

void screen_splash(void)
{
  pthread_mutex_lock(&screen_backbuffer_mutex);

  //char *splash_string;
  //asprintf(&splash_string, "QO-100 Transceiver");
  //font_render_string_with_callback(40, 100, &font_dejavu_sans_72, splash_string, screen_splash_font_cb);
  //free(splash_string);

  //asprintf(&splash_string, "Phil M0DNY");
  //font_render_string_with_callback(200, 300, &font_dejavu_sans_72, splash_string, screen_splash_font_cb);
  //free(splash_string);

  pthread_mutex_unlock(&screen_backbuffer_mutex);
}

bool screen_init(void)
{
  //struct fb_fix_screeninfo finfo;
 
  screen_fb_fd = open("/dev/fb0", O_RDWR);
  if (!screen_fb_fd) 
  {
    printf("Error: cannot open framebuffer device.\n");
    return false;
  }

  //screenSize = finfo.smem_len;
  screen_fb_ptr = (char*)mmap(0, SCREEN_PIXEL_COUNT * sizeof(screen_pixel_t), PROT_READ | PROT_WRITE, MAP_SHARED, screen_fb_fd, 0);
                    
  if ((int)screen_fb_ptr == -1) 
  {
    return false;
  }

  /* Set up empty array for clearing */
  for(uint32_t i = 0; i < SCREEN_PIXEL_COUNT; i++)
  {
    memcpy(&screen_pixel_empty_array[i], &screen_pixel_empty, sizeof(screen_pixel_t));
  }

  /* Clear Screen */
  screen_clear();

  screen_splash();

  /* Manually call render handler */
  screen_render(0);

  return true;
}

static void screen_deinit(void)
{
  munmap(screen_fb_ptr, SCREEN_PIXEL_COUNT * sizeof(screen_pixel_t));
  close(screen_fb_fd);
}

void *screen_thread(void *arg)
{
  bool *app_exit = (bool *)arg;

  /* Yuck, trigger this instead once everything is initialised */
  sleep_ms(700);

  screen_clear();

  /* Set up signal handler for timer */
  struct sigaction sa;
  memset(&sa, 0x00, sizeof(sa));
  sa.sa_handler = &screen_render;
  sigaction(SIGALRM, &sa, NULL);

  /* Set up timer, queue for immediate first render */
  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 1;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 10*1000;
  setitimer(ITIMER_REAL, &timer, NULL);

  while(!*app_exit)
  {
    sleep_ms(100);
  }

  /* De-initialise timer */
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &timer, NULL);

  screen_deinit();

  return NULL;
}