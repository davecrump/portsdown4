#!/bin/bash

# Script to run Combituner on the Portsdown V1

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

# Read from receiver config file
SYMBOLRATEK=$(get_config_var sr0 $RCONFIGFILE)
SYMBOLRATEK_T=$(get_config_var sr1 $RCONFIGFILE)
FREQ_KHZ=$(get_config_var freq0 $RCONFIGFILE)
FREQ_KHZ_T=$(get_config_var freq1 $RCONFIGFILE)
RX_MODE=$(get_config_var mode $RCONFIGFILE)
Q_OFFSET=$(get_config_var qoffset $RCONFIGFILE)
AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)

# Correct for LNB LO Frequency if required
if [ "$RX_MODE" == "sat" ]; then
  let FREQ_KHZ=$FREQ_KHZ-$Q_OFFSET
else
  FREQ_KHZ=$FREQ_KHZ_T
  SYMBOLRATEK=$SYMBOLRATEK_T
  INPUT_SEL=$INPUT_SEL_T
fi

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

# Create dummy marquee overlay file
echo " " >/home/pi/tmp/vlc_overlay.txt

sudo killall CombiTunerExpress >/dev/null 2>/dev/null
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

# Create the status fifo
sudo rm longmynd_status_fifo >/dev/null 2>/dev/null
mkfifo longmynd_status_fifo

# Make sure that the status FIFO gets read (usually read by the GUI)
#/home/pi/longmynd/fake_read &

# Start Combituner, and set buffer to output STDOUT one line at a time
stdbuf -oL /home/pi/rpidatv/bin/CombiTunerExpress -m dvbt -f $FREQ_KHZ -b $SYMBOLRATEK >>longmynd_status_fifo 2>/dev/null &

# Start VLC
cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
  --width 800 --height 480 \
  --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
  --gain 3 --alsa-audio-device $AUDIO_DEVICE \
  udp://@127.0.0.1:1314 >/dev/null 2>/dev/null &


