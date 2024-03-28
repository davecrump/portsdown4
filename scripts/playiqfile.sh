#!/bin/bash

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

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

# Read the input flags
#while getopts i:a:f: flag
while getopts i: flag
do
    case "${flag}" in
        i) FILE=${OPTARG};;
#        f) FREQ=${OPTARG};;
#        f) =${OPTARG};;
    esac
done

FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)

LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
# Make sure Lime gain is sensible
if [ "$LIME_GAIN" -lt 6 ]; then
  LIMEGAIN=6
fi
LIME_GAINF=`echo - | awk '{print '$LIME_GAIN' / 100}'`

/home/pi/rpidatv/bin/wav2lime -i $FILE -f $FREQ_OUTPUT -g $LIME_GAINF
#/home/pi/rpidatv/bin/wav642lime -i $FILE -f 999.0 -g 0.9

echo Finished

exit