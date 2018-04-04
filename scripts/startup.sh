#!/usr/bin/env bash

# set -x

# This script is sourced from .bashrc at boot and ssh session start
# to select the user's selected start-up option.
# Dave Crump 20170721 Revised for Stretch 20180327

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
CONFIGFILE=$PATHSCRIPT"/rpidatvconfig.txt"
PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

############ Function to Read from Config File ###############

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}

######################### Start here #####################

# Determine if this is a user ssh session, or an autoboot
case $(ps -o comm= -p $PPID) in
  sshd|*/sshd)
    SESSION_TYPE="ssh"
  ;;
  login)
    SESSION_TYPE="boot"
  ;;
  *)
    SESSION_TYPE="ssh"
  ;;
esac

# If gui is already running and this is an ssh session
# stop the gui, start the menu and return
ps -cax | grep 'rpidatvgui' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  if [ "$SESSION_TYPE" == "ssh" ]; then
    killall rpidatvgui >/dev/null 2>/dev/null
    killall siggen >/dev/null 2>/dev/null
    /home/pi/rpidatv/scripts/menu.sh menu
  fi
  return
fi

# If SigGen is already running and this is an ssh session
# stop the SigGen, start the menu and return
ps -cax | grep 'siggen' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  if [ "$SESSION_TYPE" == "ssh" ]; then
    killall rpidatvgui >/dev/null 2>/dev/null
    killall siggen >/dev/null 2>/dev/null
    /home/pi/rpidatv/scripts/menu.sh menu
  fi
  return
fi

# If menu is already running, exit to command prompt
ps -cax | grep 'menu.sh' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  return
fi

# So continue assuming that this could be a first-start

# If pi-sdn is not running, check if it is required to run
ps -cax | grep 'pi-sdn' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -ne 0 ]; then
  if [ -f /home/pi/.pi-sdn ]; then
    . /home/pi/.pi-sdn
  fi
fi

# Facility to Disable WiFi
# Calls .wifi_off if present and runs "sudo ip link set wlan0 down"
if [ -f ~/.wifi_off ]; then
    . ~/.wifi_off
fi

# If framebuffer copy is not already running, start it
ps -cax | grep 'fbcp' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -ne 0 ]; then
  fbcp &
fi

# If a boot session, put up the BATC Splash Screen, and then kill the process
sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

# Check if PiCam connected; if it is, sort out the
# camera inputs so that PiCam is /dev/video0 and EasyCap is /dev/video1

vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  sudo modprobe -r bcm2835_v4l2 # Unload the Pi Cam driver
  sudo modprobe -r usbtv        # Unload the EasyCap driver
  sudo modprobe -r em28xx        # Unload the EasyCap driver
  sudo modprobe -r stk1160        # Unload the EasyCap driver
  sudo modprobe -r uvcvideo        # Unload the EasyCap driver
  # replace usbtv with your EasyCap driver name
  sudo modprobe bcm2835_v4l2    # Load the Pi Cam driver
  sudo modprobe usbtv           # Load the EasyCap driver
  sudo modprobe em28xx        # Load the EasyCap driver
  sudo modprobe stk1160        # Load the EasyCap driver
# Comment out reload of uvcvideo as it causes shutdown to hang
#  sudo modprobe uvcvideo        # Load the EasyCap driver
# replace usbtv with your EasyCap driver name
fi

# Map the touchscreen event to /dev/input/touchscreen

sudo rm /dev/input/touchscreen >/dev/null 2>/dev/null
cat /proc/bus/input/devices | grep 'H: Handlers=mouse0 event0' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  sudo ln /dev/input/event0 /dev/input/touchscreen
fi
cat /proc/bus/input/devices | grep 'H: Handlers=mouse0 event1' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  sudo ln /dev/input/event1 /dev/input/touchscreen
fi
cat /proc/bus/input/devices | grep 'H: Handlers=mouse0 event2' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  sudo ln /dev/input/event2 /dev/input/touchscreen
fi

# Read the desired start-up behaviour
MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)

# Select the appropriate action

case "$MODE_STARTUP" in
  Prompt)
    # Go straight to command prompt
    return
  ;;
  Console)
    # Start the menu if this is an ssh session
    if [ "$SESSION_TYPE" == "ssh" ]; then
      /home/pi/rpidatv/scripts/menu.sh menu
    fi
    return
  ;;
  TX_boot)
    # Flow will only have got here if menu not already running
    # So start menu in immediate transmit mode
    /home/pi/rpidatv/scripts/menu.sh
    return
  ;;
  Display_boot)
    # First start DATV Express Server if required
    MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
    if [ "$MODE_OUTPUT" == "DATVEXPRESS" ]; then
      if pgrep -x "express_server" > /dev/null; then
        : # Express already running
      else
        # Not running and needed, so start it
        # Make sure that the Control file is not locked
        sudo rm /tmp/expctrl >/dev/null 2>/dev/null
        # From its own folder otherwise it doesn't read the config file
        cd /home/pi/express_server
        sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
        cd /home/pi
        sleep 5                # Give it time to start
      fi
    fi
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
    return
  ;;
  TestRig_boot)
    # Start the touchscreen interface for the filter-mod test rig
    /home/pi/rpidatv/bin/testrig
    return
  ;;
  Keyed_Stream_boot)
    # Start the Switched stream with the default GPIO Pins
    if [ "$SESSION_TYPE" == "boot" ]; then
      /home/pi/rpidatv/bin/keyedstream 1 7
    fi
    return
  ;;
  Cont_Stream_boot)
    # Start a continuous stream
    if [ "$SESSION_TYPE" == "boot" ]; then
      /home/pi/rpidatv/bin/keyedstream 0
    fi
    return
  ;;
  Keyed_TX_boot)
    # Start the Switched transmitter with the default GPIO Pins
    if [ "$SESSION_TYPE" == "boot" ]; then
      /home/pi/rpidatv/bin/keyedtx 1 7
    fi
    return
  ;;
  SigGen_boot)
    # Start the Sig Gen with the output on
    if [ "$SESSION_TYPE" == "boot" ]; then
      /home/pi/rpidatv/bin/siggen on
    fi
    return
  ;;
  *)
    return
  ;;
esac
