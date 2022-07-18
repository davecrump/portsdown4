#! /bin/bash

# Compile Airspy bandviewer and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall airspyview >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/airspyview
touch airspyview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp airspyview /home/pi/rpidatv/bin/airspyview
cd /home/pi

/home/pi/rpidatv/bin/airspyview

exit
