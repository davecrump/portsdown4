#! /bin/bash

# Compile sdrplayview and copy to rpidatv/bin

# set -x

########### Function to display message ####################

DisplayMsg() {
  # Delete any old update message image
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 800x480 xc:black -font FreeSans -fill white \
    -gravity Center -pointsize 30 -annotate 0 "$1" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}


sudo killall rpidatvgui >/dev/null 2>/dev/null

# Kill the current process if it is running
sudo killall sdrplayview >/dev/null 2>/dev/null

cd /home/pi/rpidatv/src/sdrplayview
touch sdrplayview.c
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi

sudo killall rpidatvgui >/dev/null 2>/dev/null

cp sdrplayview /home/pi/rpidatv/bin/sdrplayview
cd /home/pi

      DisplayMsg "Restarting SDRPlay Service\n\nThis may take up to 90 seconds"
      sudo systemctl restart sdrplay
      DisplayMsg " "                      # Display Blank screen

      lsusb | grep -q '1df7:'             # check for SDRPlay
      if [ $? != 0 ]; then                # Not detected
        DisplayMsg "Unable to detect SDRPlay\n\nResetting the USB Bus"
        sudo uhubctl -R -a 2              # So reset USB bus
        sleep 1
        lsusb | grep -q '1df7:'
        if [ $? != 0 ]; then              # Check again
          sudo uhubctl -R -a 2            # Try reset USB bus again
          sleep 1
          lsusb | grep -q '1df7:'         
          if [ $? != 0 ]; then            # If still no joy
            DisplayMsg "Still Unable to detect SDRPlay\n\n\nCheck connections"
            sleep 2
            DisplayMsg " "                # Display Blank screen
            GUI_RETURN_CODE=129           # Return to Portsdown     
          fi
        fi
      fi


/home/pi/rpidatv/bin/sdrplayview
GUI_RETURN_CODE="$?"

while [ "$GUI_RETURN_CODE" -gt 127 ] || [ "$GUI_RETURN_CODE" -eq 0 ];  do
  case "$GUI_RETURN_CODE" in
    0)
      exit
    ;;
    129)
      /home/pi/rpidatv/bin/rpidatvgui
      GUI_RETURN_CODE="$?"
    ;;
    144)
      sleep 1
      RPISTATE="Not_Ready"
      while [[ "$RPISTATE" == "Not_Ready" ]]
      do
        DisplayMsg "Restarting SDRPlay Service\n\nThis may take up to 90 seconds"
        /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &

        sudo systemctl restart sdrplay

        DisplayMsg " "                      # Display Blank screen
        /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &

        lsusb | grep -q '1df7:'             # check for SDRPlay
        if [ $? != 0 ]; then                # Not detected
          DisplayMsg "Unable to detect SDRPlay\n\nResetting the USB Bus"
          /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &
          sudo uhubctl -R -a 2              # So reset USB bus
          sleep 1

          lsusb | grep -q '1df7:'           # Check again
          if [ $? != 0 ]; then              # Not detected
            DisplayMsg "Unable to detect SDRPlay\n\nResetting the USB Bus"
            /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &
            sudo uhubctl -R -a 2            # Try reset USB bus again
            sleep 1

            lsusb | grep -q '1df7:'         # Has that worked?
            if [ $? != 0 ]; then            # No
              DisplayMsg "Still Unable to detect SDRPlay\n\n\nCheck connections"
              /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &
              sleep 2
              DisplayMsg " "                # Display Blank screen
              /home/pi/rpidatv/scripts/single_screen_grab_for_web.sh &
            else
              RPISTATE="Ready"     
            fi
          else
            RPISTATE="Ready"
          fi
        else
          RPISTATE="Ready"
        fi
      done

      /home/pi/rpidatv/bin/sdrplayview
      GUI_RETURN_CODE="$?"

      if [ $GUI_RETURN_CODE != 129 ] && [ $GUI_RETURN_CODE != 160 ]; then     # Not Portsdown and not shutdown
        GUI_RETURN_CODE=144                         # So restart sdrplayview        
      fi
    ;;
  esac
done


exit
