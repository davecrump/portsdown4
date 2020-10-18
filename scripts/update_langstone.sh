#!/bin/bash

# set -x

cd /home/pi

# Stop Langstone if it is running (it shouldn't be)
Langstone/stop

# Keep a copy of the old Config file
cp -f Langstone/Langstone.conf rpidatv/scripts/configs/Langstone.conf

# Remove the old Langstone build
rm -rf Langstone

# Clone the latest version
git clone https://github.com/g4eml/Langstone.git

cd Langstone

# Build the c executable
./build

cd /home/pi

# Restore the old Config file
cp -f rpidatv/scripts/configs/Langstone.conf Langstone/Langstone.conf 

