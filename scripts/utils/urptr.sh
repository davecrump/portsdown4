#! /bin/bash

# Compile the rptr application, stop it and restart it

#set -x

cd /home/pi/rpidatv/src/rptr
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

cd /home/pi

# Stop the process watchdog
sudo killall streamer_process_watchdog.sh

# Stop the old version
sudo killall keyedstream

# Stop any streaming that is happening
/home/pi/rpidatv/scripts/b.sh

echo
echo "Good install, restarting streaming"

# Overwrite the old version
cp /home/pi/rpidatv/src/rptr/keyedstream /home/pi/rpidatv/bin/keyedstream

# Start the new version
/home/pi/rpidatv/bin/keyedstream 0 &

# Restart the process watchdog
(/home/pi/rpidatv/scripts/streamer_process_watchdog.sh >/dev/null 2>/dev/null) &

exit


