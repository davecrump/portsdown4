#!/bin/bash

# Called by streamrx to start the stream receiver

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
########################################################################

cd /home/pi

# Read from receiver config file
AUDIO_OUT=$(get_config_var audio $RCONFIGFILE)
VLCVOLUME=$(get_config_var vlcvolume $PCONFIGFILE)

# Read in URL argument
STREAMURL=$1

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
  echo shutdown | nc 127.0.0.1 1111  >/dev/null 2>/dev/null
fi

# Start VLC

cvlc -I rc --rc-host 127.0.0.1:1111 --codec ffmpeg -f --video-title-timeout=100 \
  --width 800 --height 480 \
  --gain 3 --alsa-audio-device hw:CARD=Device,DEV=0 \
  $STREAMURL >/dev/null 2>/dev/null &

rm  /home/pi/tmp/stream_status.txt >/dev/null 2>/dev/null
# echo Stream not Running
echo "1" > /home/pi/tmp/stream_status.txt
sleep 1

# Set the start-up volume
printf "volume "$VLCVOLUME"\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

STATE="NOT_RUNNING"

  VLCRESPONSE=$(printf "stats\n" | nc -w 1 127.0.0.1 1111 | grep 'video')

  VLCRESPONSE1=$VLCRESPONSE
  VLCRESPONSE2=$VLCRESPONSE1
  VLCRESPONSE3=$VLCRESPONSE2

  # echo $VLCRESPONSE

KICKCOUNT=0

for (( ; ; ))
do
  sleep 0.5
  VLCRESPONSE=$(printf "stats\n" | nc -w 1 127.0.0.1 1111 | grep 'video')
  RESPONSE_SIZE=${#VLCRESPONSE}  # Make sure that all null responses are identical
  if [ "$RESPONSE_SIZE" -lt 5 ]; then
    # echo response size less than 5 = $RESPONSE_SIZE
    VLCRESPONSE=" "
    VLCRESPONSE1=" "
    VLCRESPONSE2=" "
  fi

  # echo $VLCRESPONSE

  VLCRESPONSE3=$VLCRESPONSE2
  VLCRESPONSE2=$VLCRESPONSE1
  VLCRESPONSE1=$VLCRESPONSE

  if [ $STATE == "NOT_RUNNING" ] && [ "$VLCRESPONSE2" != "$VLCRESPONSE" ]; then
    # Was not running but video counter has just incremented so start
    STATE=RUNNING
    #echo Stream Running
    KICKCOUNT=0
    echo "Video" > /home/pi/tmp/stream_status.txt
  fi
  if [ $STATE == "RUNNING" ] && [ "$VLCRESPONSE3" == "$VLCRESPONSE" ]; then
    # Was running but video counter has frozen so stop
    STATE=NOT_RUNNING
    echo "3" > /home/pi/tmp/stream_status.txt  # Turn the Stream off
    VLCRESPONSE1=$VLCRESPONSE
    VLCRESPONSE2=$VLCRESPONSE1
    VLCRESPONSE3=$VLCRESPONSE2                 # Make sure that it appears turned off

    # echo Stream not Running
  fi

  if [ $STATE == "NOT_RUNNING" ]; then
    if [ "$KICKCOUNT" -gt "8" ]; then
      printf "next\nlogout\n" | nc 127.0.0.1 1111 >/dev/null 2>/dev/null
      KICKCOUNT=0
    fi
    let KICKCOUNT=$KICKCOUNT+1
    #echo $KICKCOUNT
  fi
done

echo "3" > /home/pi/tmp/stream_status.txt  # Make sure that the stream is tuned off

exit


