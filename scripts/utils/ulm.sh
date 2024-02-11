#! /bin/bash

sudo killall longmynd >/dev/null 2>/dev/null

# Udpate longmynd
cd /home/pi
rm -rf longmynd
cp -r /home/pi/rpidatv/src/longmynd/ /home/pi/
cd /home/pi/longmynd

make clean
make
if [ $? != "0" ]; then
  echo
  echo "Failed longmynd install"
  cd /home/pi
  exit
fi

cd /home/pi
exit

