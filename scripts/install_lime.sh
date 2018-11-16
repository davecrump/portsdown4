#! /bin/bash

# Updated by davecrump 201811170

DisplayUpdateMsg() {
  # Delete any old update message image
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 720x576 xc:white \
    -gravity North -pointsize 40 -annotate 0 "\nInstalling LimeSDR Software" \
    -gravity Center -pointsize 50 -annotate 0 "$1""\n\n" \
    -gravity South -pointsize 50 -annotate 0 "DO NOT TURN POWER OFF" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

reset

DisplayUpdateMsg "1 Upgrading existing packages"

# Make sure that we are up-to-date
# Prepare to update the distribution (added 20170405)
sudo dpkg --configure -a     # Make sure that all the packages are properly configured
sudo apt-get clean           # Clean up the old archived packages
sudo apt-get update          # Update the package list

# --- Upgrade firmware and packages ----
sudo apt-get -y dist-upgrade # Upgrade all the installed packages to their latest version

DisplayUpdateMsg "2 Installing new packages"

# Install new packages required
sudo apt-get -y install libsqlite3-dev libi2c-dev

DisplayUpdateMsg "3 Deleting old LimeSuite Files"

sudo rm -rf /usr/local/lib/libLimeSuite.* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/lib/cmake/LimeSuite/* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/include/lime/* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/lib/pkgconfig/LimeSuite.pc >/dev/null 2>/dev/null
sudo rm -rf /usr/local/bin/LimeUtil >/dev/null 2>/dev/null
sudo rm -rf /usr/local/bin/LimeQuickTest >/dev/null 2>/dev/null
sudo rm -rf /home/pi/LimeSuite >/dev/null 2>/dev/null

DisplayUpdateMsg "4 Downloading LimeSuite"

# Install lines if latest version works (it doesn't)

# cd /home/pi
# wget https://github.com/myriadrf/LimeSuite/archive/master.zip -O master.zip
# unzip -o master.zip
# cp -f -r LimeSuite-master LimeSuite
# rm -rf LimeSuite-master

# Install lines for last working version (can be deleted when fixed)
cd /home/pi

# Try: b0efdb4f2cd55c8b2f61bd39f90f61275698d488
wget https://github.com/myriadrf/LimeSuite/archive/17c3e0573888519bb42c44eb9785cac59f3d68d0.zip -O master.zip
#wget https://github.com/myriadrf/LimeSuite/archive/b0efdb4f2cd55c8b2f61bd39f90f61275698d488.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-17c3e0573888519bb42c44eb9785cac59f3d68d0 LimeSuite
rm -rf LimeSuite-17c3e0573888519bb42c44eb9785cac59f3d68d0
#cp -f -r LimeSuite-b0efdb4f2cd55c8b2f61bd39f90f61275698d488 LimeSuite
#rm -rf LimeSuite-b0efdb4f2cd55c8b2f61bd39f90f61275698d488
# End last working version download

rm master.zip
cd LimeSuite/
mkdir dirbuild
cd dirbuild/

DisplayUpdateMsg "5 Compiling LimeSuite"

cmake ../
make
sudo make install
sudo ldconfig

# Install udev rules
cd /home/pi
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/pi/LimeSuite/udev-rules/install.sh

# Delete old limetools

rm -r -f /home/pi/limetool

# Install New LimeTools
DisplayUpdateMsg "7 Compiling LimeTools"
cd /home/pi/rpidatv/src/limetool
touch limetx.c  # Required so that it recompiles if external libraries have changed
make
cp -f limetx /home/pi/rpidatv/bin/limetx
cd /home/pi

# Install libdvbmod
DisplayUpdateMsg "8 Downloading DVB Apps"
sudo rm -rf /home/pi/libdvbmod >/dev/null 2>/dev/null
cd /home/pi
git clone https://github.com/F5OEO/libdvbmod
cd libdvbmod/libdvbmod
make dirmake
mkdir obj/DVB-S
mkdir obj/DVB-S2
mkdir obj/DVB-T
DisplayUpdateMsg "9 Compiling DVB Encoder"
make
cd ../DvbTsToIQ
DisplayUpdateMsg "10 Compiling DVB TS to IQ"
make
cd /home/pi

DisplayUpdateMsg "11 Lime Install Complete"
sleep 3
DisplayUpdateMsg "Finished! Touch to Continue"

echo
echo "Lime Install Complete.  No reboot required."
echo

exit