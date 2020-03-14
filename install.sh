#!/bin/bash

# Buster Version by davecrump on 201911230

# Check current user

whoami | grep -q pi
if [ $? != 0 ]; then
  echo "Install must be performed as user pi"
  exit
fi

# Check which source needs to be loaded # From M0DNY 201905090
GIT_SRC="BritishAmateurTelevisionClub"
GIT_SRC_FILE=".portsdown_gitsrc"

if [ "$1" == "-d" ]; then
  GIT_SRC="davecrump";
  echo
  echo "-----------------------------------------------------"
  echo "----- Installing development version for Buster -----"
  echo "-----------------------------------------------------"
elif [ "$1" == "-u" -a ! -z "$2" ]; then
  GIT_SRC="$2"
  echo
  echo "WARNING: Installing ${GIT_SRC} development version, press enter to continue or 'q' to quit."
  read -n1 -r -s key;
  if [[ $key == q ]]; then
    exit 1;
  fi
  echo "ok!";
else
  echo
  echo "-----------------------------------------------------------"
  echo "----- Installing BATC Production Portsdown for Buster -----"
  echo "-----------------------------------------------------------"
fi

# Update the package manager
echo
echo "------------------------------------"
echo "----- Updating Package Manager -----"
echo "------------------------------------"
sudo dpkg --configure -a
sudo apt-get update

# Uninstall the apt-listchanges package to allow silent install of ca certificates (201704030)
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

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
sudo apt-get -y install git
sudo apt-get -y install cmake libusb-1.0-0-dev libx11-dev buffer libjpeg-dev indent 
sudo apt-get -y install ttf-dejavu-core bc usbmount libfftw3-dev wiringpi libvncserver-dev
sudo apt-get -y install fbi netcat imagemagick
sudo apt-get -y install libvdpau-dev libva-dev libxcb-shape0  # For latest ffmpeg build
sudo apt-get -y install python-pip pandoc python-numpy pandoc python-pygame gdebi-core # 20180101 FreqShow
sudo apt-get -y install libsqlite3-dev libi2c-dev # 201811300 Lime
sudo apt-get -y install sshpass  # 201905090 For Jetson Nano
sudo apt-get -y install libbsd-dev # 201910100 for raspi2raspi
sudo apt-get -y install libasound2-dev sox # 201910230 for LongMynd tone and avc2ts audio
sudo apt-get -y install libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev # Required for ffmpegsrc.cpp

sudo pip install pyrtlsdr  #20180101 FreqShow

# Enable USB Storage automount in Buster
echo
echo "----------------------------------"
echo "----- Enabling USB Automount -----"
echo "----------------------------------"
cd /lib/systemd/system/
if ! grep -q PrivateMounts=no systemd-udevd.service; then
  sudo sed -i -e 's/PrivateMounts=yes/PrivateMounts=no/' systemd-udevd.service
fi
cd /home/pi

# Install LimeSuite 20.01 as at 29 Jan 20
# Commit c931854ead81307206bce750c17c2301810b5545
echo
echo "--------------------------------------"
echo "----- Installing LimeSuite 20.01 -----"
echo "--------------------------------------"
wget https://github.com/myriadrf/LimeSuite/archive/c931854ead81307206bce750c17c2301810b5545.zip -O master.zip
unzip -o master.zip
cp -f -r LimeSuite-c931854ead81307206bce750c17c2301810b5545 LimeSuite
rm -rf LimeSuite-c931854ead81307206bce750c17c2301810b5545
rm master.zip

# Compile LimeSuite
cd LimeSuite/
mkdir dirbuild
cd dirbuild/
cmake ../
make
sudo make install
sudo ldconfig
cd /home/pi

# Install udev rules for LimeSuite
cd LimeSuite/udev-rules
chmod +x install.sh
sudo /home/pi/LimeSuite/udev-rules/install.sh
cd /home/pi	

# Record the LimeSuite Version	
echo "c931854" >/home/pi/LimeSuite/commit_tag.txt

# Download the LimeSDR Mini firmware/gateware versions
echo
echo "------------------------------------------------------"
echo "----- Downloading LimeSDR Mini Firmware versions -----"
echo "------------------------------------------------------"

# Previous version
mkdir -p /home/pi/.local/share/LimeSuite/images/19.01/
wget https://downloads.myriadrf.org/project/limesuite/19.01/LimeSDR-Mini_HW_1.2_r1.29.rpd -O \
               /home/pi/.local/share/LimeSuite/images/19.01/LimeSDR-Mini_HW_1.2_r1.29.rpd
# Current Version from 19.04 (used for some touchscreen-driven updates)
mkdir -p /home/pi/.local/share/LimeSuite/images/19.04/
wget https://downloads.myriadrf.org/project/limesuite/19.04/LimeSDR-Mini_HW_1.2_r1.30.rpd -O \
               /home/pi/.local/share/LimeSuite/images/19.04/LimeSDR-Mini_HW_1.2_r1.30.rpd
# Current Version from 20.01 (used for LimeUtil updates)
mkdir -p /home/pi/.local/share/LimeSuite/images/20.01/
wget https://downloads.myriadrf.org/project/limesuite/20.01/LimeSDR-Mini_HW_1.2_r1.30.rpd -O \
               /home/pi/.local/share/LimeSuite/images/20.01/LimeSDR-Mini_HW_1.2_r1.30.rpd
# DVB-S/S2 Version
mkdir -p /home/pi/.local/share/LimeSuite/images/v0.3
wget https://github.com/natsfr/LimeSDR_DVBSGateware/releases/download/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd -O \
 /home/pi/.local/share/LimeSuite/images/v0.3/LimeSDR-Mini_lms7_trx_HW_1.2_auto.rpd

# Download the previously selected version of rpidatv
echo
echo "------------------------------------------"
echo "----- Downloading Portsdown Software -----"
echo "------------------------------------------"
wget https://github.com/${GIT_SRC}/portsdown-buster/archive/master.zip

# Unzip the rpidatv software and copy to the Pi
unzip -o master.zip
mv portsdown-buster-master rpidatv
rm master.zip
cd /home/pi

echo
echo "------------------------------------------------------------"
echo "----- Downloading Portsdown version of avc2ts Software -----"
echo "------------------------------------------------------------"

# Download the previously selected version of avc2ts
wget https://github.com/${GIT_SRC}/avc2ts/archive/master.zip

# Unzip the avc2ts software and copy to the Pi
unzip -o master.zip
mv avc2ts-master avc2ts
rm master.zip

# Compile rpidatv core
echo
echo "-----------------------------"
echo "----- Compiling rpidatv -----"
echo "-----------------------------"
cd /home/pi/rpidatv/src
make
sudo make install

# Compile rpidatv gui
echo
echo "----------------------------------"
echo "----- Compiling rpidatvtouch -----"
echo "----------------------------------"
cd /home/pi/rpidatv/src/gui
make
sudo make install

# Build avc2ts and dependencies
echo
echo "--------------------------------------------"
echo "----- Building avc2ts and dependencies -----"
echo "--------------------------------------------"

# For libmpegts
cd /home/pi/avc2ts
git clone git://github.com/F5OEO/libmpegts
cd libmpegts
./configure
make
cd ../

# For libfdkaac
sudo apt-get -y install autoconf libtool
git clone https://github.com/mstorsjo/fdk-aac
cd fdk-aac
./autogen.sh
./configure
make && sudo make install
sudo ldconfig
cd ../

#libyuv should be used for fast picture transformation : not yet implemented
git clone https://chromium.googlesource.com/libyuv/libyuv
cd libyuv
#should patch linux.mk with -DHAVE_JPEG on CXX and CFLAGS
#seems to be link with libjpeg9-dev
make V=1 -f linux.mk
cd ../

# Make avc2ts
cd /home/pi/avc2ts
make
cp avc2ts ../rpidatv/bin/
cd ..

# Compile adf4351
echo
echo "----------------------------------------"
echo "----- Compiling the ADF4351 driver -----"
echo "----------------------------------------"
cd /home/pi/rpidatv/src/adf4351
make
cp adf4351 ../../bin/

# Get rtl_sdr
echo
echo "-----------------------------------------------"
echo "----- Installing RTL-SDR Drivers and Apps -----"
echo "-----------------------------------------------"
cd /home/pi
wget https://github.com/keenerd/rtl-sdr/archive/master.zip
unzip master.zip
mv rtl-sdr-master rtl-sdr
rm master.zip

# Compile and install rtl-sdr
cd rtl-sdr/ && mkdir build && cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make && sudo make install && sudo ldconfig
sudo bash -c 'echo -e "\n# for RTL-SDR:\nblacklist dvb_usb_rtl28xxu\n" >> /etc/modprobe.d/blacklist.conf'
cd /home/pi

# Get leandvb
cd /home/pi/rpidatv/src
wget https://github.com/pabr/leansdr/archive/master.zip
unzip master.zip
mv leansdr-master leansdr
rm master.zip

# Compile leandvb
cd leansdr/src/apps
make
cp leandvb ../../../../bin/
cd /home/pi

# Get tstools
echo
echo "-----------------------------"
echo "----- Compiling tstools -----"
echo "-----------------------------"
cd /home/pi/rpidatv/src
wget https://github.com/F5OEO/tstools/archive/master.zip
unzip master.zip
mv tstools-master tstools
rm master.zip

# Compile tstools
cd tstools
make
cp bin/ts2es ../../bin/
cd /home/pi/

#install H264 Decoder : hello_video
echo
echo "---------------------------------"
echo "----- Compiling hello_video -----"
echo "---------------------------------"

#compile ilcomponent first
cd /opt/vc/src/hello_pi/
sudo ./rebuild.sh

# Compile H264 player
cd /home/pi/rpidatv/src/hello_video
make
cp hello_video.bin ../../bin/

# Compile MPEG-2 player
cd /home/pi/rpidatv/src/hello_video2
make
cp hello_video2.bin ../../bin/
cd /home/pi/

# FBCP : Duplicate Framebuffer 0 -> 1 for 3.5 inch touchscreen
echo
echo "------------------------------------------"
echo "----- Downloading and Compiling fbcp -----"
echo "------------------------------------------"
wget https://github.com/tasanakorn/rpi-fbcp/archive/master.zip
unzip master.zip
mv rpi-fbcp-master rpi-fbcp
rm master.zip

# Compile fbcp
cd rpi-fbcp/
mkdir build
cd build/
cmake ..
make
sudo install fbcp /usr/local/bin/fbcp
cd /home/pi/

# Install omxplayer
echo
echo "--------------------------------"
echo "----- Installing omxplayer -----"
echo "--------------------------------"
sudo apt-get -y install omxplayer

echo
echo "----------------------------------------------------"
echo "----- Setting up 3.5 inch touchscreen overlays -----"
echo "----------------------------------------------------"
# Install Waveshare 3.5A DTOVERLAY
cd /home/pi/rpidatv/scripts/
sudo cp ./waveshare35a.dtbo /boot/overlays/

# Install Waveshare 3.5B DTOVERLAY
sudo cp ./waveshare35b.dtbo /boot/overlays/

# Install the Waveshare 3.5A driver
sudo bash -c 'cat /home/pi/rpidatv/scripts/configs/waveshare_mkr.txt >> /boot/config.txt'

# Download, compile and install DATV Express-server
echo
echo "------------------------------------------"
echo "----- Installing DATV Express Server -----"
echo "------------------------------------------"
cd /home/pi
wget https://github.com/G4GUO/express_server/archive/master.zip
unzip master.zip
mv express_server-master express_server
rm master.zip
cd /home/pi/express_server
make
sudo make install

cd /home/pi
# Install limesdr_toolbox
echo
echo "--------------------------------------"
echo "----- Installing LimeSDR Toolbox -----"
echo "--------------------------------------"

cd /home/pi/rpidatv/src/limesdr_toolbox

# Install sub project dvb modulation
git clone https://github.com/F5OEO/libdvbmod
cd libdvbmod/libdvbmod
make
cd ../DvbTsToIQ/
make
cp dvb2iq /home/pi/rpidatv/bin/
cd /home/pi/rpidatv/src/limesdr_toolbox/

#Make 
make 
cp limesdr_send /home/pi/rpidatv/bin/
cp limesdr_dump /home/pi/rpidatv/bin/
cp limesdr_stopchannel /home/pi/rpidatv/bin/
cp limesdr_forward /home/pi/rpidatv/bin/
make dvb
cp limesdr_dvb /home/pi/rpidatv/bin/
cd /home/pi

# Download the previously selected version of LongMynd
echo
echo "--------------------------------------------"
echo "----- Installing the LongMynd Receiver -----"
echo "--------------------------------------------"
wget https://github.com/${GIT_SRC}/longmynd/archive/master.zip
unzip -o master.zip
mv longmynd-master longmynd
rm master.zip

cd longmynd
make
cd /home/pi

# Enable camera
echo
echo "--------------------------------------------------"
echo "---- Enabling the Pi Cam in /boot/config.txt -----"
echo "--------------------------------------------------"
cd /home/pi/rpidatv/scripts/
sudo bash -c 'echo -e "\n##Enable Pi Camera" >> /boot/config.txt'
sudo bash -c 'echo -e "\ngpu_mem=128\nstart_x=1\n" >> /boot/config.txt'
cd /home/pi/

# Download, compile and install the executable for hardware shutdown button
echo
echo "------------------------------------------------------------"
echo "----- Installing the hardware shutdown Button software -----"
echo "------------------------------------------------------------"

git clone https://github.com/philcrump/pi-sdn /home/pi/pi-sdn-build

# Install new version that sets swapoff
cp -f /home/pi/rpidatv/src/pi-sdn/main.c /home/pi/pi-sdn-build/main.c
cd /home/pi/pi-sdn-build
make
mv pi-sdn /home/pi/
cd /home/pi

# Edit the crontab to always run boot_startup.sh on boot
echo
echo "-------------------------------------------------"
echo "----- Setting up Start behaviour in crontab -----"
echo "-------------------------------------------------"

sudo crontab /home/pi/rpidatv/scripts/configs/blankcron

# Reduce the dhcp client timeout to speed off-network startup (201704160)
sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
sudo bash -c 'echo -e "\ntimeout 5\n" >> /etc/dhcpcd.conf'

echo
echo "--------------------------"
echo "----- Preparing WiFi -----"
echo "--------------------------"

sudo rm /etc/wpa_supplicant/wpa_supplicant.conf
sudo cp /home/pi/rpidatv/scripts/configs/wpa_supplicant.conf /etc/wpa_supplicant/wpa_supplicant.conf
sudo chown root /etc/wpa_supplicant/wpa_supplicant.conf

echo
echo "-----------------------------------------"
echo "----- Compiling Ancilliary programs -----"
echo "-----------------------------------------"


# Compile and install the executable for switched repeater streaming (201708150)
cd /home/pi/rpidatv/src/rptr
make
mv keyedstream /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission (201710080)
cd /home/pi/rpidatv/src/keyedtx
make
mv keyedtx /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission with touch (202003020)
cd /home/pi/rpidatv/src/keyedtxtouch
make
mv keyedtxtouch /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for the Stream Receiver (201807290)
cd /home/pi/rpidatv/src/streamrx
make
mv streamrx /home/pi/rpidatv/bin/
cd /home/pi

# Compile the Signal Generator (201710280)
cd /home/pi/rpidatv/src/siggen
make clean
make
sudo make install
cd /home/pi

# Compile the Attenuator Driver (201801060)
cd /home/pi/rpidatv/src/atten
make
cp /home/pi/rpidatv/src/atten/set_attenuator /home/pi/rpidatv/bin/set_attenuator
cd /home/pi

echo
echo "------------------------------------------"
echo "----- Setting up for captured images -----"
echo "------------------------------------------"

# Amend /etc/fstab to create a tmpfs drive at ~/tmp for multiple images (201708150)
sudo sed -i '4itmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0' /etc/fstab

# Create a ~/snaps folder for captured images (201708150)
mkdir /home/pi/snaps

# Set the image index number to 0 (201708150)
echo "0" > /home/pi/snaps/snap_index.txt


echo
echo "--------------------------------------"
echo "----- Configure the Video Output -----"
echo "--------------------------------------"

# Enable the Video output in PAL mode (201707120)
cd /boot
sudo sed -i 's/^#sdtv_mode=2/sdtv_mode=2/' config.txt
cd /home/pi

# Download and compile the components for Comp Vid output whilst using 7 inch screen
wget https://github.com/AndrewFromMelbourne/raspi2raspi/archive/master.zip
unzip master.zip
mv raspi2raspi-master raspi2raspi
rm master.zip
cd raspi2raspi/
mkdir build
cd build
cmake ..
make
sudo make install
cd /home/pi

echo
echo "--------------------------------------"
echo "----- Configure the Menu Aliases -----"
echo "--------------------------------------"

# Install the menu aliases
echo "alias menu='/home/pi/rpidatv/scripts/menu.sh menu'" >> /home/pi/.bash_aliases
echo "alias gui='/home/pi/rpidatv/scripts/utils/guir.sh'"  >> /home/pi/.bash_aliases
echo "alias ugui='/home/pi/rpidatv/scripts/utils/uguir.sh'"  >> /home/pi/.bash_aliases

# Load modified .bashrc to run startup script on ssh logon
cp /home/pi/rpidatv/scripts/configs/startup.bashrc /home/pi/.bashrc

# Record Version Number
cd /home/pi/rpidatv/scripts/
cp latest_version.txt installed_version.txt
cd /home/pi

# Switch to French if required
if [ "$1" == "fr" ];
then
  echo "Installation de la langue française et du clavier"
  cd /home/pi/rpidatv/scripts/
  sudo cp configs/keyfr /etc/default/keyboard
  sed -i 's/^menulanguage.*/menulanguage=fr/' portsdown_config.txt
  cd /home/pi
  echo "Installation française terminée"
else
  echo "Completed English Install"
fi

# Save git source used
echo "${GIT_SRC}" > /home/pi/${GIT_SRC_FILE}

# Reboot
echo
echo "--------------------------------"
echo "----- Complete.  Rebooting -----"
echo "--------------------------------"
sleep 1
echo
echo "---------------------------------------------------------------"
echo "----- If fitted, the touchscreen will be active on reboot -----"
echo "---------------------------------------------------------------"

sudo reboot now
exit


