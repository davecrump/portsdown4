#! /bin/bash

# Compile Ryde Monitor and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall rydemon >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/rydemon
touch rydemon.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp rydemon /home/pi/rpidatv/bin/rydemon
cd /home/pi

/home/pi/rpidatv/bin/rydemon

exit
