#ifndef __SCREEN_H__
#define __SCREEN_H__

#define SCREEN_WIDTH    800
#define SCREEN_HEIGHT   480

typedef struct {
  uint8_t Blue;
  uint8_t Green;
  uint8_t Red;
  uint8_t Alpha; // 0x80
} __attribute__((__packed__)) screen_pixel_t;

bool screen_init(void);
void *screen_thread(void *arg);

void screen_clear(void);
void screen_setPixel(int x, int y, screen_pixel_t *pixel_ptr);
void screen_setPixelLine(int x, int y, int length, screen_pixel_t *pixel_array_ptr);

#endif /* __SCREEN_H__ */

