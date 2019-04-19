#!/bin/bash

# Amended 201807090 DGC

# Set the look-up files
PATHBIN="/home/pi/rpidatv/bin/"
PATHSCRIPT="/home/pi/rpidatv/scripts"
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"
RXPRESETSFILE="/home/pi/rpidatv/scripts/rx_presets.txt"
RTLPRESETSFILE="/home/pi/rpidatv/scripts/rtl-fm_presets.txt"

# Define proc to look-up with
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

# Look up and calculate the Receive parameters

SYMBOLRATEK=$(get_config_var rx0sr $RXPRESETSFILE)
let SYMBOLRATE=SYMBOLRATEK*1000

FREQ_OUTPUT=$(get_config_var rx0frequency $RXPRESETSFILE)
FreqHz=$(echo "($FREQ_OUTPUT*1000000)/1" | bc )
#echo Freq = $FreqHz

FEC=$(get_config_var rx0fec $RXPRESETSFILE)
# Will need additional lines here to handle DVB-S2 FECs
let FECNUM=FEC
let FECDEN=FEC+1

SAMPLERATEK=$(get_config_var rx0samplerate $RXPRESETSFILE)
if [ "$SAMPLERATEK" = "0" ]; then
  if [ "$SYMBOLRATEK" -lt 251 ]; then
    SR_RTLSDR=300000
  elif [ "$SYMBOLRATEK" -gt 250 ] && [ "$SYMBOLRATEK" -lt 1001 ]; then
    SR_RTLSDR=1200000
  else
    SR_RTLSDR=2400000
  fi
else
  let SR_RTLSDR=SAMPLERATEK*1000
fi

GAIN=$(get_config_var rx0gain $RXPRESETSFILE)

MODULATION=$(get_config_var rx0modulation $RXPRESETSFILE)

ENCODING=$(get_config_var rx0encoding $RXPRESETSFILE)

SDR=$(get_config_var rx0sdr $RXPRESETSFILE)

GRAPHICS=$(get_config_var rx0graphics $RXPRESETSFILE)

PARAMS=$(get_config_var rx0parameters $RXPRESETSFILE)

SOUND=$(get_config_var rx0sound $RXPRESETSFILE)

FLOCK=$(get_config_var rx0fastlock $RXPRESETSFILE)
if [ "$FLOCK" = "ON" ]; then
  FASTLOCK="--fastlock"
else
  FASTLOCK=" "
fi

# Look up the RTL-SDR Frequency error from the RTL-FM file
FREQOFFSET=$(get_config_var roffset $RTLPRESETSFILE)

# Clean up
sudo rm fifo.264 >/dev/null 2>/dev/null
sudo rm videots >/dev/null 2>/dev/null
#sudo rm fifo.iq >/dev/null 2>/dev/null
sudo killall -9 hello_video.bin >/dev/null 2>/dev/null
sudo killall -9 hello_video2.bin >/dev/null 2>/dev/null
sudo killall leandvb >/dev/null 2>/dev/null
sudo killall ts2es >/dev/null 2>/dev/null
mkfifo fifo.iq
mkfifo fifo.264
mkfifo videots

# Make sure that the screen background is all black
sudo killall fbi >/dev/null 2>/dev/null
sudo fbi -T 1 -noverbose -a $PATHSCRIPT"/images/Blank_Black.png"
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

# Pipe the output from rtl-sdr to leandvb.  Then put videots in a fifo.

# Treat each display case differently

# Constellation and Parameters on
if [ "$GRAPHICS" = "ON" ] && [ "$PARAMS" = "ON" ]; then
  sudo rtl_sdr -p $FREQOFFSET -g $GAIN -f $FreqHz -s $SR_RTLSDR - 2>/dev/null \
    | $PATHBIN"leandvb" --fd-pp 3 --fd-info 2 --fd-const 2 --cr $FECNUM"/"$FECDEN $FASTLOCK --sr $SYMBOLRATE -f $SR_RTLSDR >videots 3>fifo.iq &
fi

# Constellation on, Parameters off
if [ "$GRAPHICS" = "ON" ] && [ "$PARAMS" = "OFF" ]; then
  sudo rtl_sdr -p $FREQOFFSET -g $GAIN -f $FreqHz -s $SR_RTLSDR - 2>/dev/null \
    | $PATHBIN"leandvb" --fd-pp 3 --fd-const 2 --cr $FECNUM"/"$FECDEN $FASTLOCK --sr $SYMBOLRATE -f $SR_RTLSDR >videots 3>fifo.iq &
fi

# Constellation off, Parameters on
if [ "$GRAPHICS" = "OFF" ] && [ "$PARAMS" = "ON" ]; then
  sudo rtl_sdr -p $FREQOFFSET -g $GAIN -f $FreqHz -s $SR_RTLSDR - 2>/dev/null \
    | $PATHBIN"leandvb" --fd-pp 3 --fd-info 2 --fd-const 2 --cr $FECNUM"/"$FECDEN $FASTLOCK --sr $SYMBOLRATE -f $SR_RTLSDR >videots 3>fifo.iq &
fi

# Constellation and Parameters off
if [ "$GRAPHICS" = "OFF" ] && [ "$PARAMS" = "OFF" ]; then
  sudo rtl_sdr -p $FREQOFFSET -g $GAIN -f $FreqHz -s $SR_RTLSDR - 2>/dev/null \
    | $PATHBIN"leandvb" --cr $FECNUM"/"$FECDEN $FASTLOCK --sr $SYMBOLRATE -f $SR_RTLSDR >videots 3>/dev/null &
fi

# read videots and output video es
$PATHBIN"ts2es" -video videots fifo.264 &

# Play the es from fifo.264 in either the H264 or MPEG-2 player.
if [ "$ENCODING" = "H264" ]; then
  $PATHBIN"hello_video.bin" fifo.264 &
else  # MPEG-2
  $PATHBIN"hello_video2.bin" fifo.264 &
fi


# Notes:
# --fd-pp FDNUM        Dump preprocessed IQ data to file descriptor
# --fd-info FDNUM      Output demodulator status to file descriptor
# --fd-const FDNUM     Output constellation and symbols to file descr
# --fd-spectrum FDNUM  Output spectrum to file descr





