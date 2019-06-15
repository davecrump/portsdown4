#!/bin/bash
# Check for the presence of an ftdi USB Device
# Echo 0 if present, echo 1 if not detected

lsusb | grep -E -q "FT2232C"
if [ $? == 0 ]; then   ## Present
  echo "0"
else
  echo "1"
fi 

exit