#!/usr/bin/env bash

# set -x

# Run at Ryde install and when LongMynd audio output is changed to synchronise Ryde audio output

############ Set Environment Variables ###############

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
LCONFIGFILE="/home/pi/rpidatv/scripts/longmynd_config.txt"

############ Function to Read from Config File ###############

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

##################################################################

# Look up LongMynd audio output
AUDIO_JACK=$(get_config_var audio $LCONFIGFILE)

# Edit Ryde Python code as approptiate
case "$AUDIO_JACK" in
  "hdmi")
    sed -i "/--alsa-audio-device/c\#        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Headphones,DEV=0 '" \
      /home/pi/ryde/rydeplayer/player.py
  ;;
  "rpi")
    sed -i "/--alsa-audio-device/c\        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Headphones,DEV=0 '" \
      /home/pi/ryde/rydeplayer/player.py
  ;;
  "usb")
    sed -i "/--alsa-audio-device/c\        vlcArgs += '--gain 4 --alsa-audio-device hw:CARD=Device,DEV=0 '" \
      /home/pi/ryde/rydeplayer/player.py
  ;;
esac

exit

