#!/bin/bash

# Version 202405291

# set -x

############ Set Environment Variables ###############

RETURN_CODE=1
MOUNT_POINT=/mnt

# Check USB drive detected


sudo bash -c ' fdisk -l | grep -q /dev/sdb1'
RETURN_CODE=$?

if [ $RETURN_CODE == 0 ]; then                       # Second drive present
  #echo Drive USB or Pluto found at sdb1
  sudo bash -c ' fdisk -l | grep -q File-Stor'
  RETURN_CODE=$?
  if [ $RETURN_CODE == 0 ]; then                     # Pluto detected
    sudo bash -c ' fdisk -l | grep -q "/dev/sdb: 50 MiB"'
    RETURN_CODE=$?
    if [ $RETURN_CODE == 0 ]; then                   # Pluto on sdb1
      #echo Pluto at sdb1, USB drive at sda1
      echo 1
      exit 1                      # So mount on sda1
    else                                             # Pluto on sda1
      #echo Pluto at sda1, USB drive at sdb1
      echo 2
      exit 2
    fi
  fi
else                                                 # Only Pluto, Single drive or no drive
  sudo bash -c ' fdisk -l | grep -q File-Stor'
  RETURN_CODE=$?
  if [ $RETURN_CODE == 0 ]; then                     # This is a Pluto, not a USB Drive
    #echo USB Drive not found, but Pluto connected
    echo 0
    exit 0                                           # So do not write to it
  else
    sudo bash -c ' fdisk -l | grep -q /dev/sda1'
    RETURN_CODE=$?
    if [ $RETURN_CODE == 0 ]; then                   # USB drive at sda1, so mount it
      #echo USB Drive found at sda1
      echo 1
      exit 1
    else
      #echo USB Drive not found
      echo 0
      exit 0                                          # No USB drive, so exit with 0
    fi
  fi
fi

exit 0
