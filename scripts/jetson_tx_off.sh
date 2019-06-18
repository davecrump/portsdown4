#! /bin/bash

# set -x # Uncomment for testing

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


############### DEFINITIONS ###########

TXOFFCMDFILE="/home/pi/tmp/jetson_tx_off.txt"

JCONFIGFILE="/home/pi/rpidatv/scripts/jetson_config.txt"
JETSONIP=$(get_config_var jetsonip $JCONFIGFILE)
JETSONPW=$(get_config_var jetsonpw $JCONFIGFILE)
JETSONROOTPW=$(get_config_var jetsonrootpw $JCONFIGFILE)
JETSONUSER=$(get_config_var jetsonuser $JCONFIGFILE)

GPIO=78  # Pin 40

############# MAKE SURE THAT WE KNOW WHERE WE ARE ##################

cd ~

rm $TXOFFCMDFILE

######### Write the assembled Jetson command to a temp file ########
/bin/cat <<EOM >$TXOFFCMDFILE
  sshpass -p "$JETSONPW" ssh -t -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH'

    rm tx_off.sh

    echo "#! /bin/bash" >> tx_off.sh
    echo "cd /sys/class/gpio" >> tx_off.sh
    echo "echo $GPIO > export" >> tx_off.sh
    echo "cd /sys/class/gpio/gpio$GPIO" >> tx_off.sh
    echo "echo \"out\" > direction" >> tx_off.sh
    echo "echo 0 > value" >> tx_off.sh
    echo "exit"  >> tx_off.sh

    chmod +x tx_off.sh

    echo "$JETSONROOTPW" | sudo -S /home/nano/tx_off.sh

    exit

ENDSSH

  exit

EOM

# Run the Command on the Jetson
source "$TXOFFCMDFILE"

exit