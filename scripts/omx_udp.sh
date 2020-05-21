#!/bin/bash

# Called by rpidatvtouch to start the player for IPTS and return stdout
# in a status file

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

RCONFIGFILE="/home/pi/rpidatv/scripts/longmynd_config.txt"

# Send audio to the correct port (uses Port selected in RX Config)
if [ "$AUDIO_OUT" == "rpi" ]; then
  AUDIO_MODE="local"
else
  AUDIO_MODE="alsa:plughw:1,0"
fi

stdbuf -oL omxplayer --adev $AUDIO_MODE --video_queue 0.5 --timeout 5 udp://:@:10000 2>/dev/null | {
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