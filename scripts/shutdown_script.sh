#!/bin/bash

# Called by pi-sdn to tidy up and shutdown.

# Kill the gui - this also stops any transmit processes
sudo killall -9 rpidatvgui

sleep 1

# Display the splash screen so the user knows something is happening
sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null

# Make sure that the swap file doesn't cause a hang
sudo swapoff -a

sudo shutdown now

exit
