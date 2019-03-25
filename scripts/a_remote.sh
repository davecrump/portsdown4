#!/bin/bash

# set -x  # Uncomment for testing

# This script is called from a Windows computer using plink.exe
# and sets everything up for, and starts transmission

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

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

######################### Start here #####################

SESSION_TYPE="ssh"

rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

# Create the banner image in the tempfs folder
convert -size 720x576 xc:white \
  -gravity North -pointsize 40 -annotate 0 "\nREMOTE CONTROL" \
  -gravity Center -pointsize 50 -annotate 0 "$1""\n\nTX On" \
  -gravity South -pointsize 50 -annotate 0 "Portsdown is Transmitting" \
  /home/pi/tmp/update.jpg

# Display the banner on the desktop
sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

# stop the gui
# Once gui dynamically updates, this can be removed
killall -9 rpidatvgui >/dev/null 2>/dev/null
killall siggen >/dev/null 2>/dev/null

# Check user-requested display type
DISPLAY=$(get_config_var display $PCONFIGFILE)

# Read the output mode
MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)

# If framebuffer copy is not already running, start it for non-Element 14 displays
if [ "$DISPLAY" != "Element14_7" ]; then
  ps -cax | grep 'fbcp' >/dev/null 2>/dev/null
  RESULT="$?"
  if [ "$RESULT" -ne 0 ]; then
    fbcp &
  fi
fi

# Turn the PTT on after a delay for the Lime
# rpidatv turns it on for other modes
if [ "$MODE_OUTPUT" == "LIMEMINI" ]; then
  /home/pi/rpidatv/scripts/lime_ptt.sh &
fi
if [ "$MODE_OUTPUT" == "LIMEUSB" ]; then
  /home/pi/rpidatv/scripts/lime_ptt.sh &
fi

# Start the Transmit processes
source /home/pi/rpidatv/scripts/a.sh & 

# And keep this process running
# Until it is killed by b_remote.sh
while [ 1 -eq 1 ]
do
  sleep 1
done
