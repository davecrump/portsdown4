#! /bin/bash

# Compile noise_meter and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall noise_meter >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/noise_meter
touch noise_meter.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp noise_meter /home/pi/rpidatv/bin/noise_meter
cd /home/pi

/home/pi/rpidatv/bin/noise_meter

exit
