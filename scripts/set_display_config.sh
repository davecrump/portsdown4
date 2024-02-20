#!/bin/bash

# Called from the main gui to change the display if touchscreen is not detected

# set -x

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
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

##########################################################################

# Read the display to be set
chdisplay=$(get_config_var display $PCONFIGFILE)

## Set constants for the amendment of /boot/config.txt
lead='^## Begin LCD Driver'               ## Marker for start of inserted text
tail='^## End LCD Driver'                 ## Marker for end of inserted text
CHANGEFILE="/boot/config.txt"             ## File requiring added text
APPENDFILE=$PATHCONFIGS"/lcd_markers.txt" ## File containing both markers
TRANSFILE=$PATHCONFIGS"/transfer.txt"     ## File used for transfer

grep -q "$lead" "$CHANGEFILE"     ## Is the first marker already present?
if [ $? -ne 0 ]; then
  sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
fi

case "$chdisplay" in              ## Select the correct driver text
  HDMITouch) INSERTFILE=$PATHCONFIGS"/hdmitouch.txt" ;;
  Console)   INSERTFILE=$PATHCONFIGS"/console.txt" ;;
  Element14_7)  INSERTFILE=$PATHCONFIGS"/element14_7.txt" ;;
  dfrobot5)  INSERTFILE=$PATHCONFIGS"/dfrobot5.txt" ;;
  Browser)  INSERTFILE=$PATHCONFIGS"/browser.txt" ;;
  hdmi)  INSERTFILE=$PATHCONFIGS"/hdmi.txt" ;;
  hdmi480)  INSERTFILE=$PATHCONFIGS"/hdmi480.txt" ;;
  hdmi720)  INSERTFILE=$PATHCONFIGS"/hdmi720.txt" ;;
  hdmi1080)  INSERTFILE=$PATHCONFIGS"/hdmi1080.txt" ;;
esac

## Replace whatever is between the markers with the driver text
    sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
	        }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
rm $TRANSFILE                               ## Delete the transfer file

## Turn off the overscan
grep -q '^disable_overscan=1' /boot/config.txt                               ## Is the overscan not disabled?
if [ $? -ne 0 ]; then
  sudo sed -i 's/#disable_overscan=1/disable_overscan=1/g' /boot/config.txt  ## so disable the overscan
fi

exit
