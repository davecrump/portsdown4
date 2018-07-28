#!/bin/bash

# Called by streamrx to start the stream receiver and return stdout
# in a status file

# Read in URL argument
STREAMURL=$1

# stdbuf -oL omxplayer --timeout 2 $STREAMURL 2>/dev/null | {

stdbuf -oL omxplayer --timeout 2 $STREAMURL 2>/dev/null | {
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