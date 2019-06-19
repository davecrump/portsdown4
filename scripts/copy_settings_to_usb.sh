#!/bin/bash

# Version 20190606

# set -x

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts

# Remove any old settings

sudo rm -rf /media/usb/portsdown_settings
sudo mkdir /media/usb/portsdown_settings

# portsdown_config.txt
sudo cp -f $PATHSCRIPT"/portsdown_config.txt" /media/usb/portsdown_settings/portsdown_config.txt

# portsdown_presets.txt
sudo cp -f $PATHSCRIPT"/portsdown_presets.txt" /media/usb/portsdown_settings/portsdown_presets.txt

# siggencal.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /media/usb/portsdown_settings/siggencal.txt

# siggenconfig.txt
sudo cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /media/usb/portsdown_settings/siggenconfig.txt

# touchcal.txt
sudo cp -f $PATHSCRIPT"/touchcal.txt" /media/usb/portsdown_settings/touchcal.txt.

# rtl-fm_presets.txt
sudo cp -f $PATHSCRIPT"/rtl-fm_presets.txt" /media/usb/portsdown_settings/rtl-fm_presets.txt

# portsdown_locators.txt
sudo cp -f $PATHSCRIPT"/portsdown_locators.txt" /media/usb/portsdown_settings/portsdown_locators.txt

# rx_presets.txt
sudo cp -f $PATHSCRIPT"/rx_presets.txt" /media/usb/portsdown_settings/rx_presets.txt

# Stream Presets
sudo cp -f $PATHSCRIPT"/stream_presets.txt" /media/usb/portsdown_settings/stream_presets.txt

# Jetson Config
sudo cp -f $PATHSCRIPT"/jetson_config.txt" /media/usb/portsdown_settings/jetson_config.txt

# 10 files
