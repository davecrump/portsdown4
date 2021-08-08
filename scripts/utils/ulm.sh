#! /bin/bash

# Udpate longmynd
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

