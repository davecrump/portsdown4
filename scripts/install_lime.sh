#! /bin/bash

# Updated by davecrump 20180717

DisplayUpdateMsg() {
  # Delete any old update message image
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 720x576 xc:white \
    -gravity North -pointsize 40 -annotate 0 "\nInstalling LimeSDR Software" \
    -gravity Center -pointsize 50 -annotate 0 "$1""\n\nPlease wait" \
    -gravity South -pointsize 50 -annotate 0 "DO NOT TURN POWER OFF" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

reset

# exit #Uncomment this line for release without Lime

DisplayUpdateMsg "Checking update status"

# Make sure that we are up-to-date
# Prepare to update the distribution (added 20170405)
sudo dpkg --configure -a
sudo apt-get clean
sudo apt-get update

# Update the distribution (added 20170403)
sudo apt-get -y dist-upgrade
sudo apt-get update

DisplayUpdateMsg "Installing new packages"

# Install new packages required
sudo apt-get -y install libsqlite3-dev libi2c-dev

DisplayUpdateMsg "Deleting any old LimeSuite Files"

sudo rm -rf /usr/local/lib/libLimeSuite.* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/lib/cmake/LimeSuite/* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/include/lime/* >/dev/null 2>/dev/null
sudo rm -rf /usr/local/lib/pkgconfig/LimeSuite.pc >/dev/null 2>/dev/null
sudo rm -rf /usr/local/bin/LimeUtil >/dev/null 2>/dev/null
sudo rm -rf /usr/local/bin/LimeQuickTest >/dev/null 2>/dev/null
sudo rm -rf /home/pi/LimeSuite >/dev/null 2>/dev/null

DisplayUpdateMsg "Downloading LimeSuite"

# Install lines if latest version works (it doesn't)

# cd /home/pi
# wget https://github.com/myriadrf/LimeSuite/archive/master.zip -O master.zip
# unzip -o master.zip
# cp -f -r LimeSuite-master LimeSuite
# rm -rf LimeSuite-master

# Install lines for last working version (can be deleted when fixed)
cd /home/pi
wget https://github.com/myriadrf/LimeSuite/archive/17c3e0573888519bb42c44eb9785cac59f3d68d0.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-17c3e0573888519bb42c44eb9785cac59f3d68d0 LimeSuite
rm -rf LimeSuite-17c3e0573888519bb42c44eb9785cac59f3d68d0
# End last working version download

rm master.zip
cd LimeSuite/
mkdir dirbuild
cd dirbuild/
DisplayUpdateMsg "Compiling LimeSuite"
cmake ../
make
sudo make install
sudo ldconfig

# Install udev rules
cd /home/pi
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/pi/LimeSuite/udev-rules/install.sh

# Install LimeTools
DisplayUpdateMsg "Downloading Lime Tools"
cd /home/pi
git clone https://github.com/davecrump/limetool
cd limetool
DisplayUpdateMsg "Compiling Lime Tools"
make

# Install libdvbmod
DisplayUpdateMsg "Downloading DVB Modulator"
cd /home/pi
git clone https://github.com/F5OEO/libdvbmod
cd libdvbmod/libdvbmod
make dirmake
mkdir obj/DVB-S
mkdir obj/DVB-S2
mkdir obj/DVB-T
DisplayUpdateMsg "Compiling DVB Modulator Stage 1"
make
cd ../DvbTsToIQ
DisplayUpdateMsg "Compiling DVB Modulator Stage 2"
make
cd /home/pi

DisplayUpdateMsg "Lime Install Complete"
sleep 3
DisplayUpdateMsg "Touch to Continue"

exit