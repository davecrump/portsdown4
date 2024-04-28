#!/bin/bash

# set -x

# This script is sourced from .bashrc at boot and ssh session start
# to sort out driver issues and
# to select the user's selected start-up option.
# Dave Crump 20200516

############ Set Environment Variables ###############

PATHSCRIPT=/home/pi/rpidatv/scripts
PATHRPI=/home/pi/rpidatv/bin
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

############ Function to Write to Config File ###############

set_config_var() {
lua - "$1" "$2" "$3" <<EOF > "$3.bak"
local key=assert(arg[1])
local value=assert(arg[2])
local fn=assert(arg[3])
local file=assert(io.open(fn))
local made_change=false
for line in file:lines() do
if line:match("^#?%s*"..key.."=.*$") then
line=key.."="..value
made_change=true
end
print(line)
end
if not made_change then
print(key.."="..value)
end
EOF
mv "$3.bak" "$3"
}

######################### Start here #####################

if [ "$SESSION_TYPE" == "cron" ]; then
  SESSION_TYPE="boot"
else
  # Determine if this is a user ssh session, or an autoboot
  case $(ps -o comm= -p $PPID) in
    sshd|*/sshd)
      SESSION_TYPE="ssh"
    ;;
    login|sudo)
      SESSION_TYPE="boot"
    ;;
    *)
      SESSION_TYPE="ssh"
    ;;
  esac
fi

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

# If StreamRX is already running and this is an ssh session
# stop the StreamRX, start the menu and return
ps -cax | grep 'streamrx' >/dev/null 2>/dev/null
RESULT="$?"
if [ "$RESULT" -eq 0 ]; then
  if [ "$SESSION_TYPE" == "ssh" ]; then
    killall streamrx >/dev/null 2>/dev/null
    killall omxplayer.bin >/dev/null 2>/dev/null
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
# or it could be a second ssh session.

# If this is the first reboot after install, sort out the sdrplay api and subsequent makes
if [ -f /home/pi/rpidatv/.post-install_actions ]; then
  /home/pi/rpidatv/scripts/post-install_actions.sh
fi

# Read the desired start-up behaviour
MODE_STARTUP=$(get_config_var startup $PCONFIGFILE)

# First check that the correct display has been selected 
# to load drivers against the one that is fitted

# But only if it is a boot session and display boot selected

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

# If a boot session, put up the BATC Splash Screen, and then kill the process
sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/BATC_Black.png >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

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
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
    #return
  ;;
  Langstone_boot)
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
  ;;
  Bandview_boot)
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
  ;;
  Meteorbeacon_boot)
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
  ;;
  Meteorview_boot)
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
  ;;
  Keyed_Stream_boot)
    # Start the Switched stream with the default GPIO Pins
    if [ "$SESSION_TYPE" == "boot" ]; then
      /home/pi/rpidatv/bin/keyedstream 1 7 &
      (/home/pi/rpidatv/scripts/streamer_process_watchdog.sh >/dev/null 2>/dev/null) &
    fi
    return
  ;;
  Keyed_TX_boot)
    # Start the Switched transmitter with the default GPIO Pins
    if [ "$SESSION_TYPE" == "boot" ]; then
      (sleep 10; /home/pi/rpidatv/bin/keyedtx 1 7) &
    fi
    return
  ;;
  Keyed_TX_Touch_boot)
    # Start the Switched transmitter with the default GPIO Pins and Touchscreen
    if [ "$SESSION_TYPE" == "boot" ]; then
      (sleep 10; /home/pi/rpidatv/bin/keyedtxtouch 1 7) &
    fi
    return
  ;;
  *)
    # Start the Touchscreen Scheduler
    source /home/pi/rpidatv/scripts/scheduler.sh
  ;;
esac


