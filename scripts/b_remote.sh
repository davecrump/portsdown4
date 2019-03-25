#!/bin/bash

# set -x

# This script is called from a Windows computer using plink.exe
# and stops transmission and drops PTT for LimeSDR

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
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

echo "**************************************"
echo "Calling b.sh to stop transmit"
echo "**************************************"

rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

# Create the banner image in the tempfs folder
convert -size 720x576 xc:white \
  -gravity North -pointsize 40 -annotate 0 "\nRemote Control" \
  -gravity Center -pointsize 50 -annotate 0 "$1""\n\nTX Off" \
  -gravity South -pointsize 50 -annotate 0 "Portsdown is in Standby" \
  /home/pi/tmp/update.jpg

# Display the update message on the desktop
sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

sudo killall a_remote.sh

source /home/pi/rpidatv/scripts/b.sh

# Check user-requested display type
DISPLAY=$(get_config_var display $PCONFIGFILE)

# If framebuffer copy is not already running, start it for non-Element 14 displays
if [ "$DISPLAY" != "Element14_7" ]; then
  ps -cax | grep 'fbcp' >/dev/null 2>/dev/null
  RESULT="$?"
  if [ "$RESULT" -ne 0 ]; then
    fbcp &
  fi
fi

# Make sure that the gui is not running
sudo killall rpidatvgui

# Read the desired start-up behaviour
MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)

sleep 2

if [ "$MODE_STARTUP" == "Display_boot" ]; then # restart the gui
  /home/pi/rpidatv/scripts/scheduler.sh &
fi

exit



