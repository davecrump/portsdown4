#! /bin/bash

# Udpate limesdr_toolbox
cd /home/pi/rpidatv/src/limesdr_toolbox
cmake .
make
cp limesdr_dump  ../../bin/
cp limesdr_send ../../bin/
cp limesdr_stopchannel ../../bin/
cp limesdr_forward ../../bin/
