#! /bin/bash

# This script is called when the transmitter is stopped.
# It can be added to for any purpose, and is not overwritten on software update

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
JCONFIGFILE="/home/pi/rpidatv/scripts/jetson_config.txt"

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


exit
