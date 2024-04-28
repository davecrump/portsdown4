#! /bin/bash

# Compile Pluto nf_meter and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall pluto_nf_meter >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/pluto_nf_meter
touch pluto_nf_meter.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp pluto_nf_meter /home/pi/rpidatv/bin/pluto_nf_meter
cd /home/pi

/home/pi/rpidatv/bin/pluto_nf_meter

exit
