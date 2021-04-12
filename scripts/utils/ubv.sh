#! /bin/bash

# Compile bandview and copy to rpidatv/bin

# set -x

# Kill the current process if it is running
sudo killall bandview

cd /home/pi/rpidatv/src/bandview
touch bandview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cp bandview /home/pi/rpidatv/bin/bandview
cd /home/pi

exit
