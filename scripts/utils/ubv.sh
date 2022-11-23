#! /bin/bash

# Compile LimeSDR bandview and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall bandview >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/bandview
touch bandview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp bandview /home/pi/rpidatv/bin/bandview
cd /home/pi

/home/pi/rpidatv/bin/bandview

exit
