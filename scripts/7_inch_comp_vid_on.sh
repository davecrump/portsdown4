#!/bin/bash

# Version 201910101

# set -x

sudo cp /home/pi/raspi2raspi/raspi2raspi.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable raspi2raspi.service
sudo systemctl start raspi2raspi
