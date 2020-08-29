#!/bin/bash

cd /home/nano/dvbsdr/scripts

gst-launch-1.0  -q videotestsrc pattern=smpte ! "video/x-raw,width=1280, height=720, format=(string)YUY2" !  \
  nvvidconv flip-method=0  ! "video/x-raw(memory:NVMM), format=(string)I420"  ! \
  nvvidconv ! "video/x-raw(memory:NVMM), width=(int)720, height=(int)416, format=(string)I420" ! \
  omxh264enc   vbv-size=15 control-rate=2 bitrate=240186 peak-bitrate=264204 \
  insert-sps-pps=1 insert-vui=1 cabac-entropy-coding=1 preset-level=3 profile=8 iframeinterval=100 ! \
  "video/x-h264, level=(string)4.1, stream-format=(string)byte-stream" ! queue ! \
  mux. audiotestsrc ! 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' ! \
  voaacenc bitrate=20000 ! queue  ! mux. mpegtsmux alignment=7 name=mux ! queue ! fdsink \
  | ffmpeg -i - -ss 8 \
  -c:v copy -max_delay 200000 -muxrate 440310 \
  -c:a copy \
  -f mpegts -metadata service_provider="Portsdown" -metadata service_name="TEST" \
  -mpegts_pmt_start_pid 4095 -streamid 0:"256" -streamid 1:"257" - \
  | ../bin/limesdr_dvb -s "333"000 -f 2/3 -r 2 -m DVBS2 -c QPSK -t "437"e6 -g 0.88 -q 1