#!/bin/bash

# Updated by davecrump 201804021

DisplayUpdateMsg() {
  # Delete any old update message image  201802040
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 720x576 xc:white \
    -gravity North -pointsize 40 -annotate 0 "\nUpdating Portsdown Software" \
    -gravity Center -pointsize 50 -annotate 0 "$1""\n\nPlease wait" \
    -gravity South -pointsize 50 -annotate 0 "DO NOT TURN POWER OFF" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

reset

printf "\nCommencing update.\n\n"

DisplayUpdateMsg "Step 3 of 10\nSaving Current Config\n\nXXX-------"

# Note previous version number
cp -f -r /home/pi/rpidatv/scripts/installed_version.txt /home/pi/prev_installed_version.txt

# Make safe copies of portsdown_config and portsdown_presets
cp -f -r /home/pi/rpidatv/scripts/portsdown_config.txt /home/pi/portsdown_config.txt
cp -f -r /home/pi/rpidatv/scripts/portsdown_presets.txt /home/pi/portsdown_presets.txt

# Make a safe copy of siggencal.txt
cp -f -r /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/siggencal.txt

# Make a safe copy of touchcal.txt if required
cp -f -r /home/pi/rpidatv/scripts/touchcal.txt /home/pi/touchcal.txt

# Delete any old update message image  201802040
rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

DisplayUpdateMsg "Step 4 of 10\nUpdating Software Packages\n\nXXXX------"

# Uninstall the apt-listchanges package to allow silent install of ca certificates
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

# Prepare to update the distribution (added 20170405)
sudo dpkg --configure -a
sudo apt-get clean
sudo apt-get update

# Update the distribution (added 20170403)
sudo apt-get -y dist-upgrade
sudo apt-get update

# ---------- Update rpidatv -----------

DisplayUpdateMsg "Step 5 of 10\nDownloading Portsdown SW\n\nXXXXX-----"

cd /home/pi

# Check which source to download.  Default is production
# option -p or null is the production load
# option -d is development from davecrump

if [ "$1" == "-d" ]; then
  echo "Installing development load"
  wget https://github.com/davecrump/portsdown/archive/master.zip -O master.zip
else
  echo "Installing BATC Production load"
  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/master.zip -O master.zip
fi

# Unzip and overwrite where we need to
unzip -o master.zip
cp -f -r portsdown-master/bin rpidatv
cp -f -r portsdown-master/scripts rpidatv
cp -f -r portsdown-master/src rpidatv
rm -f rpidatv/video/*.jpg
cp -f -r portsdown-master/video rpidatv
cp -f -r portsdown-master/version_history.txt rpidatv/version_history.txt
rm master.zip
rm -rf portsdown-master

DisplayUpdateMsg "Step 6 of 10\nCompiling Portsdown SW\n\nXXXXXX----"

# Compile rpidatv core
sudo killall -9 rpidatv
cd rpidatv/src
make clean
make
sudo make install

# Compile rpidatv gui
sudo killall -9 rpidatvgui
cd gui
make clean
make
sudo make install
cd ../

# Compile avc2ts
sudo killall -9 avc2ts
cd avc2ts
make clean
make
sudo make install

#install adf4351
cd /home/pi/rpidatv/src/adf4351
touch adf4351.c
make
cp adf4351 ../../bin/
cd /home/pi

#install H264 Decoder : hello_video
#compile ilcomponet first
cd /opt/vc/src/hello_pi/
sudo ./rebuild.sh
cd /home/pi/rpidatv/src/hello_video
touch video.c
make
cp hello_video.bin ../../bin/

# There is no step 7!

# Disable fallback IP (201701230)

cd /etc
sudo sed -i '/profile static_eth0/d' dhcpcd.conf
sudo sed -i '/static ip_address=192.168.1.60/d' dhcpcd.conf
sudo sed -i '/static routers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/static domain_name_servers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/interface eth0/d' dhcpcd.conf
sudo sed -i '/fallback static_eth0/d' dhcpcd.conf

DisplayUpdateMsg "Step 8 of 10\nRestoring Config\n\nXXXXXXXX--"

# Restore portsdown_config.txt
cp -f -r /home/pi/portsdown_config.txt /home/pi/rpidatv/scripts/portsdown_config.txt
cp -f -r /home/pi/portsdown_presets.txt /home/pi/rpidatv/scripts/portsdown_presets.txt
rm -f /home/pi/portsdown_config.txt
rm -f /home/pi/portsdown_presets.txt

# Load new .bashrc to source the startup script at boot and log-on (201704160)
cp -f /home/pi/rpidatv/scripts/configs/startup.bashrc /home/pi/.bashrc

# Always auto-logon and run .bashrc (and hence startup.sh) (201704160)
sudo ln -fs /etc/systemd/system/autologin@.service\
 /etc/systemd/system/getty.target.wants/getty@tty1.service

# Reduce the dhcp client timeout to speed off-network startup (201704160)
# If required
if ! grep -q timeout /etc/dhcpcd.conf; then
  sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 5 secs" >> /etc/dhcpcd.conf'
  sudo bash -c 'echo -e "timeout 5\n" >> /etc/dhcpcd.conf'
fi

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

# Restore the user's original siggencal.txt if required
if [ -f "/home/pi/siggencal.txt" ]; then
  cp -f -r /home/pi/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt
fi

# Restore the user's original touchcal.txt if required (201711030)
if [ -f "/home/pi/touchcal.txt" ]; then
  cp -f -r /home/pi/touchcal.txt /home/pi/rpidatv/scripts/touchcal.txt
fi

DisplayUpdateMsg "Step 9 of 10\nInstalling FreqShow SW\n\nXXXXXXXXX-"

# Downgrade the sdl version so FreqShow works
sudo dpkg -i /home/pi/rpidatv/scripts/configs/freqshow/libsdl1.2debian_1.2.15-5_armhf.deb

# Delete the old FreqShow version
  rm -fr /home/pi/FreqShow/

# Download FreqShow
git clone https://github.com/adafruit/FreqShow.git
  
# Change the settings for our environment
rm /home/pi/FreqShow/freqshow.py
cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_freqshow.py /home/pi/FreqShow/freqshow.py
rm /home/pi/FreqShow/model.py
cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_146_model.py /home/pi/FreqShow/model.py

# Update the version number
rm -rf /home/pi/rpidatv/scripts/installed_version.txt
cp /home/pi/rpidatv/scripts/latest_version.txt /home/pi/rpidatv/scripts/installed_version.txt
cp -f -r /home/pi/prev_installed_version.txt /home/pi/rpidatv/scripts/prev_installed_version.txt
rm -rf /home/pi/prev_installed_version.txt

# Reboot
DisplayUpdateMsg "Step 10 of 10\nRebooting\n\nXXXXXXXXXX"
printf "\nRebooting\n"
sleep 2
sudo reboot now

exit
