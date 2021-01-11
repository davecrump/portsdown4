#!/bin/bash

# Script to run Combituner with UDP Output

PATHBIN="/home/pi/rpidatv/bin/"
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
UDPIP=$(get_config_var udpip $RCONFIGFILE)
UDPPORT=$(get_config_var udpport $RCONFIGFILE)

# Correct for LNB LO Frequency if required
if [ "$RX_MODE" == "sat" ]; then
  let FREQ_KHZ=$FREQ_KHZ-$Q_OFFSET
else
  FREQ_KHZ=$FREQ_KHZ_T
  SYMBOLRATEK=$SYMBOLRATEK_T
fi

sudo killall longmynd >/dev/null 2>/dev/null
sudo killall CombiTunerExpress >/dev/null 2>/dev/null
sudo killall vlc >/dev/null 2>/dev/null

# Start Combituner, and set buffer to output STDOUT one line at a time
stdbuf -oL /home/pi/rpidatv/bin/CombiTunerExpress -m dvbt -i $UDPIP -p $UDPPORT -f $FREQ_KHZ -b $SYMBOLRATEK >>longmynd_status_fifo 2>/dev/null &

# sudo /home/pi/longmynd/longmynd -i $UDPIP $UDPPORT -s longmynd_status_fifo $VOLTS_CMD $TIMEOUT_CMD $INPUT_CMD $FREQ_KHZ $SYMBOLRATEK &

exit







