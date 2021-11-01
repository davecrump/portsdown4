#! /bin/bash

# Compile nf_meter and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall nf_meter >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/nf_meter
touch nf_meter.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp nf_meter /home/pi/rpidatv/bin/nf_meter
cd /home/pi

/home/pi/rpidatv/bin/nf_meter

exit
