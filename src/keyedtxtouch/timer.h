#ifndef _TIMER_H__
#define _TIMER_H__

#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct _timer_t {
  timer_t timer;
  uint64_t milliseconds;
  void *cb;
} _timer_t;

void timer_set(_timer_t *new_timer, uint64_t _milliseconds, void*_cb);
bool timer_reset(_timer_t *_timer);

#endif /* _TIMER_H__ */