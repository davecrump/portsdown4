#! /bin/bash

# Update libdvbmod and DvbTsToIQ
cd /home/pi/rpidatv/src/libdvbmod
make dirmake
make
cd ../DvbTsToIQ
make
cp dvb2iq ../../bin/
cd /home/pi
exit
