#!/bin/bash

# Called by cron on initial boot to bring up the touchscreen or TX for normal operation

cd /home/pi
sudo -u pi PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/games:/usr/games
sudo -u pi SESSION_TYPE=cron
sudo -u pi /home/pi/rpidatv/scripts/startup.sh
