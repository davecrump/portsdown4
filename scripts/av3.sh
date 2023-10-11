#!/bin/bash

# This script uses mplayer to display the video from the C920 and other webcams

# set -x

AUDIO="22050"

############ IDENTIFY USB VIDEO DEVICES #############################

# List the video devices, select the 2 lines for the each comptible webcam device, then
# select the line with the device details and delete the leading tab

# C920
VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/C920/,/dev/p' | grep 'dev' | tr -d '\t')"

# C930E
if [ "$VID_USB" == '' ]; then
  VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/C930e/,/dev/p' | grep 'dev' | tr -d '\t')"
fi

# C270
if [ "$VID_USB" == '' ]; then
   VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
     sed -n '/(046d:0825)/,/dev/p' | grep 'dev' | tr -d '\t')"
fi

# Logitech  Logi 4K Stream Edition
if [ "$VID_USB" == '' ]; then
   VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
     sed -n '/Logi 4K Stream Edition/,/dev/p' | grep 'dev' | tr -d '\t')"
fi

# EagleEye
if [ "$VID_USB" == '' ]; then
  VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/EagleEye/,/dev/p' | grep 'dev' | tr -d '\t')"

  if [ "$VID_USB" == '' ]; then
    printf "VID_USB was not found, setting to /dev/video0\n"
    VID_USB="/dev/video0"        # Try default with audio
  else
    AUDIO="none"                 # EagleEye has no audio
  fi
fi

printf "The USB device string is $VID_USB\n"

###########################################################################

sudo killall mplayer >/dev/null 2>/dev/null

if [ "$AUDIO" == 'none' ]; then

  # mplayer with no audio with resolution for EagleEye
  mplayer tv:// -tv driver=v4l2:device="$VID_USB":width=1280:height=720 \
    -vo fbdev2 -vf scale=800:480

else

  # mplayer with audio for other webcams
  mplayer tv:// -tv driver=v4l2:device="$VID_USB":width=864:height=480:audiorate=22050 \
    -vo fbdev2 -vf scale=800:480

fi

exit

