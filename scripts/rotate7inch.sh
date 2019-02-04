#!/bin/bash

# set -x

# Sourced from scheduler.sh to rotate 7 inch screen display and touch
# by 180 degrees.  Followed by a reboot to apply the change

# Three scenarios:
#  No text in /boot/config.txt, so append it
#  Rotate text is in /boot/config.txt, so comment it out
#  Commented text in /boot/config.txt, so uncomment it

# Test for Scenario 1
if ! grep -q 'lcd_rotate=2' /boot/config.txt; then
  # No relevant text, so append it (Scenario 1)
  sudo sh -c 'echo "\n## Rotate 7 inch Display\nlcd_rotate=2\n" >> /boot/config.txt'
else
  # Text exists, so see if it is commented or not
  if ! grep -qF '#lcd_rotate=2' /boot/config.txt; then
    #Scenario 2
    sudo sed -i '/lcd_rotate=2/c\#lcd_rotate=2' /boot/config.txt >/dev/null 2>/dev/null
  else
    #Scenario 3
    sudo sed -i '/#lcd_rotate=2/c\lcd_rotate=2' /boot/config.txt  >/dev/null 2>/dev/null
  fi
fi

# Return control to scheduler for reboot
