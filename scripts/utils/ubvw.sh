#!/bin/bash

# Compile RTL-SDR waterfall bandviewer and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall waterview >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/waterview
touch rtlsdrview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp rtlsdrview /home/pi/rpidatv/bin/waterview
cd /home/pi

/home/pi/rpidatv/bin/waterview

exit
