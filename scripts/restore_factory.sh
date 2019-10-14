#!/bin/bash

# Version 201910103

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts

# portsdown_config.txt
cp -f $PATHSCRIPT"/portsdown_config.txt" $PATHSCRIPT"/portsdown_config.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_config.txt.factory" $PATHSCRIPT"/portsdown_config.txt"

# portsdown_presets.txt
cp -f $PATHSCRIPT"/portsdown_presets.txt" $PATHSCRIPT"/portsdown_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_presets.txt.factory" $PATHSCRIPT"/portsdown_presets.txt"

# siggencal.txt
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt.bak
cp -f /home/pi/rpidatv/src/siggen/siggencal.txt.factory /home/pi/rpidatv/src/siggen/siggencal.txt

# siggenconfig.txt
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt.bak
cp -f /home/pi/rpidatv/src/siggen/siggenconfig.txt.factory /home/pi/rpidatv/src/siggen/siggenconfig.txt

# touchcal.txt
cp -f $PATHSCRIPT"/touchcal.txt" $PATHSCRIPT"/touchcal.txt.bak"
cp -f $PATHSCRIPT"/configs/touchcal.txt.factory" $PATHSCRIPT"/touchcal.txt"

# rtl-fm_presets.txt
cp -f $PATHSCRIPT"/rtl-fm_presets.txt" $PATHSCRIPT"/rtl-fm_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/rtl-fm_presets.txt.factory" $PATHSCRIPT"/rtl-fm_presets.txt"

# portsdown_locators.txt
cp -f $PATHSCRIPT"/portsdown_locators.txt" $PATHSCRIPT"/portsdown_locators.txt.bak"
cp -f $PATHSCRIPT"/configs/portsdown_locators.txt.factory" $PATHSCRIPT"/portsdown_locators.txt"

# rx_presets.txt
cp -f $PATHSCRIPT"/rx_presets.txt" $PATHSCRIPT"/rx_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/rx_presets.txt.factory" $PATHSCRIPT"/rx_presets.txt"

# stream_presets.txt
cp -f $PATHSCRIPT"/stream_presets.txt" $PATHSCRIPT"/stream_presets.txt.bak"
cp -f $PATHSCRIPT"/configs/stream_presets.txt.factory" $PATHSCRIPT"/stream_presets.txt"

# jetson_config.txt
cp -f $PATHSCRIPT"/jetson_config.txt" $PATHSCRIPT"/jetson_config.txt.bak"
cp -f $PATHSCRIPT"/configs/jetson_config.txt.factory" $PATHSCRIPT"/jetson_config.txt"

# longmynd_config.txt
cp -f $PATHSCRIPT"/longmynd_config.txt" $PATHSCRIPT"/longmynd_config.txt.bak"
cp -f $PATHSCRIPT"/configs/longmynd_config.txt.factory" $PATHSCRIPT"/longmynd_config.txt"

