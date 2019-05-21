#!/bin/bash

# This script remotely reboots down the Jetson nano
# It composes the ssh and shutdown commands in a temp file as
# it needs to resolve local variables
# Then it sources the temp file.

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

# Read environment variables and passwords

JCONFIGFILE="/home/pi/rpidatv/scripts/jetson_config.txt"

JETSONIP=$(get_config_var jetsonip $JCONFIGFILE)
JETSONUSER=$(get_config_var jetsonuser $JCONFIGFILE)
JETSONPW=$(get_config_var jetsonpw $JCONFIGFILE)
JETSONROOTPW=$(get_config_var jetsonrootpw $JCONFIGFILE)
CMDFILE="/home/pi/tmp/jetson_command.txt"

# Write the assembled Jetson command to a temp file
/bin/cat <<EOM >$CMDFILE
sshpass -p "$JETSONPW" ssh -t -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
echo "$JETSONROOTPW" | sudo -S reboot now
ENDSSH
EOM

# Run the Command on the Jetson
source "$CMDFILE"

