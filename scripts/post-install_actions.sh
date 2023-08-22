#!/bin/bash

# This file is run (from startup.sh) on the first reboot after an install.

# Its function is to install the sdrplay api and then compile the sdrplay-related applications.
# The sdrplay needs a reboot after other installation actions before it will run successfully

cd /home/pi/rpidatv/src/meteorview

./sdrplay_api_install.exp

sudo systemctl disable sdrplay  # service is started only when required

# Compile meteorview
make
cp meteorview ../../bin/
cd /home/pi

# Delete the trigger file
rm /home/pi/rpidatv/.post-install_actions

exit