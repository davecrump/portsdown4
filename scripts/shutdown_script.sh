#!/bin/bash

# set -x

# Called by pi-sdn to tidy up and shutdown.

do_stop_transmit()
{
  # Kill the key processes as nicely as possible
  sudo killall avc2ts >/dev/null 2>/dev/null
  sudo killall limesdr_send >/dev/null 2>/dev/null
  sudo killall limesdr_dvb >/dev/null 2>/dev/null
  sudo killall sox >/dev/null 2>/dev/null
  sudo killall arecord >/dev/null 2>/dev/null

  # Then pause and make sure that avc2ts has really been stopped (needed at high SRs)
  sleep 0.1
  sudo killall -9 avc2ts >/dev/null 2>/dev/null

  # And make sure limesdr_send has been stopped
  sudo killall -9 limesdr_send >/dev/null 2>/dev/null
  sudo killall -9 limesdr_dvb >/dev/null 2>/dev/null
  sudo killall -9 sox >/dev/null 2>/dev/null

  # Make sure that the PTT is released (required for carrier, Lime and test modes)
  gpio mode $GPIO_PTT out
  gpio write $GPIO_PTT 0

  # Set the SR Filter correctly, because it might have been set all high by Lime
  /home/pi/rpidatv/scripts/ctlSR.sh

  # Reset the LimeSDR
  #/home/pi/rpidatv/bin/limesdr_stopchannel & # Fork process as this sometimes hangs

  # Kill a.sh which sometimes hangs during testing
  sudo killall a.sh >/dev/null 2>/dev/null
}

GPIO_PTT=29  ## WiringPi value, not BCM

do_stop_transmit

sleep 5  #1

# Display the splash screen so the user knows something is happening
sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png  >/dev/null 2>/dev/null

# Make sure that the swap file doesn't cause a hang
sudo swapoff -a

sudo shutdown now

exit
