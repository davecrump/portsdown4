#!/bin/bash

# "$1" is SSID
# "$2" is passphrase

# Script used to set up WiFi from rpidatv gui

## Set up new network


SSID=$1
PW=$2

# echo SSID $SSID
# echo PW $PW

PSK_TEXT=$(wpa_passphrase "$SSID" "$PW" | grep 'psk=' | grep -v '#psk')

# echo PSK_TEXT $PSK_TEXT

PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files

## Build text for supplicant file
## Include Country (required for Stretch)

rm $PATHCONFIGS"/wpa_text.txt" >/dev/null 2>/dev/null

echo -e "network={" >> $PATHCONFIGS"/wpa_text.txt"
echo -e "    ssid="\"""$SSID"\"" >> $PATHCONFIGS"/wpa_text.txt"
echo -e "   "$PSK_TEXT >> $PATHCONFIGS"/wpa_text.txt"
echo -e "}" >>  $PATHCONFIGS"/wpa_text.txt"

## Copy the existing wpa_supplicant file to work on

sudo cp /etc/wpa_supplicant/wpa_supplicant.conf $PATHCONFIGS"/wpa_supcopy.txt"
sudo chown pi:pi $PATHCONFIGS"/wpa_supcopy.txt"

## Define the parameters for the replace script

lead='^##STARTNW'                         ## Marker for start of inserted text
tail='^##ENDNW'                           ## Marker for end of inserted text
CHANGEFILE=$PATHCONFIGS"/wpa_supcopy.txt" ## File requiring added text
APPENDFILE=$PATHCONFIGS"/wpa_markers.txt" ## File containing both markers
TRANSFILE=$PATHCONFIGS"/transfer.txt"     ## File used for transfer
INSERTFILE=$PATHCONFIGS"/wpa_text.txt"    ## File to be included

grep -q "$lead" "$CHANGEFILE"             ## Is the first marker already present?
if [ $? -ne 0 ]; then
    sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
fi

## Replace whatever is between the markers with the insert text

sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
        }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
rm $TRANSFILE                               ## Delete the transfer file

## Give the file root ownership and copy it back over the original

sudo chown root:root $PATHCONFIGS"/wpa_supcopy.txt"
sudo cp $PATHCONFIGS"/wpa_supcopy.txt" /etc/wpa_supplicant/wpa_supplicant.conf
sudo rm $PATHCONFIGS"/wpa_supcopy.txt" >/dev/null 2>/dev/null
rm $PATHCONFIGS"/wpa_text.txt" >/dev/null 2>/dev/null

##bring wifi down and up again, then reset

sudo ip link set wlan0 down
sudo ip link set wlan0 up
wpa_cli -i wlan0 reconfigure >/dev/null 2>/dev/null

## Make sure that it is not soft-blocked
sleep 1
sudo rfkill unblock 0 >/dev/null 2>/dev/null

exit
