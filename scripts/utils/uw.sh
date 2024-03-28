#! /bin/bash

# Compile wav2lime and wav642lime and copy to rpidatv/bin

# set -x

sudo killall wav642lime >/dev/null 2>/dev/null
sudo killall wav2lime >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/wav2lime
gcc -o wav2lime wav2lime.c -lLimeSuite
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

gcc -o wav642lime wav642lime.c -lLimeSuite
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cp wav642lime /home/pi/rpidatv/bin/wav642lime
cp wav2lime /home/pi/rpidatv/bin/wav2lime
cd /home/pi

exit
