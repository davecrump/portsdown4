#!/bin/bash

# Called with a parameter in the range 0 to 512.
# Opens an ssh session with VLC and sets the volume to that value

VLCVOLUME=$1

# Error check parameter
if [ "$VLCVOLUME" -lt 0 ]; then
  VLCVOLUME=0
fi
if [ "$VLCVOLUME" -gt 512 ]; then
  VLCVOLUME=512
fi

printf "volume "$VLCVOLUME"\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

exit



