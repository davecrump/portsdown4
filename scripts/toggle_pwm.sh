#!/bin/bash

# set -x

# Sourced from scheduler.sh to toggle the force_pwm_open between 1 and 0
# setting.  Followed by a reboot to apply the change

# Three scenarios:
#  No text in /boot/config.txt, so append it at 0
#  PWM text is in /boot/config.txt, and is 0, so replace it
#  PWM text is in /boot/config.txt, and is 1, so replace it

# Test for Scenario 1
if ! grep -q 'force_pwm_open' /boot/config.txt; then
  # No relevant text, so append it (Scenario 1)
  sudo sh -c 'echo "\n## Set PWM for audio 1 or RF 0\nforce_pwm_open=0\n" >> /boot/config.txt'
else
  # Text exists, so see if it is set to 0
  if grep -qF 'force_pwm_open=0' /boot/config.txt; then
    #Scenario 2
    sudo sed -i '/force_pwm_open=0/c\force_pwm_open=1' /boot/config.txt >/dev/null 2>/dev/null
  else
    #Scenario 3
    sudo sed -i '/force_pwm_open=1/c\force_pwm_open=0' /boot/config.txt  >/dev/null 2>/dev/null
  fi
fi

# Return control to scheduler for reboot
