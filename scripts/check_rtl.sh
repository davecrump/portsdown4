#!/bin/bash
# Check for the presence of an RTL-SDR
# Echo 0 if present, echo 1 if not detected

lsusb | grep -E -q "RTL|DVB"
if [ $? == 0 ]; then   ## Present
  echo "0"
else
  echo "1"
fi 

exit
