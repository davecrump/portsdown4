#!/bin/bash

# This script uses mplayer to display the video from the C920.

# set -x

############ IDENTIFY USB VIDEO DEVICES #############################

# List the video devices, select the 2 lines for the usbtv device, then
# select the line with the device details and delete the leading tab

VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/C920/,/dev/p' | grep 'dev' | tr -d '\t')"

if [ "$VID_USB" == '' ]; then
  printf "VID_USB was not found, setting to /dev/video0\n"
  VID_USB="/dev/video0"
fi

printf "The USB device string is $VID_USB\n"

###########################################################################

sudo killall mplayer >/dev/null 2>/dev/null

mplayer tv:// -tv driver=v4l2:device="$VID_USB" \
   -fs -vo fbdev /dev/fb0


