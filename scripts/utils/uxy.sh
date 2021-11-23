#! /bin/bash

# Compile power meter and copy to rpidatv/bin
# And run as XY Display

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall power_meter >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/power_meter
touch power_meter.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp power_meter /home/pi/rpidatv/bin/power_meter
cd /home/pi

/home/pi/rpidatv/bin/power_meter xy

exit
