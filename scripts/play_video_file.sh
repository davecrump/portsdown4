#!/bin/bash

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
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

cd /home/pi

# Read audio output socket and volume

AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)
VLCVOLUME=$(get_config_var vlcvolume $PCONFIGFILE)
FILETOPLAY=$1

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

# Error check volume
if [ "$VLCVOLUME" -lt 0 ]; then
  VLCVOLUME=0
fi
if [ "$VLCVOLUME" -gt 512 ]; then
  VLCVOLUME=512
fi

# Create dummy marquee overlay file
echo " " >/home/pi/tmp/vlc_overlay.txt

sudo killall vlc >/dev/null 2>/dev/null

# Play a very short dummy file if this is a first start for VLC since boot
# This makes sure the RX works on first selection after boot
if [[ ! -f /home/pi/tmp/vlcprimed ]]; then
  cvlc -I rc --rc-host 127.0.0.1:1111 -f --codec ffmpeg --video-title-timeout=100 \
    --width 800 --height 480 \
    --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
    --gain 3 --alsa-audio-device $AUDIO_DEVICE \
    /home/pi/rpidatv/video/blank.ts vlc:quit >/dev/null 2>/dev/null &
  sleep 1
  touch /home/pi/tmp/vlcprimed
  echo shutdown | nc 127.0.0.1 1111
fi

# Start VLC
#cvlc -I rc --rc-host 127.0.0.1:1111 -f --video-title-timeout=100 \
cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
  --width 800 --height 480 --play-and-exit \
  --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
  --gain 3 --alsa-audio-device $AUDIO_DEVICE \
  "$FILETOPLAY" >/dev/null 2>/dev/null &

sleep 1

# Set the start-up volume
printf "volume "$VLCVOLUME"\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

exit


