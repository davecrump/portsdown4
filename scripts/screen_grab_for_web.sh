#!/bin/bash

# This script saves a screengrab of the touchscreen once a second.
# The screengrab can then be served by the nginx web server to
# allow web-based control.
# It needs the creation of a /home/pi/tmp/ folder
# as a tmpfs by adding an entry in /etc/fstab
# tmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0
# and a /home/pi/snaps folder

#set -x

###########################################################################

# Make sure that there aren't any previous screengrabs
sudo rm /home/pi/tmp/frame.png >/dev/null 2>/dev/null

# Run until terminated
while [ true ]
do
  raspi2png -p /home/pi/tmp/stage_screen.png -c 3
  mv /home/pi/tmp/stage_screen.png /home/pi/tmp/screen.png
  sleep 1
done

exit

