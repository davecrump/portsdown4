#!/usr/bin/env bash

# This script is sourced (run) by startup.sh if the touchscreen interface
# is required.
# It enables the various touchscreen applications to call each other
# by checking their return code
# If any applications exits abnormally (with a 1 or a 0 exit code)
# it currently terminates or (for interactive sessions) goes back to a prompt

# set -x

# Return Codes
#
# <128  Exit leaving system running
# 128  Exit leaving system running
# 129  Exit from any app requesting restart of main rpidatvgui
# 130  Exit from rpidatvgui requesting start of siggen
# 131  Exit from rpidatvgui requesting start of spectrum monitor
# 132  Run Update Script for production load
# 133  Run Update Script for development load
# 134  Run XY Display
# 160  Shutdown from GUI
# 192  Reboot from GUI
# 193  Rotate 7 inch and reboot

GUI_RETURN_CODE=129             # Start rpidatvgui on first call

while [ "$GUI_RETURN_CODE" -gt 127 ] || [ "$GUI_RETURN_CODE" -eq 0 ];  do
  case "$GUI_RETURN_CODE" in
    0)
      /home/pi/rpidatv/bin/rpidatvgui
      GUI_RETURN_CODE="$?"
    ;;
    128)
      # Jump out of the loop
      break
    ;;
    129)
      /home/pi/rpidatv/bin/rpidatvgui
      GUI_RETURN_CODE="$?"
    ;;
    130)
      /home/pi/rpidatv/bin/siggen
      GUI_RETURN_CODE=129
    ;;
    131)
      # cd /home/pi/FreqShow
      # sudo python freqshow.py
      # cd /home/pi
      GUI_RETURN_CODE=129
    ;;
    132)
      cd /home/pi
      /home/pi/update.sh -p
    ;;
    133)
      cd /home/pi
      /home/pi/update.sh -d
    ;;
    134)
      GUI_RETURN_CODE="129"
    ;;
    160)
      sleep 1
      sudo swapoff -a
      sudo shutdown now
      break
    ;;
    192)
      sleep 1
      sudo swapoff -a
      sudo reboot now
      break
    ;;
    193)
      # Rotate 7 inch display
      # Three scenarios:
      #  (1) No text in /boot/config.txt, so append it
      #  (2) Rotate text is in /boot/config.txt, so comment it out
      #  (3) Commented text in /boot/config.txt, so uncomment it

      # Test for Scenario 1
      if ! grep -q 'lcd_rotate=2' /boot/config.txt; then
        # No relevant text, so append it (Scenario 1)
        sudo sh -c 'echo "\n## Rotate 7 inch Display\nlcd_rotate=2\n" >> /boot/config.txt'
      else
        # Text exists, so see if it is commented or not
        TEST_STRING="#lcd_rotate=2"
        if ! grep -q -F $TEST_STRING /boot/config.txt; then
          # Scenario 2
          sudo sed -i '/lcd_rotate=2/c\#lcd_rotate=2' /boot/config.txt >/dev/null 2>/dev/null
        else
          # Scenario 3
          sudo sed -i '/#lcd_rotate=2/c\lcd_rotate=2' /boot/config.txt  >/dev/null 2>/dev/null
        fi
      fi

      # Make sure that scheduler reboots and does not repeat 7 inch rotation
      GUI_RETURN_CODE=192
      sleep 1
      sudo swapoff -a
      sudo reboot now
      break
    ;;
    194)
      source /home/pi/rpidatv/scripts/toggle_pwm.sh
      # Make sure that scheduler reboots and does not repeat the command
      GUI_RETURN_CODE=192
      sleep 1
      sudo swapoff -a
      sudo reboot now
      break
    ;;
    *)
      # Jump out of the loop
      break
    ;;
  esac
done


