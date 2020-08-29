#!/bin/bash

# Nano dvbsdr installer by davecrump on 20200827
cd /home/nano

# Check current user
whoami | grep -q nano
if [ $? != 0 ]; then
  echo "Install must be performed as user nano"
  exit
fi

# Check platform
grep -E -q "jetson-nano|Jetson" /proc/device-tree/model
if [ $? != 0 ]; then   ## Not being run on a Nano
  echo "This script must be run on the Nano, not on a Raspberry Pi."
  exit
fi

echo
echo "---------------------------------------------------------------------------"
echo "----- Installing dvbsdr applications on Nano for use with Portsdown 4 -----"
echo "---------------------------------------------------------------------------"

# Update the package manager and pass the sudo password so that all subsequent commands run
echo
echo "------------------------------------"
echo "----- Updating Package Manager -----"
echo "------------------------------------"
echo jetson | sudo -S apt-get update

# Upgrade the distribution
echo
echo "-----------------------------------"
echo "----- Performing dist-upgrade -----"
echo "-----------------------------------"
sudo apt-get -y dist-upgrade

# Install the packages that we need
echo
echo "-------------------------------"
echo "----- Installing Packages -----"
echo "-------------------------------"

sudo apt-get -y install git htop nano vlc v4l-utils
sudo apt-get install -y git g++ cmake libsqlite3-dev libi2c-dev libusb-1.0-0-dev netcat
sudo apt-get install -y buffer ffmpeg
sudo apt-get install -y libfftw3-dev

# Install LimeSuite 20.07.02 as at 27 Aug 20
# Install in /home/nano
# Commit 3858ed79b95a040145629f95bcaa75e1ea623c6d
echo
echo "--------------------------------------"
echo "----- Installing LimeSuite 20.07 -----"
echo "--------------------------------------"
wget https://github.com/myriadrf/LimeSuite/archive/3858ed79b95a040145629f95bcaa75e1ea623c6d.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-3858ed79b95a040145629f95bcaa75e1ea623c6d LimeSuite
rm -rf LimeSuite-3858ed79b95a040145629f95bcaa75e1ea623c6d
rm master.zip

# Compile LimeSuite
cd LimeSuite/
mkdir dirbuild
cd dirbuild/
cmake ../
make
sudo make install
sudo ldconfig
cd /home/nano

# Install udev rules for LimeSuite
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/nano/LimeSuite/udev-rules/install.sh
cd /home/nano	

# Record the LimeSuite Version	
echo "3858ed7" >/home/nano/LimeSuite/commit_tag.txt

# Install DVB Encoding Tools
echo
echo "----------------------------------"
echo "----- Installing DVB Encoder -----"
echo "----------------------------------"

mkdir dvbsdr
cd /home/nano/dvbsdr
mkdir build
mkdir bin
mkdir scripts

git clone https://github.com/F5OEO/limesdr_toolbox
cd /home/nano/dvbsdr/limesdr_toolbox

# Install sub project dvb modulation
git clone https://github.com/F5OEO/libdvbmod
cd libdvbmod/libdvbmod
make
cd ../DvbTsToIQ/
make
cp dvb2iq /home/nano/dvbsdr/bin/dvb2iq

# Make the limesdr toolbox apps
cd /home/nano/dvbsdr/limesdr_toolbox
make 
cp limesdr_send /home/nano/dvbsdr/bin/limesdr_send
make dvb
cp limesdr_dvb /home/nano/dvbsdr/bin/limesdr_dvb
cd /home/nano

# Install the test card file
echo
echo "-------------------------------------"
echo "----- Installing Test Card file -----"
echo "-------------------------------------"

cd /home/nano/dvbsdr/scripts
wget https://github.com/BritishAmateurTelevisionClub/portsdown4/raw/master/scripts/images/tcfw.jpg

# Install Test script
wget https://github.com/BritishAmateurTelevisionClub/portsdown4/raw/master/scripts/utils/nano_test.sh
chmod +x nano_test.sh

# Record Version Number
wget https://github.com/BritishAmateurTelevisionClub/portsdown4/raw/master/scripts/latest_version.txt
cd /home/nano

echo
echo "--------------------------------------"
echo "----- Configure the Menu Aliases -----"
echo "--------------------------------------"

# Install the menu aliases
echo "alias test='/home/nano/dvbsdr/scripts/nano_test.sh'" >> /home/nano/.bash_aliases


# Reboot
echo
echo "--------------------------------"
echo "----- Complete.  Rebooting -----"
echo "--------------------------------"
sleep 1

sudo reboot now
exit


