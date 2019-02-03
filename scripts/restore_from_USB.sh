#!/bin/bash

# Version 20190207

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts

# set -x

# portsdown_config.txt
cp -f $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
cp -f /media/usb/portsdown_settings/portsdown_config.txt $PATHSCRIPT"/portsdown_config.txt"

# portsdown_presets.txt
cp -f $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
cp -f /media/usb/portsdown_settings/portsdown_presets.txt $PATHSCRIPT"/portsdown_presets.txt"

# siggencal.txt
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt.bak
cp -f /media/usb/portsdown_settings/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt

# siggenconfig.txt
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt.bak
cp -f /media/usb/portsdown_settings/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt

# touchcal.txt
cp -f $PATHSCRIPT"/touchcal.txt" $PATHSCRIPT"/touchcal.txt.bak"
cp -f /media/usb/portsdown_settings/touchcal.txt $PATHSCRIPT"/touchcal.txt"

# rtl-fm_presets.txt
cp -f $PATHSCRIPT"/rtl-fm_presets.txt" $PATHSCRIPT"/rtl-fm_presets.txt.bak"
cp -f /media/usb/portsdown_settings/rtl-fm_presets.txt $PATHSCRIPT"/rtl-fm_presets.txt"

# portsdown_locators.txt
cp -f $PATHSCRIPT"/portsdown_locators.txt" $PATHSCRIPT"/portsdown_locators.txt.bak"
cp -f /media/usb/portsdown_settings/portsdown_locators.txt $PATHSCRIPT"/portsdown_locators.txt"

# rx_presets.txt
cp -f $PATHSCRIPT"/rx_presets.txt" $PATHSCRIPT"/rx_presets.txt.bak"
cp -f /media/usb/portsdown_settings/rx_presets.txt $PATHSCRIPT"/rx_presets.txt"

# Make a safe copy of the Stream Presets if required
cp -f $PATHSCRIPT"/stream_presets.txt" $PATHSCRIPT"/stream_presets.txt.bak"
cp -f /media/usb/portsdown_settings/stream_presets.txt $PATHSCRIPT"/stream_presets.txt"
