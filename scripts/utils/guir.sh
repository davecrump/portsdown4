#! /bin/bash

# Kill and Restart gui

sudo killall rpidatvgui >/dev/null 2>/dev/null
sudo killall longmynd >/dev/null 2>/dev/null
sudo killall vlc >/dev/null 2>/dev/null

cd /home/pi
reset

/home/pi/rpidatv/scripts/scheduler_debug.sh
