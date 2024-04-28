#! /bin/bash

# Compile Pluto noise_meter and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall pluto_noise_meter >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/pluto_noise_meter
touch pluto_noise_meter.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp pluto_noise_meter /home/pi/rpidatv/bin/pluto_noise_meter
cd /home/pi

/home/pi/rpidatv/bin/pluto_noise_meter

exit
