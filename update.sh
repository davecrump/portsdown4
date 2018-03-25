#!/bin/bash

# Updated by davecrump 201802040
# Modified to overwrite ~/rpidatv/scripts and
# ~/rpidatv/src, then compile
# rpidatv, rpidatvgui avc2ts and adf4351

reset

printf "\nCommencing update.\n\n"

# Note previous version number
cp -f -r /home/pi/rpidatv/scripts/installed_version.txt /home/pi/prev_installed_version.txt


# Make safe copies of portsdown_config and portsdown_presets
cp -f -r /home/pi/rpidatv/scripts/portsdown_config.txt /home/pi/portsdown_config.txt
cp -f -r /home/pi/rpidatv/scripts/portsdown_presets.txt /home/pi/portsdown_presets.txt


# Make a safe copy of siggencal.txt if required
if [ -f "/home/pi/rpidatv/src/siggen/siggencal.txt" ]; then
  cp -f -r /home/pi/rpidatv/src/siggen/siggencal.txt /home/pi/siggencal.txt
fi

# Make a safe copy of touchcal.txt if required
if [ -f "/home/pi/rpidatv/scripts/touchcal.txt" ]; then
  cp -f -r /home/pi/rpidatv/scripts/touchcal.txt /home/pi/touchcal.txt
fi

# Delete any old update message image  201802040
rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

# Create the update image in the tempfs folder
convert -size 720x576 xc:white \
    -gravity Center -pointsize 50 -annotate 0 "Updating Portsdown\nSoftware\n\nPlease wait" \
    -gravity South -pointsize 50 -annotate 0 "DO NOT TURN POWER OFF" \
    /home/pi/tmp/update.jpg

# Display the update message on the desktop
sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

# Uninstall the apt-listchanges package to allow silent install of ca certificates
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

# Prepare to update the distribution (added 20170405)
sudo dpkg --configure -a
sudo apt-get clean
sudo apt-get update

# Update the distribution (added 20170403)
sudo apt-get -y dist-upgrade

# ---------- Update rpidatv -----------

cd /home/pi

# Check which source to download.  Default is production
# option -p or null is the production load
# option -d is development from davecrump
# option -s is staging from batc/staging
if [ "$1" == "-d" ]; then
  echo "Installing development load"
  wget https://github.com/davecrump/portsdown/archive/master.zip -O master.zip
elif [ "$1" == "-s" ]; then
  echo "Installing BATC Staging load"
  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/batc_staging.zip -O master.zip
else
  echo "Installing BATC Production load"
  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/master.zip -O master.zip
fi

# Unzip and overwrite where we need to
unzip -o master.zip

if [ "$1" == "-s" ]; then
  cp -f -r portsdown-batc_staging/bin rpidatv
  # cp -f -r portsdown-batc_staging/doc rpidatv
  cp -f -r portsdown-batc_staging/scripts rpidatv
  cp -f -r portsdown-batc_staging/src rpidatv
  rm -f portsdown/video/*.jpg
  cp -f -r portsdown-batc_staging/video rpidatv
  cp -f -r portsdown-batc_staging/version_history.txt rpidatv/version_history.txt
  rm master.zip
  rm -rf portsdown-batc_staging
else
  cp -f -r portsdown-master/bin rpidatv
  # cp -f -r portsdown-master/doc rpidatv
  cp -f -r portsdown-master/scripts rpidatv
  cp -f -r portsdown-master/src rpidatv
  rm -f rpidatv/video/*.jpg
  cp -f -r portsdown-master/video rpidatv
  cp -f -r portsdown-master/version_history.txt rpidatv/version_history.txt
  rm master.zip
  rm -rf portsdown-master
fi

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

# Disable fallback IP (201701230)

cd /etc
sudo sed -i '/profile static_eth0/d' dhcpcd.conf
sudo sed -i '/static ip_address=192.168.1.60/d' dhcpcd.conf
sudo sed -i '/static routers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/static domain_name_servers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/interface eth0/d' dhcpcd.conf
sudo sed -i '/fallback static_eth0/d' dhcpcd.conf

# Disable the Touchscreen Screensaver (201701070)
cd /boot
if ! grep -q consoleblank cmdline.txt; then
  sudo sed -i -e 's/rootwait/rootwait consoleblank=0/' cmdline.txt
fi
cd /etc/kbd
sudo sed -i 's/^BLANK_TIME.*/BLANK_TIME=0/' config
sudo sed -i 's/^POWERDOWN_TIME.*/POWERDOWN_TIME=0/' config
cd /home/pi

# Restore or update portsdown_config.txt 20180204

if [ -f "/home/pi/portsdown_config.txt" ]; then  ## file exists, so restore it
  cp -f -r /home/pi/portsdown_config.txt /home/pi/rpidatv/scripts/portsdown_config.txt
  cp -f -r /home/pi/portsdown_presets.txt /home/pi/rpidatv/scripts/portsdown_presets.txt
else           ## file does not exist, so copy relavent items from portsdown_config.txt
  source /home/pi/rpidatv/scripts/copy_config.sh
fi
## rm -f /home/pi/rpidatvconfig.txt
rm -f /home/pi/portsdown_config.txt
rm -f /home/pi/portsdown_presets.txt
rm -f /home/pi/rpidatv/scripts/copy_config.sh

# Install Waveshare 3.5B DTOVERLAY if required (201704080)
if [ ! -f /boot/overlays/waveshare35b.dtbo ]; then
  sudo cp /home/pi/rpidatv/scripts/waveshare35b.dtbo /boot/overlays/
fi

# Load new .bashrc to source the startup script at boot and log-on (201704160)
cp -f /home/pi/rpidatv/scripts/configs/startup.bashrc /home/pi/.bashrc

# Always auto-logon and run .bashrc (and hence startup.sh) (201704160)
sudo ln -fs /etc/systemd/system/autologin@.service\
 /etc/systemd/system/getty.target.wants/getty@tty1.service

# Reduce the dhcp client timeout to speed off-network startup (201704160)
# If required
if ! grep -q timeout /etc/dhcpcd.conf; then
  sudo bash -c 'echo -e "\n# Shorten dhcpcd timeout from 30 to 15 secs" >> /etc/dhcpcd.conf'
  sudo bash -c 'echo -e "timeout 15\n" >> /etc/dhcpcd.conf'
fi

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

# Check if tmpfs at ~/tmp exists.  If not,
# amend /etc/fstab to create a tmpfs drive at ~/tmp for multiple images (201708150)
if [ ! -d /home/pi/tmp ]; then
  sudo sed -i '4itmpfs           /home/pi/tmp    tmpfs   defaults,noatime,nosuid,size=10m  0  0' /etc/fstab
fi

# Check if ~/snaps folder exists for captured images.  Create if required
# and set the snap index number to zero. (201708150)
if [ ! -d /home/pi/snaps ]; then
  mkdir /home/pi/snaps
  echo "0" > /home/pi/snaps/snap_index.txt
fi

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

# Either install FreqShow, or downgrade sdl so that it works (20180101)
if [ -f "/home/pi/FreqShow/LICENSE" ]; then
  # Freqshow has already been installed, so downgrade the sdl version
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
else
  # Start the install from scratch
  sudo apt-get -y install python-pip pandoc python-numpy pandoc python-pygame gdebi-core
  sudo pip install pyrtlsdr
  # Load the old (1.2.15-5) version of sdl.  Later versions do not work
  sudo gdebi --non-interactive /home/pi/rpidatv/scripts/configs/freqshow/libsdl1.2debian_1.2.15-5_armhf.deb
  # Load touchscreen configuration
  sudo cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_pointercal /etc/pointercal
  # Download FreqShow
  git clone https://github.com/adafruit/FreqShow.git
  # Change the settings for our environment
  rm /home/pi/FreqShow/freqshow.py
  cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_freqshow.py /home/pi/FreqShow/freqshow.py
  rm /home/pi/FreqShow/model.py
  cp /home/pi/rpidatv/scripts/configs/freqshow/waveshare_146_model.py /home/pi/FreqShow/model.py
fi

# Update the version number
rm -rf /home/pi/rpidatv/scripts/installed_version.txt
cp /home/pi/rpidatv/scripts/latest_version.txt /home/pi/rpidatv/scripts/installed_version.txt
cp -f -r /home/pi/prev_installed_version.txt /home/pi/rpidatv/scripts/prev_installed_version.txt
rm -rf /home/pi/prev_installed_version.txt

# Delete any old update image
rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

# Create the update image in the tempfs folder
convert -size 720x576 xc:white \
    -gravity Center -pointsize 50 -annotate 0 "Update Complete\n\nPLEASE REBOOT NOW\n\n(by pressing "y" on the keyboard)" \
    /home/pi/tmp/update.jpg

# Display the reboot message on the touchscreen
sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
(sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

# Offer reboot
printf "A reboot may be required before using the update.\n"
printf "Do you want to reboot now? (y/n)\n"
read -n 1
printf "\n"
if [[ "$REPLY" = "y" || "$REPLY" = "Y" ]]; then
  printf "\nRebooting\n"
  sudo reboot now
fi
exit
