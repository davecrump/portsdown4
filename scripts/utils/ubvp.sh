#! /bin/bash

# Compile Pluto bandviewer and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall plutoview >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/plutoview
touch plutoview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp plutoview /home/pi/rpidatv/bin/plutoview
cd /home/pi

/home/pi/rpidatv/bin/plutoview

exit
