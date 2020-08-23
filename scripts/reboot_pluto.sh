#!/usr/bin/env bash

# set -x

# This script is called from the GUI to reboot the Pluto if required by the user

############ Set Environment Variables ###############

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

##############################################################

# Look up the Pluto IP.  Note that pluto.local does not work for sorting out the ssh-keygen
PLUTOIP=$(get_config_var plutoip $PCONFIGFILE)

# Make sure that we will be able to log in
ssh-keygen -f "/home/pi/.ssh/known_hosts" -R "$PLUTOIP"

# Tell the Pluto to reboot
timeout 2 sshpass -p analog ssh -o StrictHostKeyChecking=no root@"$PLUTOIP" 'reboot'

exit
