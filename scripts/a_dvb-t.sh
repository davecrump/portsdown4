# This script is called from a.sh when DVB-T modulation is required
# Control is returned to a.sh after the calculations

# First, look up the guard interval and QAM
GUARD=$(get_config_var guard $PCONFIGFILE)
QAM=$(get_config_var qam $PCONFIGFILE)
PLUTOIP=$(get_config_var plutoip $PCONFIGFILE)

# Calculate the max bitrate
# bitrate = 423/544 * bandwidth * FEC * (bits per symbol) * (Guard Factor)

case "$QAM" in
  "qpsk" )
    BITSPERSYMBOL="2"
    CONSTLN="qpsk"
  ;;
  "16qam" )
    BITSPERSYMBOL="4"
    CONSTLN="16qam"
  ;;
  "64qam" )
    BITSPERSYMBOL="6"
    CONSTLN="64qam"
  ;;
  * )
    BITSPERSYMBOL="2"
    CONSTLN="qpsk"
  ;;
esac

let GUARDDEN=$GUARD+1

# Top line first
let BITRATE_TS=423*$SYMBOLRATE
let BITRATE_TS=$BITRATE_TS*$FECNUM
let BITRATE_TS=$BITRATE_TS*$BITSPERSYMBOL
let BITRATE_TS=$BITRATE_TS*$GUARD

# Now divide (smallest first)
let BITRATE_TS=$BITRATE_TS/$FECDEN

let BITRATE_TS=$BITRATE_TS/$GUARDDEN
let BITRATE_TS=$BITRATE_TS/544

echo "Full DVB-T TS bitrate calculated as "$BITRATE_TS

# Now apply margin % for non-CBR encoding (Vary for modulator and encoder)

MARGIN=100           # 

let BITRATE_TS=$BITRATE_TS*$MARGIN
let BITRATE_TS=$BITRATE_TS/100