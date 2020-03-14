#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "timer.h"

static void timer_callback(union sigval val)
{
  _timer_t *timer = val.sival_ptr;
  timer_reset(timer);

  void (*cb)(void) = (void (*)(void)) timer->cb;
  cb();
}

void timer_set(_timer_t *new_timer, uint64_t _milliseconds, void*_cb)
{
  uint64_t milliseconds = _milliseconds % 1000;
  uint64_t nanoseconds = milliseconds * 1000 * 1000;
  uint64_t seconds = _milliseconds / 1000;

  new_timer->cb = _cb;

  pthread_attr_t attr;
  pthread_attr_init(&attr);

  struct sched_param parm;
  parm.sched_priority = 255;
  pthread_attr_setschedparam(&attr, &parm);

  struct sigevent sig;
  sig.sigev_notify = SIGEV_THREAD;
  sig.sigev_notify_function = timer_callback;
  sig.sigev_notify_attributes = &attr;

  sigval_t val;
  val.sival_ptr = (void*) new_timer;
  sig.sigev_value = val;

  timer_create(CLOCK_REALTIME, &sig, &new_timer->timer);

  struct itimerspec in, out;
  in.it_value.tv_sec = seconds;
  in.it_value.tv_nsec = nanoseconds;
  in.it_interval.tv_sec = 0;
  in.it_interval.tv_nsec = 0;
  timer_settime(new_timer->timer, 0, &in, &out);
}

bool timer_reset(_timer_t *reset_timer) {
  timer_t timer = reset_timer->timer;
  struct itimerspec arm;
 
  /* Check timer exists */
  if(!timer)
  {
    return false;
  }

  /* Disarm timer */
  arm.it_value.tv_sec = 0;
  arm.it_value.tv_nsec = 0L;
  arm.it_interval.tv_sec = 0;
  arm.it_interval.tv_nsec = 0;
  if (timer_settime(timer, 0, &arm, NULL))
  {
    return false;
  }

  /* Destroy the timer */
  if (timer_delete(timer))
  {
    return false;
  }
  
  return true;
}