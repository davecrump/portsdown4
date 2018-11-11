#! /bin/bash

# Run XY Display gui

# set -x

cd /home/pi

sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/src/xy/xy
reset
