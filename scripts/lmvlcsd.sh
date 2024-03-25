#!/bin/bash

# Called (as a forked thread) to shutdown vlc normally, and then if required
# shut it down forcefully, which should clear the graphics layer after a kernel panic 

echo shutdown | nc 127.0.0.1 1111

sleep 0.5

if  $(pgrep vlc >/dev/null)  ; then
  sleep 0.5
  sudo killall -9 vlc >/dev/null 2>/dev/null
fi

exit





