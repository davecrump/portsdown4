#!/bin/bash

# set -x

########## ctlfilter.sh ############

# Called by a.sh in IQ and DATVEXPRESS modes to switch in correct
# Nyquist Filter, band/transverter switching and attenuator level
# Also sets Pin 21 (BCM 9, WiringPi 13) low for Portsdown/Langstone switching
# Written by Dave G8GKQ 20170209.  Last amended 202111100

# SR Outputs:

# <130   000
# <260   001
# <360   010
# <550   011
# <1100  100
# <2200  101
# >=2200 110

# Band Outputs:

# <100   00  (71 MHz)
# <250   01  (146.5 MHz)
# <950   10  (437 MHz)
# <4400  11  (1255 MHz)

# DATV Express Switching

# <100   0  (71 MHz)
# <250   1  (146.5 MHz)
# <950   2  (437 MHz)
# <20000 3  (1255 MHz)
# <4400  4  (2400 MHz


# Non integer frequencies are rounded down

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATH_ATTEN="/home/pi/rpidatv/bin/set_attenuator "

############### PIN DEFINITIONS ###########

#filter_bit0 LSB of filter control word = BCM 16 / Header 36  
filter_bit0=16

#filter_bit1 Mid Bit of filter control word = BCM 26 / Header 37
filter_bit1=26

#filter_bit0 MSB of filter control word = BCM 20 / Header 38
filter_bit2=20

#band_bit_0 LSB of band switching word = BCM 1 / Header 28  Changed for RPi 4
#band_bit0=1
#band_bit_0 LSB of band switching word = BCM 1 / Header 32  Changed for RPi 4
band_bit0=12

#band_bit_1 MSB of band switching word = BCM 19 / Header 35
band_bit1=19

#band_bit_3 MSB of band switching word = BCM 25 / Header 22
band_bit3=25

#tverter_bit: 0 for direct, 1 for transverter = BCM 4 / Header 7
tverter_bit=4

# pdown bit: 0 for Portsdown, 1 for Langstone = BCM 9 / Header 21
pdown_bit=9

# Set all as outputs
gpio -g mode $filter_bit0 out
gpio -g mode $filter_bit1 out
gpio -g mode $filter_bit2 out
gpio -g mode $band_bit0 out
gpio -g mode $band_bit1 out
gpio -g mode $tverter_bit out
gpio -g mode $band_bit3 out
gpio -g mode $pdown_bit out

# Set Portsdown id bit low
gpio -g write $pdown_bit 0

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

if [ "$SYMBOLRATEK" -lt "130" ] ; then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 0;
elif [ "$SYMBOLRATEK" -lt "260" ] ; then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 0;
elif [ "$SYMBOLRATEK" -lt "360" ] ; then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 0;
elif [ "$SYMBOLRATEK" -lt "550" ] ; then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 0;
elif [ "$SYMBOLRATEK" -lt "1100" ] ; then
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 1;
elif [ "$SYMBOLRATEK" -lt "2200" ] ; then
                gpio -g write $filter_bit0 1;
                gpio -g write $filter_bit1 0;
                gpio -g write $filter_bit2 1;
else
                gpio -g write $filter_bit0 0;
                gpio -g write $filter_bit1 1;
                gpio -g write $filter_bit2 1;
fi

############### Set band GPIOs ##############

EXPPORTS=$(get_config_var expports $PCONFIGFILE)
if [ "$EXPPORTS" -gt "15" ]; then  # Set in range 0 - 15.  16 to 31 was used in the past
    let EXPPORTS=$EXPPORTS-16
fi

# In Keyed TX modes, Band D2 is used to indicate TX, so don't set the band.
MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)
if [ "$MODE_STARTUP" == "Keyed_TX_boot" ] || [ "$MODE_STARTUP" == "Keyed_Stream_boot" ]\
   || [ "$MODE_STARTUP" == "Cont_Stream_boot" ]; then
      : # Do nothing
else
  case "$EXPPORTS" in
  "0" | "8" )
    gpio -g write $band_bit0 0;
    gpio -g write $band_bit1 0;
    gpio -g write $tverter_bit 0;
  ;;
  "1" | "9" )
    gpio -g write $band_bit0 1;
    gpio -g write $band_bit1 0;
    gpio -g write $tverter_bit 0;
  ;;
  "2" | "10" )
    gpio -g write $band_bit0 0;
    gpio -g write $band_bit1 1;
    gpio -g write $tverter_bit 0;
  ;;
  "3" | "11" )
    gpio -g write $band_bit0 1;
    gpio -g write $band_bit1 1;
    gpio -g write $tverter_bit 0;
  ;;
  "4" | "12" )
    gpio -g write $band_bit0 0;
    gpio -g write $band_bit1 0;
    gpio -g write $tverter_bit 1;
  ;;
  "5" | "13" )
    gpio -g write $band_bit0 1;
    gpio -g write $band_bit1 0;
    gpio -g write $tverter_bit 1;
  ;;
  "6" | "14" )
    gpio -g write $band_bit0 0;
    gpio -g write $band_bit1 1;
    gpio -g write $tverter_bit 1;
  ;;
  "7" | "15" )
    gpio -g write $band_bit0 1;
    gpio -g write $band_bit1 1;
    gpio -g write $tverter_bit 1;
  ;;
  esac

  if [ "$EXPPORTS" -gt "7" ]; then 
    gpio -g write $band_bit3 1;
  else
    gpio -g write $band_bit3 0;
  fi
fi

################ Set Attenuator Level #####################

ATTENUATOR=$(get_config_var attenuator $PCONFIGFILE)
ATTENLEVEL=$(get_config_var attenlevel $PCONFIGFILE)

#Change ATTENLEVEL sign if not 0 
if [ "$ATTENLEVEL" = "0" ] || [ "$ATTENLEVEL" = "0.0" ] || [ "$ATTENLEVEL" = "0.00" ] ; then
  ATTENLEVEL=0.00
else
  ATTENLEVEL=${ATTENLEVEL:1}
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

################ If DATV EXPRESS in use, Set Ports ########

MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
if [ "$MODE_OUTPUT" = "DATVEXPRESS" ] ; then
  echo "set port "$EXPPORTS >> /tmp/expctrl
fi

### End ###

# Revert to rpidatvgui, menu.sh or a.sh #

