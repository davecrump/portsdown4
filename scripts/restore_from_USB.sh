#!/bin/bash

# Version 202405290

# set -x

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
RETURN_CODE=1
MOUNT_POINT=/mnt

# Check USB drive detected

sudo bash -c ' fdisk -l | grep -q /dev/sdb1'
RETURN_CODE=$?

if [ $RETURN_CODE != 0 ]; then
  echo USB Drive not found
  exit
fi

sudo mount /dev/sdb1 /mnt

if [ ! -d "$MOUNT_POINT"/portsdown_settings/ ]; then
  echo "$DIRECTORY does not exist."
fi

# portsdown_config.txt
cp -f $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_config.txt $PATHSCRIPT"/portsdown_config.txt"

# portsdown_presets.txt
cp -f $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_presets.txt $PATHSCRIPT"/portsdown_presets.txt"

# siggencal.txt
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt

# siggenconfig.txt
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt.bak
cp -f "$MOUNT_POINT"/portsdown_settings/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt

# rtl-fm_presets.txt
cp -f $PATHSCRIPT"/rtl-fm_presets.txt" $PATHSCRIPT"/rtl-fm_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/rtl-fm_presets.txt $PATHSCRIPT"/rtl-fm_presets.txt"

# portsdown_locators.txt
cp -f $PATHSCRIPT"/portsdown_locators.txt" $PATHSCRIPT"/portsdown_locators.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/portsdown_locators.txt $PATHSCRIPT"/portsdown_locators.txt"

# rx_presets.txt
cp -f $PATHSCRIPT"/rx_presets.txt" $PATHSCRIPT"/rx_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/rx_presets.txt $PATHSCRIPT"/rx_presets.txt"

# Stream Presets
cp -f $PATHSCRIPT"/stream_presets.txt" $PATHSCRIPT"/stream_presets.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/stream_presets.txt $PATHSCRIPT"/stream_presets.txt"

# Jetson Config
cp -f $PATHSCRIPT"/jetson_config.txt" $PATHSCRIPT"/jetson_config.txt.bak"
cp -f "$MOUNT_POINT"/portsdown_settings/jetson_config.txt $PATHSCRIPT"/jetson_config.txt"

sudo umount /dev/sdb1

