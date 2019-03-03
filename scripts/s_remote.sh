#!/bin/bash

# set -x

# This script is called from a Windows computer using plink.exe
# and stops transmission and Shuts Down the remote

######################### Start here #####################

echo "**************************************"
echo "Shutting Down the Remote Portsdown"
echo "**************************************"

sudo killall a_remote.sh

source /home/pi/rpidatv/scripts/b.sh

sleep 1

sudo swapoff -a
sudo shutdown now

sleep 1


