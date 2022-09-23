#!/bin/bash

# Used to display the incoming video from an LKV373A or Elgato CamLink 4K

RCONFIGFILE="/home/pi/rpidatv/scripts/longmynd_config.txt"
JCONFIGFILE="/home/pi/rpidatv/scripts/jetson_config.txt"
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

cd /home/pi

# Read from receiver config file
AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)

# Read from Jetson config file
LKVUDP=$(get_config_var lkvudp $JCONFIGFILE)
LKVPORT=$(get_config_var lkvport $JCONFIGFILE)

# Send audio to the correct port
if [ "$AUDIO_OUT" == "rpi" ]; then
  # Check for latest Buster update
  aplay -l | grep -q 'bcm2835 Headphones'
  if [ $? == 0 ]; then
    AUDIO_DEVICE="hw:CARD=Headphones,DEV=0"
  else
    AUDIO_DEVICE="hw:CARD=ALSA,DEV=0"
  fi
else
  AUDIO_DEVICE="hw:CARD=Device,DEV=0"
fi
echo The audio output device string is $AUDIO_DEVICE

############ CHECK FOR CAMLINK 4K DEVICE STRING ###################################

# List the video devices, select the 2 lines for the Cam Link device, then
# select the line with the device details and delete the leading tab

VID_USB="$(v4l2-ctl --list-devices 2> /dev/null | \
  sed -n '/Cam Link 4K/,/dev/p' | grep 'dev' | tr -d '\t')"

############ IDENTIFY CAMLINK 4K AUDIO CARD NUMBER #############################

# List the audio capture devices, select the line for the Cam Link 4K device:
# card 2: C4K [Cam Link 4K], device 0: USB Audio [USB Audio]
# then take the 6th character

AUDIO_IN_CARD="$(arecord -l 2> /dev/null | grep 'Cam Link 4K' | cut -c6-6)"

if [ "$AUDIO_IN_CARD" == '' ]; then
  printf "EasyCap audio device was not found, setting to 1\n"
  AUDIO_IN_CARD="1"
fi
echo "The Cam Link 4K Audio Card number is "$AUDIO_IN_CARD":0"

#####################################################################

sudo killall vlc >/dev/null 2>/dev/null

# Play a very short dummy file if this is a first start for VLC since boot
# This makes sure the RX works on first selection after boot
if [[ ! -f /home/pi/tmp/vlcprimed ]]; then
  cvlc -I rc --rc-host 127.0.0.1:1111 -f --codec ffmpeg --video-title-timeout=100 \
    --width 800 --height 480 \
    --gain 3 --alsa-audio-device $AUDIO_DEVICE \
    /home/pi/rpidatv/video/blank.ts vlc:quit >/dev/null 2>/dev/null &
  sleep 1
  touch /home/pi/tmp/vlcprimed
  echo shutdown | nc 127.0.0.1 1111
fi

# Start VLC

if [ "$VID_USB" == '' ]; then

  echo Showing LKV input

  cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
    --width 800 --height 480 \
    --gain 3 --alsa-audio-device "$AUDIO_DEVICE" \
    udp://@"$LKVUDP":"$LKVPORT" >/dev/null 2>/dev/null &

  exit

else

  echo The Cam Link 4K Video Device string is $VID_USB

  cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
    --width 800 --height 480 \
    --gain 3 --alsa-audio-device "$AUDIO_DEVICE" \
    v4l2:///"$VID_USB" \
    --input-slave alsa://plughw:"$AUDIO_IN_CARD",0 \
    >/dev/null 2>/dev/null &

  exit

fi




