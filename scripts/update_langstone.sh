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
git clone https://github.com/g4eml/Langstone.git

cd /home/pi/Langstone

# Build the c executable
./build

cd /home/pi

# Restore the old Config file
cp -f /home/pi/rpidatv/scripts/configs/Langstone.conf /home/pi/Langstone/Langstone.conf 

