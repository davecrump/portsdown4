#!/bin/bash

# This script uses mplayer to display the video from the EasyCap.


# No scaling required for RPi 4!!

# set -x

RCONFIGFILE="/home/pi/rpidatv/scripts/longmynd_config.txt"

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


############ IDENTIFY USBTV VIDEO DEVICES #############################

# List the video devices, select the 2 lines for the usbtv device, then
# select the line with the device details and delete the leading tab

# Check for Fushicai USBTVOO7 (old BATC EasyCap)
VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/usbtv/,/dev/p' | grep 'dev' | tr -d '\t')"

# Check for MS2106 (new BATC EayCap) 
if [ "$VID_USB" == '' ]; then
  VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/AV TO USB2/,/dev/p' | grep 'dev' | tr -d '\t')"
fi

if [ "$VID_USB" == '' ]; then
  printf "VID_USB was not found, setting to /dev/video0\n"
fi



printf "The EasyCap USB device string is $VID_USB\n"

###########################################################################

############ IDENTIFY USBTV AUDIO CARD NUMBER #############################

# List the audio capture devices, select the line for the usbtv device:
# card 2: usbtv [usbtv], device 0: USBTV Audio [USBTV Audio Input]
# then take the 6th character

EC_AUDIO_DEV="$(arecord -l 2> /dev/null | grep 'usbtv' | cut -c6-6)"

if [ "$EC_AUDIO_DEV" == '' ]; then
  EC_AUDIO_DEV="$(arecord -l 2> /dev/null | grep 'AV TO USB2' | cut -c6-6)"
fi

if [ "$EC_AUDIO_DEV" == '' ]; then
  printf "EasyCap audio device was not found, setting to 1\n"
  EC_AUDIO_DEV="1"
fi
echo "The EasyCap Audio Card number is -"$EC_AUDIO_DEV"-"

############ IDENTIFY RPi JACK AUDIO CARD NUMBER #############################

# List the audio playback devices, select the line for the Headphones device:
# card 0: Headphones [bcm2835 Headphones], device 0: bcm2835 Headphones [bcm2835 Headphones]
# then take the 6th character

# If headphones not found, look for bcm2835 ALSA:
# card 0: ALSA [bcm2835 ALSA], device 0: bcm2835 ALSA [bcm2835 ALSA]
# and take 6th character

RPIJ_AUDIO_DEV="$(aplay -l 2> /dev/null | grep 'Headphones' | cut -c6-6)"

if [ "$RPIJ_AUDIO_DEV" == '' ]; then
  RPIJ_AUDIO_DEV="$(aplay -l 2> /dev/null | grep 'bcm2835 ALSA' | cut -c6-6)"
  if [ "$RPIJ_AUDIO_DEV" == '' ]; then
    printf "RPi Jack audio device was not found, setting to 0\n"
    RPIJ_AUDIO_DEV="0"
  fi
fi

# Take only the first character
RPIJ_AUDIO_DEV="$(echo $RPIJ_AUDIO_DEV | cut -c1-1)"

echo "The RPi Jack Audio Card number is -"$RPIJ_AUDIO_DEV"-"

############ IDENTIFY USB DONGLE AUDIO CARD NUMBER #############################

# List the audio playback devices, select the line for the audio dongle device:
# card 1: Device [USB Audio Device], device 0: USB Audio [USB Audio]
# then take the 6th character

USBOUT_AUDIO_DEV="$(aplay -l 2> /dev/null | grep 'USB Audio Device' | cut -c6-6)"

if [ "$USBOUT_AUDIO_DEV" == '' ]; then
  printf "USB Dongle audio device was not found, setting to 1\n"
  USBOUT_AUDIO_DEV="1"
fi
echo "The USB Dongle Audio Card number is -"$USBOUT_AUDIO_DEV"-"

############ CHOOSE THE AUDIO OUTPUT DEVICE #############################

AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)

# Send audio to the correct port
if [ "$AUDIO_OUT" == "rpi" ]; then
  AUDIO_OUT_DEV=$RPIJ_AUDIO_DEV
else
  AUDIO_OUT_DEV=$USBOUT_AUDIO_DEV
fi

echo "The Selected Audio Card number is -"$AUDIO_OUT_DEV"-"


###########################################################################


sudo killall mplayer >/dev/null 2>/dev/null

mplayer -nolirc tv:// -tv driver=v4l2:device="$VID_USB":norm=PAL:input=0:alsa:adevice=hw."$EC_AUDIO_DEV",0:amode=1:audiorate=48000:forceaudio:volume=100:immediatemode=0\
  -ao alsa:device=hw="$AUDIO_OUT_DEV".0 -vo fbdev2 -vf scale=800:480 &

# If a Fushicai EasyCap, adjust the contrast to prevent white crushing
lsusb | grep -q '1b71:3002'
if [ $? == 0 ]; then   ## Fushuicai USBTV007
  sleep 0.7
  v4l2-ctl -d "$VID_USB" --set-ctrl contrast=380 >/dev/null 2>/dev/null
fi


