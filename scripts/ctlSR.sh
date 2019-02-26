#!/bin/bash

# set -x

########## ctlSR.sh ############

# Called by b.sh, Menu.sh and rpidatvgui to reset nyquist filter settings after
# LimeSDR transmission
# Written by Dave G8GKQ 20190224.

# SR Outputs:

# <130   000
# <260   001
# <360   010
# <550   011
# <1100  100
# <2200  101
# >=2200 110

# Non integer frequencies are rounded down

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

############### PIN DEFINITIONS ###########

#filter_bit0 LSB of filter control word = BCM 16 / Header 36  
filter_bit0=16

#filter_bit1 Mid Bit of filter control word = BCM 26 / Header 37
filter_bit1=26

#filter_bit0 MSB of filter control word = BCM 20 / Header 38
filter_bit2=20

# Set all as outputs
gpio -g mode $filter_bit0 out
gpio -g mode $filter_bit1 out
gpio -g mode $filter_bit2 out

############### Function to read Config File ###############

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

############### Read Symbol Rate #########################

SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)

############### Switch GPIOs based on Symbol Rate ########

if (( $SYMBOLRATEK \< 130 )); then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 0;
elif (( $SYMBOLRATEK \< 260 )); then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 0;
elif (( $SYMBOLRATEK \< 360 )); then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 0;
elif (( $SYMBOLRATEK \< 550 )); then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 0;
elif (( $SYMBOLRATEK \< 1100 )); then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 1;
elif (( $SYMBOLRATEK \< 2200 )); then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 1;
else
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 1;
fi

### End ###

# Revert to rpidatvgui, menu.sh or b.sh #

