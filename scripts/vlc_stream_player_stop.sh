#!/bin/bash

# Called by streamrx to stop the VLC stream receiver 

echo shutdown | nc 127.0.0.1 1111 >/dev/null 2>/dev/null

sleep 0.5

if  $(pgrep vlc >/dev/null)  ; then
  sleep 0.5
  sudo killall -9 vlc >/dev/null 2>/dev/null
fi

sudo kill -9 $(ps aux | grep 'vlc_stream_player' | grep -v "grep" | tr -s " "| cut -d " " -f 2) >/dev/null 2>/dev/null

