#!/bin/bash

# Called by a.sh to delay the PTT 15 seconds to allow lime calibration off-air before transmitting

############### PIN AND TIMING DEFINITIONS ###########

# DELAY_TIME in seconds
DELAY_TIME=15

# PTT_BIT: 0 for Receive, 1 for (delayed) transmit = BCM 21 / Header pin 40
PTT_BIT=21

# SR_BIT0 - SR_BIT2.  Set all high to indicate Lime in use, not F-M board
SR_BIT0=16
SR_BIT1=26
SR_BIT2=20

# Set PTT BIT and SR Bits as outputs
gpio -g mode $PTT_BIT out
gpio -g mode $SR_BIT0 out
gpio -g mode $SR_BIT1 out
gpio -g mode $SR_BIT2 out


############### MAIN PROGRAM ###########

sleep "$DELAY_TIME"

  # set PTT high
  gpio -g write $PTT_BIT 1

  # set SR bits high
  gpio -g write $SR_BIT0 1
  gpio -g write $SR_BIT1 1
  gpio -g write $SR_BIT2 1

exit