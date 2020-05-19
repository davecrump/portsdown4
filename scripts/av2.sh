#!/bin/bash

# This script uses mplayer to display the video from the EasyCap.
# It scales the video by 3/4 (this is easiest in processor load)
# The video overflows if not scaled.

# No scaling required for RPi 4!!

# set -x

############ IDENTIFY USB VIDEO DEVICES #############################

# List the video devices, select the 2 lines for the usbtv device, then
# select the line with the device details and delete the leading tab

VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/usbtv/,/dev/p' | grep 'dev' | tr -d '\t')"

if [ "$VID_USB" == '' ]; then
  printf "VID_USB was not found, setting to /dev/video0\n"
  VID_USB="/dev/video0"
fi

printf "The USB device string is $VID_USB\n"

###########################################################################

sudo killall mplayer >/dev/null 2>/dev/null

mplayer tv:// -tv driver=v4l2:device="$VID_USB":norm=PAL:input=0\
  -ao alsa:device=hw=2.0 -vo fbdev2 -vf scale=800:480

#mplayer -ao alsa:device=hw=2.0 -vo fbdev2 -vf scale=800:480 hdtest.mp4
#mplayer tv:// -tv driver=v4l2:device="/dev/video0":norm=PAL:input=0:alsa:adevice=hw=0.0:forceaudio:audiorate=48000 -vo fbdev2 -vf scale=800:480 -ao alsa:adevice=hw=2.0


#mplayer -ao alsa:device=hw=2.0 -vo fbdev2 -vf scale=800:480 hdtest.mp4  Works!
