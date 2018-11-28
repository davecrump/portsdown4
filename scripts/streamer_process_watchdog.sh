#!/bin/bash

# Called by startup.sh to watchdog keyedstream

# Waits 60 seconds
# Checks keyedstream is running
# If running goes and waits again

# if not running, waits 60 seconds
# checks again, and if not running reboots.  Else goes back and waits.

CHECK1="Running"
CHECK2="Running"
while [ "$CHECK2" == "Running" ]
do
  while [ "$CHECK1" == "Running" ]
  do
    # wait 60 seconds (for startup and then for loop delay
    sleep 60
    printf "Checking keyedstream for Check1\n"
    pgrep -x 'keyedstream' # 1 not running
    if [ "$?" = "1" ]; then
      CHECK1="NotRunning"
      printf "Check1 Not Running\n"
    else 
      CHECK1="Running"
      printf "Check1 Running\n"
    fi
  done
  # wait 60 seconds (for startup and then for loop delay
  sleep 60
  printf "Checking keyedstream for Check2\n"
  pgrep -x 'keyedstream' # 1 not running
  if [ "$?" = "1" ]; then
    CHECK2="NotRunning"
    printf "Check2 Not Running\n"
  else 
    CHECK2="Running"
    printf "Check2 Running\n"
  fi
done

# keyedstream has not been running for at least 1 minute, so reboot
printf "Rebooting\n"
sleep 1
sudo swapoff -a
sudo reboot now