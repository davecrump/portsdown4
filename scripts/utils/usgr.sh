#! /bin/bash

sudo killall siggen >/dev/null 2>/dev/null
sudo killall rpidatvgui >/dev/null 2>/dev/null

# Compile Signal Generator
cd /home/pi/rpidatv/src/siggen
# make clean
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi
sudo make install

cd /home/pi
reset

/home/pi/rpidatv/bin/siggen 1

