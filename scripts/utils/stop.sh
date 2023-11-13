#! /bin/bash

# Script to stop all running Portsdown processes (except pi-sdn)
# Called from the stop alias

sudo killall rpidatvgui        >/dev/null 2>/dev/null

sudo killall adf4351           >/dev/null 2>/dev/null
sudo killall airspyview        >/dev/null 2>/dev/null
sudo killall avc2ts            >/dev/null 2>/dev/null
sudo killall bandview          >/dev/null 2>/dev/null
sudo killall CombiTunerExpress >/dev/null 2>/dev/null
sudo killall dmm               >/dev/null 2>/dev/null
sudo killall dvb_t_stack       >/dev/null 2>/dev/null
sudo killall dvb2iq            >/dev/null 2>/dev/null
sudo killall ffmpeg            >/dev/null 2>/dev/null
sudo killall keyedtx           >/dev/null 2>/dev/null
sudo killall keyedtxtouch      >/dev/null 2>/dev/null
sudo killall limesdrdvb        >/dev/null 2>/dev/null
sudo killall meteorview        >/dev/null 2>/dev/null
sudo killall nf_meter          >/dev/null 2>/dev/null
sudo killall noise_meter       >/dev/null 2>/dev/null
sudo killall plutoview         >/dev/null 2>/dev/null
sudo killall power_meter       >/dev/null 2>/dev/null
sudo killall rtlsdrview        >/dev/null 2>/dev/null
sudo killall sdrplayview       >/dev/null 2>/dev/null
sudo killall set_attenuator    >/dev/null 2>/dev/null
sudo killall siggen            >/dev/null 2>/dev/null
sudo killall sweeper           >/dev/null 2>/dev/null
sudo killall wav2lime          >/dev/null 2>/dev/null
sudo killall xy                >/dev/null 2>/dev/null

# More to come here once failure cases identified.
# scheduler or a.sh?


