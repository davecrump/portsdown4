#!/bin/bash

# This script uses mplayer to display the video from the Pi Cam

# set -x

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

############ FUNCTION TO READ CONFIG FILE #############################

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}


########### Reload the Pi Cam Driver in case it has been unloaded ###########

sudo modprobe bcm2835_v4l2 >/dev/null 2>/dev/null

############ IDENTIFY USB VIDEO DEVICES ###################################

# List the video devices, select the 2 lines for the PiCam device, then
# select the line with the device details and delete the leading tab

VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/mmal/,/dev/p' | grep 'dev' | tr -d '\t')"

if [ "$VID_USB" == '' ]; then
  printf "VID_USB was not found, setting to /dev/video0\n"
  VID_USB="/dev/video0"
fi

printf "The USB device string is $VID_USB\n"


############ ALLOW FOR INVERTED CAMERA ###################################

# Rotate image if required

PICAM=$(get_config_var picam $PCONFIGFILE)

if [ "$PICAM" != "normal" ]; then
  v4l2-ctl -d $VID_USB --set-ctrl=rotate=180
else
  v4l2-ctl -d $VID_USB --set-ctrl=rotate=0
fi

############ USE MPLAYER TO DISPLAY ####################################

sudo killall mplayer >/dev/null 2>/dev/null

mplayer tv:// -tv driver=v4l2:device="$VID_USB" \
   -vo fbdev2 -vf scale=800:480


