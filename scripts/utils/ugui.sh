#! /bin/bash

# Compile rpidatv gui
cd /home/pi/rpidatv/src/gui
make clean
make
sudo make install
cd ../


