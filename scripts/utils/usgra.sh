#! /bin/bash

sudo killall siggen >/dev/null 2>/dev/null
sudo killall rpidatvgui >/dev/null 2>/dev/null
sudo killall adf4351 >/dev/null 2>/dev/null

# Compile ADF4351
cd /home/pi/rpidatv/src/adf4351
touch adf4351.c
make
if [ $? != "0" ]; then
  echo
  echo "failed adf4351 driver install"
  cd /home/pi
  exit
fi
cp adf4351 ../../bin/
cd /home/pi

# Compile Signal Generator
cd /home/pi/rpidatv/src/siggen
# make clean
make
if [ $? != "0" ]; then
  echo
  echo "failed signal generator install"
  cd /home/pi
  exit
fi
sudo make install

cd /home/pi
reset

/home/pi/rpidatv/bin/siggen 1

