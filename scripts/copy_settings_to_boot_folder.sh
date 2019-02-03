#!/bin/bash

# Version 20190207

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts

# set -x

# Remove any old settings

sudo rm -rf /boot/portsdown_settings
sudo mkdir /boot/portsdown_settings

# portsdown_config.txt
sudo cp -f $PATHSCRIPT"/portsdown_config.txt" /boot/portsdown_settings/portsdown_config.txt

# portsdown_presets.txt
sudo cp -f $PATHSCRIPT"/portsdown_presets.txt" /boot/portsdown_settings/portsdown_presets.txt

# siggencal.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /boot/portsdown_settings/siggencal.txt

# siggenconfig.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /boot/portsdown_settings/siggenconfig.txt

# touchcal.txt
sudo cp -f $PATHSCRIPT"/touchcal.txt" /boot/portsdown_settings/touchcal.txt.

# rtl-fm_presets.txt
sudo cp -f $PATHSCRIPT"/rtl-fm_presets.txt" /boot/portsdown_settings/rtl-fm_presets.txt

# portsdown_locators.txt
sudo cp -f $PATHSCRIPT"/portsdown_locators.txt" /boot/portsdown_settings/portsdown_locators.txt

# rx_presets.txt
sudo cp -f $PATHSCRIPT"/rx_presets.txt" /boot/portsdown_settings/rx_presets.txt

# Stream Presets
sudo cp -f $PATHSCRIPT"/stream_presets.txt" /boot/portsdown_settings/stream_presets.txt

# 9 files
