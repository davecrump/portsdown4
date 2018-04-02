#!/bin/bash

# Called by rpidatvtouch.c to reset Logitech Webcams
# DGC 20180329

   echo "Please wait for Webcam driver to be reset"
    sleep 3
    READY=0
    while [ $READY == 0 ]
    do
      v4l2-ctl --list-devices  >/dev/null 2>/dev/null
      if [ $? == 1 ] ; then
        echo
        echo "Still waiting...."
        sleep 3
      else
        READY=1
        echo
        echo "Now reset.  Returning control to touchscreen"
      fi
    done

