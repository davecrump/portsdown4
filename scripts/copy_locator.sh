## Updated for version 20180515

## Usage ########################################

# This file copies the user's locator from portsdown_config.txt into
# the portsdown_locator.txt file.

####### Set Environment Variables ###############

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PLOCATORSFILE="/home/pi/rpidatv/scripts/portsdown_locators.txt"

####### Define Functions ########################

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

####### Transfer Locator ###############

TRANSFER=$(get_config_var locator $PCONFIGFILE)
set_config_var mylocator "$TRANSFER" $PLOCATORSFILE

####### Now return to update.sh ##########
