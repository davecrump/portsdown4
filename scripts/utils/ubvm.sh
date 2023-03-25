#! /bin/bash

# Compile Meteor viewer and copy to rpidatv/bin

# set -x

sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall meteorview >/dev/null 2>/dev/null

sudo systemctl restart sdrplay

cd /home/pi/rpidatv/src/meteorview
touch meteorview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp meteorview /home/pi/rpidatv/bin/meteorview
cd /home/pi

/home/pi/rpidatv/bin/meteorview

exit
