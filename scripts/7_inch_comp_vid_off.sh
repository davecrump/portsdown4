#!/bin/bash

# Version 201910101

# set -x

sudo systemctl stop raspi2raspi
sudo systemctl disable raspi2raspi.service
sudo rm /etc/systemd/system/raspi2raspi.service
