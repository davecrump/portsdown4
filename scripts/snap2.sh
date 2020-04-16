#!/bin/bash

# This script saves a screengrab of the touchscreen and saves it to
# file.  It needs the creation of a /home/pi/tmp/ folder
# as a tmpfs by adding an entry in /etc/fstab
# tmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0
# and a /home/pi/snaps folder

#set -x

###########################################################################

# Make sure that there aren't any previous temp snaps
sudo rm /home/pi/tmp/frame.* >/dev/null 2>/dev/null

# If the index number does not exist, create it as zero
if  [ ! -f "/home/pi/snaps/snap_index.txt" ]; then
    echo '0' > /home/pi/snaps/snap_index.txt
fi

# Read the index number
SNAP_SERIAL=$(head -c 4 /home/pi/snaps/snap_index.txt)

# Take the snap and convert it to a jpg
raspi2png -p /home/pi/tmp/frame.png -c 0
convert /home/pi/tmp/frame.png /home/pi/tmp/frame.jpg

# Then save it to the snap folder 
cp /home/pi/tmp/frame.jpg /home/pi/snaps/snap"$SNAP_SERIAL".jpg

# Increment the stored index number
let SNAP_SERIAL=$SNAP_SERIAL+1
rm  /home/pi/snaps/snap_index.txt
echo $SNAP_SERIAL  >  /home/pi/snaps/snap_index.txt
