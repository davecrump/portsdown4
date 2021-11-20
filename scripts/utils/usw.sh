#! /bin/bash

# Compile sweeper and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall sweeper >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/sweeper
touch sweeper.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp sweeper /home/pi/rpidatv/bin/sweeper
cd /home/pi

/home/pi/rpidatv/bin/sweeper

exit
