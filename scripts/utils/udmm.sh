#! /bin/bash

# Compile dmm and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall dmm >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/dmm
touch dmm.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp dmm /home/pi/rpidatv/bin/dmm

cd /home/pi

/home/pi/rpidatv/bin/dmm

exit
