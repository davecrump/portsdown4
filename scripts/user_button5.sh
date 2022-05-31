#!/bin/bash

# This script is activated by "Button 5" on the Test Equipment Menu
#
# It overwrites the current locator and the contest numbers for each band

############ Define Locator and Contest Codes ###########

########### Edit this section of the file ONLY ##########

# Locator (can be 6, 8 or 10 characters)
LOCATOR=IO90LU

# Direct band numbers:

D0_50__MHz_NUMBERS=0000
D1_71__MHz_NUMBERS=1111
D2_146_MHz_NUMBERS=2222
D3_437_MHz_NUMBERS=3333
D4_1255MHz_NUMBERS=4444
D5_2395MHz_NUMBERS=5555
D6_3401MHz_NUMBERS=6666

# Transverter band numbers

T0_50___MHz_NUMBERS=2000
T1_3401_MHz_NUMBERS=2111
T2_5761_MHz_NUMBERS=0222
T3_10369MHz_NUMBERS=2333
T4_24049MHz_NUMBERS=2444
T5_2395_MHz_NUMBERS=2555
T6_47089MHz_NUMBERS=2666
T7_75977MHz_NUMBERS=2777
T8_SPAREMHz_NUMBERS=2888

########## Do not edit anything below this line! ##############

############ Set Environment Variables ########################

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATH_PPRESETS="/home/pi/rpidatv/scripts/portsdown_presets.txt"
PATH_LOCATORS="/home/pi/rpidatv/scripts/portsdown_locators.txt"

############ Function to Write to Config File #################

set_config_var() {
lua - "$1" "$2" "$3" <<EOF > "$3.bak"
local key=assert(arg[1])
local value=assert(arg[2])
local fn=assert(arg[3])
local file=assert(io.open(fn))
local made_change=false
for line in file:lines() do
if line:match("^#?%s*"..key.."=.*$") then
line=key.."="..value
made_change=true
end
print(line)
end
if not made_change then
print(key.."="..value)
end
EOF
mv "$3.bak" "$3"
}

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

############################################################

# Set the new locator in the config file and the range and bearing file
set_config_var locator "$LOCATOR" $PCONFIGFILE
set_config_var mylocator "$LOCATOR" $PATH_LOCATORS

# Set the new Contest Numbers in the presets file
set_config_var d0numbers "$D0_50__MHz_NUMBERS" $PATH_PPRESETS
set_config_var d1numbers "$D1_71__MHz_NUMBERS" $PATH_PPRESETS
set_config_var d2numbers "$D2_146_MHz_NUMBERS" $PATH_PPRESETS
set_config_var d3numbers "$D3_437_MHz_NUMBERS" $PATH_PPRESETS
set_config_var d4numbers "$D4_1255MHz_NUMBERS" $PATH_PPRESETS
set_config_var d5numbers "$D5_2395MHz_NUMBERS" $PATH_PPRESETS
set_config_var d6numbers "$D6_3401MHz_NUMBERS" $PATH_PPRESETS

set_config_var t0numbers "$T0_50___MHz_NUMBERS" $PATH_PPRESETS
set_config_var t1numbers "$T1_3401_MHz_NUMBERS" $PATH_PPRESETS
set_config_var t2numbers "$T2_5761_MHz_NUMBERS" $PATH_PPRESETS
set_config_var t3numbers "$T3_10369MHz_NUMBERS" $PATH_PPRESETS
set_config_var t4numbers "$T4_24049MHz_NUMBERS" $PATH_PPRESETS
set_config_var t5numbers "$T5_2395_MHz_NUMBERS" $PATH_PPRESETS
set_config_var t6numbers "$T6_47089MHz_NUMBERS" $PATH_PPRESETS
set_config_var t7numbers "$T7_75977MHz_NUMBERS" $PATH_PPRESETS
set_config_var t8numbers "$T8_SPAREMHz_NUMBERS" $PATH_PPRESETS

# Set the new contest numbers for the current band
CURRENT_BAND=$(get_config_var band $PCONFIGFILE)
case "$CURRENT_BAND" in
  d0)
    set_config_var numbers "$D0_50__MHz_NUMBERS" $PCONFIGFILE
  ;;
  d1)
    set_config_var numbers "$D1_71__MHz_NUMBERS" $PCONFIGFILE
  ;;
  d2)
    set_config_var numbers "$D2_146_MHz_NUMBERS" $PCONFIGFILE
  ;;
  d3)
    set_config_var numbers "$D3_437_MHz_NUMBERS" $PCONFIGFILE
  ;;
  d4)
    set_config_var numbers "$D4_1255MHz_NUMBERS" $PCONFIGFILE
  ;;
  d5)
    set_config_var numbers "$D5_2395MHz_NUMBERS" $PCONFIGFILE
  ;;
  d6)
    set_config_var numbers "$D6_3401MHz_NUMBERS" $PCONFIGFILE
  ;;
  t0)
    set_config_var numbers "$T0_50___MHz_NUMBERS" $PCONFIGFILE
  ;;
  t1)
    set_config_var numbers "$T1_3401_MHz_NUMBERS" $PCONFIGFILE
  ;;
  t2)
    set_config_var numbers "$T2_5761_MHz_NUMBERS" $PCONFIGFILE
  ;;
  t3)
    set_config_var numbers "$T3_10369MHz_NUMBERS" $PCONFIGFILE
  ;;
  t4)
    set_config_var numbers "$T4_24049MHz_NUMBERS" $PCONFIGFILE
  ;;
  t5)
    set_config_var numbers "$T5_2395_MHz_NUMBERS" $PCONFIGFILE
  ;;
  t6)
    set_config_var numbers "$T6_47089MHz_NUMBERS" $PCONFIGFILE
  ;;
  t7)
    set_config_var numbers "$T7_75977MHz_NUMBERS" $PCONFIGFILE
  ;;
  t8)
    set_config_var numbers "$T8_SPAREMHz_NUMBERS" $PCONFIGFILE
  ;;
esac

exit




