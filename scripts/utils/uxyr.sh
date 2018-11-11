#! /bin/bash

# Compile XY Display gui

# set -x
cd /home/pi/rpidatv/src/xy
make clean
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cd /home/pi

sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/src/xy/xy
reset
