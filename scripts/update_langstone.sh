#!/bin/bash

# set -x

cd /home/pi

# Stop Langstone if it is running (it shouldn't be)
/home/pi/Langstone/stop

# Keep a copy of the old Config file
cp -f /home/pi/Langstone/Langstone.conf /home/pi/rpidatv/scripts/configs/Langstone.conf

# Remove the old Langstone build
rm -rf /home/pi/Langstone

# Clone the latest version
git clone http://www.github.com/g4eml/Langstone

cd Langstone
chmod +x build
chmod +x run
chmod +x stop
chmod +x update
cd /home/pi

# Build the c executable
/home/pi/Langstone/build

# Restore the old Config file
cp -f /home/pi/rpidatv/scripts/configs/Langstone.conf /home/pi/Langstone/Langstone.conf 

exit
