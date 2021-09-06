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

# Read from receiver config file
SYMBOLRATEK=$(get_config_var sr0 $RCONFIGFILE)
SYMBOLRATEK_T=$(get_config_var sr1 $RCONFIGFILE)
FREQ_KHZ=$(get_config_var freq0 $RCONFIGFILE)
FREQ_KHZ_T=$(get_config_var freq1 $RCONFIGFILE)
RX_MODE=$(get_config_var mode $RCONFIGFILE)
Q_OFFSET=$(get_config_var qoffset $RCONFIGFILE)
AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)
INPUT_SEL=$(get_config_var input $RCONFIGFILE)
INPUT_SEL_T=$(get_config_var input1 $RCONFIGFILE)
LNBVOLTS=$(get_config_var lnbvolts $RCONFIGFILE)
TSTIMEOUT=$(get_config_var tstimeout $RCONFIGFILE)
TSTIMEOUT_T=$(get_config_var tstimeout1 $RCONFIGFILE)
SCANWIDTH=$(get_config_var scanwidth $RCONFIGFILE)
SCANWIDTH_T=$(get_config_var scanwidth1 $RCONFIGFILE)
CHAN=$(get_config_var chan $RCONFIGFILE)
CHAN_T=$(get_config_var chan1 $RCONFIGFILE)
VLCVOLUME=$(get_config_var vlcvolume $PCONFIGFILE)

# Correct for LNB LO Frequency if required
if [ "$RX_MODE" == "sat" ]; then
  let FREQ_KHZ=$FREQ_KHZ-$Q_OFFSET
else
  FREQ_KHZ=$FREQ_KHZ_T
  SYMBOLRATEK=$SYMBOLRATEK_T
  INPUT_SEL=$INPUT_SEL_T
  TSTIMEOUT=$TSTIMEOUT_T
  SCANWIDTH=$SCANWIDTH_T
  CHAN=$CHAN_T
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

# Select the correct tuner input
INPUT_CMD=" "
if [ "$INPUT_SEL" == "b" ]; then
  INPUT_CMD="-w"
fi

# Set the LNB Volts
VOLTS_CMD=" "
if [ "$LNBVOLTS" == "h" ]; then
  VOLTS_CMD="-p h"
fi
if [ "$LNBVOLTS" == "v" ]; then
  VOLTS_CMD="-p v"
fi

# Select the correct channel in the stream
PROG=" "
if [ "$CHAN" != "0" ] && [ "$CHAN" != "" ]; then
  PROG="--program "$CHAN
fi

# Adjust timeout for low SRs
if [[ $SYMBOLRATEK -lt 300 ]] && [[ $TSTIMEOUT -ne -1 ]]; then
  let TSTIMEOUT=2*$TSTIMEOUT
  if [[ $SYMBOLRATEK -lt 150 ]]; then
    let TSTIMEOUT=2*$TSTIMEOUT
  fi
fi

TIMEOUT_CMD=" "
if [[ $TSTIMEOUT -ge 500 ]] || [[ $TSTIMEOUT -eq -1 ]]; then
  TIMEOUT_CMD=" -r "$TSTIMEOUT" "
fi

SCAN_CMD=" "
if [[ $SCANWIDTH -ge 10 ]] && [[ $SCANWIDTH -lt 250 ]]; then
  SCAN_CMD=`echo - | awk '{print '$SCANWIDTH' / 100}'`
  SCAN_CMD="-S "$SCAN_CMD
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

sudo killall longmynd >/dev/null 2>/dev/null
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

# Create the ts fifo
sudo rm longmynd_main_ts >/dev/null 2>/dev/null
mkfifo longmynd_main_ts

UDPIP=127.0.0.1
UDPPORT=1234

# Start LongMynd
sudo /home/pi/longmynd/longmynd -i $UDPIP $UDPPORT -s longmynd_status_fifo \
  $VOLTS_CMD $TIMEOUT_CMD -A 11 $SCAN_CMD $INPUT_CMD $FREQ_KHZ $SYMBOLRATEK >/dev/null 2>/dev/null &

# Start VLC
cvlc -I rc --rc-host 127.0.0.1:1111 $PROG --codec ffmpeg -f --video-title-timeout=100 \
  --width 800 --height 480 \
  --sub-filter marq --marq-x 25 --marq-file "/home/pi/tmp/vlc_overlay.txt" \
  --gain 3 --alsa-audio-device $AUDIO_DEVICE \
  udp://@127.0.0.1:1234 >/dev/null 2>/dev/null &

sleep 1

# Set the start-up volume
printf "volume "$VLCVOLUME"\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

exit


