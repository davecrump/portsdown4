#!/bin/bash

# Updated for Stretch by davecrump on 20180325

# Update the package manager
sudo dpkg --configure -a
sudo apt-get clean
sudo apt-get update

# Uninstall the apt-listchanges package to allow silent install of ca certificates (201704030)
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

# Update the distribution
sudo apt-get -y dist-upgrade
sudo apt-get update

# Install the packages that we need
sudo apt-get -y install apt-transport-https git rpi-update
sudo apt-get -y install cmake libusb-1.0-0-dev g++ libx11-dev buffer libjpeg-dev indent 
sudo apt-get -y install libfreetype6-dev ttf-dejavu-core bc usbmount fftw3-dev wiringpi libvncserver-dev
sudo apt-get -y install fbi netcat imagemagick htop
sudo apt-get -y install libvdpau-dev libva-dev libxcb-shape0  # For latest ffmpeg build
sudo apt-get -y install python-pip pandoc python-numpy pandoc python-pygame gdebi-core # 20180101 FreqShow

sudo pip install pyrtlsdr  #20180101 FreqShow

cd /home/pi

# Check which source to download.  Default is production
# option d is development from davecrump
# option s is staging from batc/staging
if [ "$1" == "-d" ]; then
  echo "Installing development load"
  wget https://github.com/davecrump/portsdown/archive/master.zip
elif [ "$1" == "-s" ]; then
  echo "Installing BATC Staging load"
  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/batc_staging.zip -O master.zip
else
  echo "Installing BATC Production load"
  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/master.zip
fi

# Unzip the source software and copy to the Pi
unzip -o master.zip
if [ "$1" == "-s" ]; then
  mv portsdown-batc_staging rpidatv
else
  mv portsdown-master rpidatv
fi
rm master.zip

# Compile rpidatv core
cd rpidatv/src
make
sudo make install

# Compile rpidatv gui
cd gui
make
sudo make install
cd ../

# Get libmpegts and compile
cd avc2ts
wget https://github.com/kierank/libmpegts/archive/master.zip
unzip master.zip
mv libmpegts-master libmpegts
rm master.zip
cd libmpegts
./configure
make

# Compile avc2ts
cd ../
make
sudo make install

# Compile adf4351
cd /home/pi/rpidatv/src/adf4351
touch adf4351.c
make
cp adf4351 ../../bin/

# Get rtl_sdr
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
cd ../../

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

# Get tstools
cd /home/pi/rpidatv/src
wget https://github.com/F5OEO/tstools/archive/master.zip
unzip master.zip
mv tstools-master tstools
rm master.zip

# Compile tstools
cd tstools
make
cp bin/ts2es ../../bin/

#install H264 Decoder : hello_video
#compile ilcomponet first
cd /opt/vc/src/hello_pi/
sudo ./rebuild.sh

cd /home/pi/rpidatv/src/hello_video
make
cp hello_video.bin ../../bin/

# TouchScreen GUI
# FBCP : Duplicate Framebuffer 0 -> 1
cd /home/pi/
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
cd ../../

# Install Waveshare 3.5A DTOVERLAY
cd /home/pi/rpidatv/scripts/
sudo cp ./waveshare35a.dtbo /boot/overlays/

# Install Waveshare 3.5B DTOVERLAY
sudo cp ./waveshare35b.dtbo /boot/overlays/

# Install the Waveshare 3.5A driver

sudo bash -c 'cat /home/pi/rpidatv/scripts/configs/waveshare_mkr.txt >> /boot/config.txt'

# Disable the Touchscreen Screensaver
# Probably not required for Stretch

# cd /boot
# sudo sed -i -e 's/rootwait/rootwait consoleblank=0/' cmdline.txt
# cd /etc/kbd
#  sudo sed -i 's/^BLANK_TIME.*/BLANK_TIME=0/' config
#  sudo sed -i 's/^POWERDOWN_TIME.*/POWERDOWN_TIME=0/' config

# Download, compile and install DATV Express-server

cd /home/pi
wget https://github.com/G4GUO/express_server/archive/master.zip
unzip master.zip
mv express_server-master express_server
rm master.zip
cd /home/pi/express_server
make
sudo make install

cd /home/pi/rpidatv/scripts/

# Enable camera
sudo bash -c 'echo -e "\n##Enable Pi Camera" >> /boot/config.txt'
sudo bash -c 'echo -e "\ngpu_mem=128\nstart_x=1\n" >> /boot/config.txt'

# Disable sync option for usbmount
sudo sed -i 's/sync,//g' /etc/usbmount/usbmount.conf

# Download, compile and install the executable for hardware shutdown button
# Updated version that is less trigger-happy (201705200)
git clone https://github.com/philcrump/pi-sdn /home/pi/pi-sdn-build
cd /home/pi/pi-sdn-build
make
mv pi-sdn /home/pi/
cd /home/pi

# Create directory for Autologin link
sudo mkdir -p /etc/systemd/system/getty.target.wants

# Add the console menu to the bash history
/home/pi/rpidatv/scripts/configs/add_menu_to_history.sh

# Load new .bashrc to source the startup script at boot and log-on (201704160)
cp /home/pi/rpidatv/scripts/configs/startup.bashrc /home/pi/.bashrc

# Always auto-logon and run .bashrc (and hence startup.sh) (201704160)
sudo ln -fs /etc/systemd/system/autologin@.service\
 /etc/systemd/system/getty.target.wants/getty@tty1.service

# Reduce the dhcp client timeout to speed off-network startup (201704160)
sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
sudo bash -c 'echo -e "\ntimeout 5\n" >> /etc/dhcpcd.conf'

# Enable the Video output in PAL mode (201707120)
cd /boot
sudo sed -i 's/^#sdtv_mode=2/sdtv_mode=2/' config.txt
cd /home/pi

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

# Amend /etc/fstab to create a tmpfs drive at ~/tmp for multiple images (201708150)
sudo sed -i '4itmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0' /etc/fstab

# Create a ~/snaps folder for captured images (201708150)
mkdir /home/pi/snaps

# Set the image index number to 0 (201708150)
echo "0" > /home/pi/snaps/snap_index.txt

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

# Install FreqShow (see https://learn.adafruit.com/freq-show-raspberry-pi-rtl-sdr-scanner/overview)

# Remove the existing version of libsdl1.2debian
sudo apt-get -y remove libsdl1.2debian
# Then load the old (1.2.15-5) version of sdl.  Later versions do not work
sudo gdebi --non-interactive /home/pi/rpidatv/scripts/configs/freqshow/libsdl1.2debian_1.2.15-5_armhf.deb
# Now reload python-pygame
sudo apt-get -y install python-pygame
# Load touchscreen configuration
sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal
# Download FreqShow
git clone https://github.com/adafruit/FreqShow.git
# Change the settings for our environment
rm /home/pi/FreqShow/freqshow.py
cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_freqshow.py /home/pi/FreqShow/freqshow.py
rm /home/pi/FreqShow/model.py
cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_146_model.py /home/pi/FreqShow/model.py

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
  sed -i 's/^menulanguage.*/menulanguage=fr/' rpidatvconfig.txt
  cd /home/pi
  echo "Installation française terminée"
else
  echo "Completed English Install"
fi

# Reboot
printf "\nA reboot is required before using the software\n\n"
printf "Rebooting\n\n"
printf "If fitted, the touchscreen will be active on reboot\n"
sudo reboot now
exit
