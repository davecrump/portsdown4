#!/bin/bash

# Script to momentarily reduce then increase the brightness of the 7 inch display.
# Used to indicate a successful snap

sudo sh -c 'echo "0" > /sys/class/backlight/rpi_backlight/brightness'

sleep 0.1

sudo sh -c 'echo "255" > /sys/class/backlight/rpi_backlight/brightness'

exit

