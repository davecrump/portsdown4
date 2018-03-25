## Updated for version 20180204

## Usage ########################################

# This file copies the relevant entries for the old rpidatvconfig.txt into
# the portsdown_config.txt and portsdown_presets files.

####### Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
OLDCONFIGFILE=/home/pi/rpidatvconfig.txt

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
PATH_PPRESETS="/home/pi/rpidatv/scripts/portsdown_presets.txt"


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

####### Transfer each value to the New File ###############

TRANSFER=$(get_config_var modeinput $OLDCONFIGFILE)
set_config_var modeinput "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var symbolrate $OLDCONFIGFILE)
set_config_var symbolrate "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var fec $OLDCONFIGFILE)
set_config_var fec "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var freqoutput $OLDCONFIGFILE)
set_config_var freqoutput "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var rfpower $OLDCONFIGFILE)
set_config_var rfpower "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var modeoutput $OLDCONFIGFILE)
set_config_var modeoutput "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var call $OLDCONFIGFILE)
set_config_var call "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var pidvideo $OLDCONFIGFILE)
set_config_var pidvideo "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var pidpmt $OLDCONFIGFILE)
set_config_var pidpmt "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var serviceid $OLDCONFIGFILE)
set_config_var serviceid "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var locator $OLDCONFIGFILE)
set_config_var locator "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var pidstart $OLDCONFIGFILE)
set_config_var pidstart "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var pidaudio $OLDCONFIGFILE)
set_config_var pidaudio "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var display $OLDCONFIGFILE)
set_config_var display "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var menulanguage $OLDCONFIGFILE)
set_config_var menulanguage "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var analogcamname $OLDCONFIGFILE)
set_config_var analogcamname "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var startup $OLDCONFIGFILE)
set_config_var startup "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var analogcaminput $OLDCONFIGFILE)
set_config_var analogcaminput "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var analogcamstandard $OLDCONFIGFILE)
set_config_var analogcamstandard "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var pfreq1 $OLDCONFIGFILE)
set_config_var pfreq1 "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var pfreq2 $OLDCONFIGFILE)
set_config_var pfreq2 "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var pfreq3 $OLDCONFIGFILE)
set_config_var pfreq3 "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var pfreq4 $OLDCONFIGFILE)
set_config_var pfreq4 "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var pfreq5 $OLDCONFIGFILE)
set_config_var pfreq5 "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var adfref $OLDCONFIGFILE)
set_config_var adfref "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var audio $OLDCONFIGFILE)
set_config_var audio "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var explevel0 $OLDCONFIGFILE)
set_config_var d1explevel "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var explevel1 $OLDCONFIGFILE)
set_config_var d2explevel "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var explevel2 $OLDCONFIGFILE)
set_config_var d3explevel "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var explevel3 $OLDCONFIGFILE)
set_config_var d4explevel "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var explevel4 $OLDCONFIGFILE)
set_config_var d5explevel "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var expports0 $OLDCONFIGFILE)
set_config_var d1expports "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var expports1 $OLDCONFIGFILE)
set_config_var d2expports "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var expports2 $OLDCONFIGFILE)
set_config_var d3expports "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var expports3 $OLDCONFIGFILE)
set_config_var d4expports "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var expports4 $OLDCONFIGFILE)
set_config_var d5expports "$TRANSFER" $PATH_PPRESETS

TRANSFER=$(get_config_var psr1 $OLDCONFIGFILE)
set_config_var psr1 "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var psr2 $OLDCONFIGFILE)
set_config_var psr2 "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var psr3 $OLDCONFIGFILE)
set_config_var psr3 "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var psr4 $OLDCONFIGFILE)
set_config_var psr4 "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var psr5 $OLDCONFIGFILE)
set_config_var psr5 "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var batcoutput $OLDCONFIGFILE)
set_config_var batcoutput "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var streamurl $OLDCONFIGFILE)
set_config_var streamurl "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var streamkey $OLDCONFIGFILE)
set_config_var streamkey "$TRANSFER" $PCONFIGFILE

TRANSFER=$(get_config_var outputstandard $OLDCONFIGFILE)
set_config_var outputstandard "$TRANSFER" $PCONFIGFILE
