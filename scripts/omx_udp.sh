#!/bin/bash

# Called by rpidatvtouch to start the player for IPTS and return stdout
# in a status file

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
########################################################################


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

USBOUT_AUDIO_DEV="$(aplay -l 2> /dev/null | grep 'USB Audio' | cut -c6-6)"

if [ "$USBOUT_AUDIO_DEV" == '' ]; then
  printf "USB Dongle audio device was not found, setting to RPi Jack\n"
  USBOUT_AUDIO_DEV=$RPIJ_AUDIO_DEV
fi

# Take only the first character
USBOUT_AUDIO_DEV="$(echo $USBOUT_AUDIO_DEV | cut -c1-1)"

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

stdbuf -oL omxplayer --video_queue 0.5 --timeout 5 --vol 600 --adev alsa:plughw:"$AUDIO_OUT_DEV",0 --layer 6 udp://:@:10000 2>/dev/null | {
LINE="1"
rm  /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null
while IFS= read -r line
do
  # echo $line
  if [ "$LINE" = "1" ]; then
    echo "$line" >> /home/pi/tmp/stream_status.txt
    LINE="2"  
  fi
done
# Exits loop when omxplayer is killed or a stream stops

rm  /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null
echo "$line" >> /home/pi/tmp/stream_status.txt
}