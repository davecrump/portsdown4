#!/bin/bash

# Updated 201802040

########## ctlvco.sh ############

# Called by a.sh in IQ mode to set ADF4351 vco 
# to correct frequency with correct ref freq and level

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATH_ATTEN="/home/pi/rpidatv/bin/set_attenuator "

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

########### Read Frequency and Ref Frequency ###############

FREQM=$(get_config_var freqoutput $PCONFIGFILE)
FREQR=$(get_config_var adfref $PCONFIGFILE)
ATTENUATOR=$(get_config_var attenuator $PCONFIGFILE)
ATTENLEVEL=$(get_config_var attenlevel $PCONFIGFILE)

################ Set Attenuator Level #####################

#Change ATTENLEVEL sign if not 0 
if (( $(bc <<< "$ATTENLEVEL < 0") )); then
  ATTENLEVEL=${ATTENLEVEL:1}
else
  ATTENLEVEL=0.00
fi

case "$ATTENUATOR" in
NONE)
  :
;;
PE4312)
  sudo $PATH_ATTEN PE4312 "$ATTENLEVEL"
;;
PE43713)
  sudo $PATH_ATTEN PE43713 "$ATTENLEVEL"
;;
HMC1119)
  sudo $PATH_ATTEN HMC1119 "$ATTENLEVEL"
;;
esac

############### Call binary to set frequency ########

PWR="0";
sudo $PATHRPI"/adf4351" $FREQM $FREQR $PWR

### End ###

# Revert to a.sh #

