#! /bin/bash

# Udpate limesdr_toolbox
cd /home/pi/rpidatv/src/limesdr_toolbox
make
if [ $? != "0" ]; then
  echo
  echo "Failed basic LimeSDR Toolbox install"
  cd /home/pi
  exit
fi

cp limesdr_send /home/pi/rpidatv/bin/
cp limesdr_dump /home/pi/rpidatv/bin/
cp limesdr_stopchannel /home/pi/rpidatv/bin/
cp limesdr_forward /home/pi/rpidatv/bin/

make dvb
if [ $? != "0" ]; then
  echo
  echo "Failed LimeSDR DVB install"
  cd /home/pi
  exit
fi

cp limesdr_dvb /home/pi/rpidatv/bin/
cd /home/pi
exit

