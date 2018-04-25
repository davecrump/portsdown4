#! /bin/bash

# Compile Signal Generator
cd /home/pi/rpidatv/src/siggen
make clean
make
sudo make install
cd ../

cd /home/pi
reset
sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/bin/siggen 1

