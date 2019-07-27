#!/bin/bash

# Updated by davecrump 201906060

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

DisplayRebootMsg() {
  # Delete any old update message image  201802040
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 720x576 xc:white \
    -gravity North -pointsize 40 -annotate 0 "\nUpdating Portsdown Software" \
    -gravity Center -pointsize 50 -annotate 0 "$1""\n\nDone" \
    -gravity South -pointsize 50 -annotate 0 "SAFE TO POWER OFF" \
    /home/pi/tmp/update.jpg

  # Display the update message on the desktop
  sudo fbi -T 1 -noverbose -a /home/pi/tmp/update.jpg >/dev/null 2>/dev/null
  (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
}

############ Function to Read from Config File ###############

get_config_var() {
lua - "$1" "$2" <<EOF
local key=assert(arg[1])
local fn=assert(arg[2])
local file=assert(io.open(fn))
for line in file:lines() do
local val = line:match("^#?%s*"..key.."=(.*)$")
if (val ~= nil) then
print(val)
break
end
end
EOF
}

reset

printf "\nCommencing update.\n\n"

cd /home/pi

## Check which update to load. From M0DNY 201905090
GIT_SRC_FILE=".portsdown_gitsrc"
if [ -e ${GIT_SRC_FILE} ]; then
  GIT_SRC=$(</home/pi/${GIT_SRC_FILE})
else
  GIT_SRC="BritishAmateurTelevisionClub"
fi

## If previous version was Dev (davecrump), load production by default
if [ "$GIT_SRC" == "davecrump" ]; then
  GIT_SRC="BritishAmateurTelevisionClub"
fi

if [ "$1" == "-d" ]; then
  echo "Overriding to update to latest development version"
  GIT_SRC="davecrump"
fi

if [ "$GIT_SRC" == "BritishAmateurTelevisionClub" ]; then
  echo "Updating to latest Production Portsdown build";
elif [ "$GIT_SRC" == "davecrump" ]; then
  echo "Updating to latest development Portsdown build";
else
  echo "Updating to latest ${GIT_SRC} development Portsdown build";
fi

printf "Pausing Streamer or TX if running.\n\n"
killall keyedstream >/dev/null 2>/dev/null
sudo killall ffmpeg >/dev/null 2>/dev/null

DisplayUpdateMsg "Step 3 of 10\nSaving Current Config\n\nXXX-------"

PATHSCRIPT="/home/pi/rpidatv/scripts"
PATHUBACKUP="/home/pi/user_backups"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

# Note previous version number
cp -f -r /home/pi/rpidatv/scripts/installed_version.txt /home/pi/prev_installed_version.txt

# Create a folder for user configs
mkdir "$PATHUBACKUP" >/dev/null 2>/dev/null

# Make a safe copy of portsdown_config.txt and portsdown_presets
cp -f -r "$PATHSCRIPT"/portsdown_config.txt "$PATHUBACKUP"/portsdown_config.txt

# Make a safe copy of portsdown_presets.txt
cp -f -r "$PATHSCRIPT"/portsdown_presets.txt "$PATHUBACKUP"/portsdown_presets.txt

# Make a safe copy of siggencal.txt
cp -f -r /home/pi/rpidatv/src/siggen/siggencal.txt "$PATHUBACKUP"/siggencal.txt

# Make a safe copy of siggenconfig.txt
cp -f -r /home/pi/rpidatv/src/siggen/siggenconfig.txt "$PATHUBACKUP"/siggenconfig.txt

# Make a safe copy of touchcal.txt
cp -f -r /home/pi/rpidatv/scripts/touchcal.txt "$PATHUBACKUP"/touchcal.txt

# Make a safe copy of rtl-fm_presets.txt
cp -f -r "$PATHSCRIPT"/rtl-fm_presets.txt "$PATHUBACKUP"/rtl-fm_presets.txt

# Make a safe copy of portsdown_locators.txt
cp -f -r "$PATHSCRIPT"/portsdown_locators.txt "$PATHUBACKUP"/portsdown_locators.txt

# Make a safe copy of rx_presets.txt
cp -f -r "$PATHSCRIPT"/rx_presets.txt "$PATHUBACKUP"/rx_presets.txt

# Make a safe copy of the Stream Presets
cp -f -r "$PATHSCRIPT"/stream_presets.txt "$PATHUBACKUP"/stream_presets.txt

# Make a safe copy of the Jetson config
cp -f -r "$PATHSCRIPT"/jetson_config.txt "$PATHUBACKUP"/jetson_config.txt

# Make a safe copy of the User Button scripts
cp -f -r "$PATHSCRIPT"/user_button1.sh "$PATHUBACKUP"/user_button1.sh
cp -f -r "$PATHSCRIPT"/user_button2.sh "$PATHUBACKUP"/user_button2.sh
cp -f -r "$PATHSCRIPT"/user_button3.sh "$PATHUBACKUP"/user_button3.sh
cp -f -r "$PATHSCRIPT"/user_button4.sh "$PATHUBACKUP"/user_button4.sh
cp -f -r "$PATHSCRIPT"/user_button5.sh "$PATHUBACKUP"/user_button5.sh

# Make a safe copy of the transmit start and transmit stop scripts
cp -f -r "$PATHSCRIPT"/TXstartextras.sh "$PATHUBACKUP"/TXstartextras.sh
cp -f -r "$PATHSCRIPT"/TXstopextras.sh "$PATHUBACKUP"/TXstopextras.sh

# Delete any old backups in the home directory
if [ -e "/home/pi/portsdown_config.txt" ]; then
  rm /home/pi/portsdown_config.txt
  rm /home/pi/portsdown_presets.txt
  rm /home/pi/siggencal.txt
  rm /home/pi/touchcal.txt
  rm /home/pi/rtl-fm_presets.txt
  rm /home/pi/portsdown_locators.txt
  rm /home/pi/rx_presets.txt
  rm /home/pi/stream_presets.txt
  rm /home/pi/jetson_config.txt
fi

# Delete any old update message image  201802040
rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

DisplayUpdateMsg "Step 4 of 10\nUpdating Software Packages\n\nXXXX------"

# Uninstall the apt-listchanges package to allow silent install of ca certificates
# http://unix.stackexchange.com/questions/124468/how-do-i-resolve-an-apparent-hanging-update-process
sudo apt-get -y remove apt-listchanges

sudo dpkg --configure -a     # Make sure that all the packages are properly configured
sudo apt-get clean           # Clean up the old archived packages
sudo apt-get update          # Update the package list

DisplayUpdateMsg "Step 4a of 10\nStill Updating Software Packages\n\nXXXX------"

# --------- Update Packages ------

# Don't do this until Comp Vid bug is corrected 201907270
#sudo apt-get -y dist-upgrade # Upgrade all the installed packages to their latest version

# --------- Install the Random Number Generator ------

sudo apt-get -y install rng-tools # This makes sure that there is enough entropy for wget

# --------- Install sshpass ------
sudo apt-get -y install sshpass  # For controlling the Jetson Nano

# Enable USB Storage automount in Stretch (only) 20180704
cd /lib/systemd/system/
if ! grep -q MountFlags=shared systemd-udevd.service; then
  sudo sed -i -e 's/MountFlags=slave/MountFlags=shared/' systemd-udevd.service
fi

# Check if Lime needs to be updated
# Look for Commit 42f752a
grep -Fxqs "42f752a" /home/pi/LimeSuite/commit_tag.txt
Lime_Update_Not_Required=$?

if [ $Lime_Update_Not_Required != 0 ]; then

  # Install packages here to catch first-time Lime Install
  sudo apt-get -y install libsqlite3-dev libi2c-dev 

  # Delete the old installation files
  sudo rm -rf /usr/local/lib/cmake/LimeSuite/* >/dev/null 2>/dev/null
  sudo rm -rf /usr/local/include/lime/* >/dev/null 2>/dev/null
  sudo rm -rf /usr/local/lib/libLimeSuite.* >/dev/null 2>/dev/null
  sudo rm -rf /usr/local/lib/pkgconfig/LimeSuite.pc >/dev/null 2>/dev/null
  sudo rm -rf /usr/local/bin/LimeUtil >/dev/null 2>/dev/null
  sudo rm -rf /usr/local/bin/LimeQuickTest >/dev/null 2>/dev/null
  sudo rm -rf /home/pi/LimeSuite >/dev/null 2>/dev/null

  # Install LimeSuite 19.01 as at 12 Feb 19
  # Commit 42f752af905a5b4464cdb95964e408a4682b4ffa
  cd /home/pi
  wget https://github.com/myriadrf/LimeSuite/archive/42f752af905a5b4464cdb95964e408a4682b4ffa.zip -O master.zip
  unzip -o master.zip
  cp -f -r LimeSuite-42f752af905a5b4464cdb95964e408a4682b4ffa LimeSuite
  rm -rf LimeSuite-42f752af905a5b4464cdb95964e408a4682b4ffa

  rm master.zip

  # Compile LimeSuite
  cd LimeSuite/
  mkdir dirbuild
  cd dirbuild/
  cmake ../
  make
  sudo make install
  sudo ldconfig

  # Install udev rules for LimeSuite
  cd /home/pi
  cd LimeSuite/udev-rules
  chmod +x install.sh
  sudo /home/pi/LimeSuite/udev-rules/install.sh

  # Record the LimeSuite Version
  echo "42f752a" >/home/pi/LimeSuite/commit_tag.txt
fi

# Delete old limetool and binary
rm -r -f /home/pi/limetool
rm -r -f /home/pi/rpidatv/bin/limetx

# ---------- Update rpidatv -----------

DisplayUpdateMsg "Step 5 of 10\nDownloading Portsdown SW\n\nXXXXX-----"

cd /home/pi

# Download selected source of rpidatv
wget https://github.com/${GIT_SRC}/portsdown/archive/master.zip -O master.zip

# Check which source to download.  Default is production
# option -p or null is the production load
# option -d is development from davecrump

#if [ "$1" == "-d" ]; then
#  echo "Installing development load"
#  wget https://github.com/davecrump/portsdown/archive/master.zip -O master.zip
#else
#  echo "Installing BATC Production load"
#  wget https://github.com/BritishAmateurTelevisionClub/portsdown/archive/master.zip -O master.zip
#fi

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
cd /home/pi

# Check if avc2ts dependencies need to be installed 20190420
avc2ts_Deps_Not_Required=1
if [ -f /home/pi/avc2ts/libmpegts/README ]; then
  avc2ts_Deps_Not_Required=0
  echo "avc2ts dependencies not required"
else
  echo "avc2ts dependencies required and will be installed after avc2ts"
fi

# Download selected source of rpidatv
wget https://github.com/${GIT_SRC}/avc2ts/archive/master.zip

# Check which avc2ts to download.  Default is production
# option d is development from davecrump
#if [ "$1" == "-d" ]; then
#  echo "Installing development avc2ts"
#  wget https://github.com/davecrump/avc2ts/archive/master.zip
#else
#  echo "Installing BATC Production avc2ts"
#  wget https://github.com/BritishAmateurTelevisionClub/avc2ts/archive/master.zip
#fi

# Unzip the avc2ts software
unzip -o master.zip

# Overwrite files in ~/avc2ts without deleting dependencies
cp -f -r -T avc2ts-master/ /home/pi/avc2ts/
rm master.zip
rm -rf avc2ts-master

DisplayUpdateMsg "Step 6 of 10\nCompiling Portsdown SW\n\nXXXXXX----"

# Compile rpidatv core
sudo killall -9 rpidatv
echo "Installing rpidatv"
cd rpidatv/src
touch rpidatv.c
make clean
make
sudo make install

# Test if the device is a LimeNet Micro which needs the dt-blob.bin changing
# (0 for LimeNet Micro detected, 1 for not detected)
cat /proc/device-tree/model | grep 'Raspberry Pi Compute Module 3' >/dev/null 2>/dev/null
LIMENET_RESULT="$?"

if [ "$LIMENET_RESULT" == 0 ]; then
  # LimeNET-micro detected, so change dt-blob.bin
  sudo cp /home/pi/rpidatv/scripts/configs/dt-blob.bin.lmn /boot/dt-blob.bin
fi



# Compile rpidatv gui
sudo killall -9 rpidatvgui
echo "Installing rpidatvgui"
cd gui
make clean
make
sudo make install
cd ../


if [ $avc2ts_Deps_Not_Required != 0 ]; then
  DisplayUpdateMsg "Step 6a of 10\nTakes 20 Minutes\n\nXXXXXX----"

  # For libmpegts
  echo "Installing libmpegts"
  cd /home/pi/avc2ts
  git clone git://github.com/F5OEO/libmpegts
  cd libmpegts
  ./configure
  make
  cd ../

  # For libfdkaac
  echo "Installing libfdkaac"
  sudo apt-get -y install autoconf libtool
  git clone https://github.com/mstorsjo/fdk-aac
  cd fdk-aac
  ./autogen.sh
  ./configure
  make && sudo make install
  sudo ldconfig
  cd ../

  #libyuv should be used for fast picture transformation : not yet implemented
  echo "Installing libyuv"
  git clone https://chromium.googlesource.com/libyuv/libyuv
  cd libyuv
  #should patch linux.mk with -DHAVE_JPEG on CXX and CFLAGS
  #seems to be link with libjpeg9-dev
  make V=1 -f linux.mk
  cd ../

  # Required for ffmpegsrc.cpp
  sudo apt-get -y install libvncserver-dev libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev

fi

# Delete the old version of avc2ts (owned by root)
sudo rm /home/pi/rpidatv/bin/avc2ts

# Make the new avc2ts
echo "Installing avc2ts"
cd /home/pi/avc2ts
touch avc2ts.cpp
make
cp avc2ts ../rpidatv/bin/
cd ..

#install adf4351
echo "Installing adf4351"
cd /home/pi/rpidatv/src/adf4351
touch adf4351.c
make
cp adf4351 ../../bin/
cd /home/pi

#install H264 Decoder : hello_video
#compile ilcomponet first
cd /opt/vc/src/hello_pi/
sudo ./rebuild.sh

# install H264 player
echo "Installing hello_video"
cd /home/pi/rpidatv/src/hello_video
touch video.c
make
cp hello_video.bin ../../bin/

# install MPEG-2 player
echo "Installing hello_video2"
cd /home/pi/rpidatv/src/hello_video2
touch video.c
make
cp hello_video2.bin ../../bin/

# Check if omxplayer needs to be installed 201807150
if [ ! -f "/usr/bin/omxplayer" ]; then
  echo "Installing omxplayer"
  sudo apt-get -y install omxplayer
fi

# Install limesdr_toolbox
echo "Installing limesdr_toolbox"
cd /home/pi/rpidatv/src/limesdr_toolbox
cmake .
make
cp limesdr_dump  ../../bin/
cp limesdr_send ../../bin/
cp limesdr_stopchannel ../../bin/
cp limesdr_forward ../../bin/

# Update libdvbmod and DvbTsToIQ
echo "Installing libdvbmod and DvbTsToIQ"
cd /home/pi/rpidatv/src/libdvbmod
make dirmake
make
cd ../DvbTsToIQ

# First compile the dvb2iq to be used for mpeg-2
cp DvbTsToIQ2.cpp DvbTsToIQ.cpp
make
cp dvb2iq ../../bin/dvb2iq2

# Now compile the dvb2iq to be used for H264
cp DvbTsToIQ0.cpp DvbTsToIQ.cpp
make
cp dvb2iq ../../bin/dvb2iq

# There is no step 7!

# Disable fallback IP (201701230)

cd /etc
sudo sed -i '/profile static_eth0/d' dhcpcd.conf
sudo sed -i '/static ip_address=192.168.1.60/d' dhcpcd.conf
sudo sed -i '/static routers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/static domain_name_servers=192.168.1.1/d' dhcpcd.conf
sudo sed -i '/interface eth0/d' dhcpcd.conf
sudo sed -i '/fallback static_eth0/d' dhcpcd.conf

# Install the menu alias if required
if ! grep -q "menu" /home/pi/.bash_aliases; then
  echo "alias menu='/home/pi/rpidatv/scripts/menu.sh menu'" >> /home/pi/.bash_aliases
fi

DisplayUpdateMsg "Step 8 of 10\nRestoring Config\n\nXXXXXXXX--"

# Restore portsdown_config.txt and portsdown_presets.txt
cp -f -r "$PATHUBACKUP"/portsdown_config.txt "$PATHSCRIPT"/portsdown_config.txt
cp -f -r "$PATHUBACKUP"/portsdown_presets.txt "$PATHSCRIPT"/portsdown_presets.txt

# Update config file with modulation and limegain
if ! grep -q modulation /home/pi/rpidatv/scripts/portsdown_config.txt; then
  # File needs updating
  printf "Adding modulation and limegain to user's portsdown_config.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/portsdown_config.txt
  # Add the 2 new entries and a new line 
  echo "modulation=DVB-S" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "limegain=90" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "" >> /home/pi/rpidatv/scripts/portsdown_config.txt
fi

# Update config file with pilots and frames          201905090
if ! grep -q frames /home/pi/rpidatv/scripts/portsdown_config.txt; then
  # File needs updating
  printf "Adding pilots and frames to user's portsdown_config.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/portsdown_config.txt
  # Add the 2 new entries and a new line 
  echo "pilots=off" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "frames=long" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "" >> /home/pi/rpidatv/scripts/portsdown_config.txt
fi

# Update config file with format and encoding          201905090
if ! grep -q encoding /home/pi/rpidatv/scripts/portsdown_config.txt; then
  # File needs updating
  printf "Adding format and encoding to user's portsdown_config.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/portsdown_config.txt
  # Add the 2 new entries and a new line 
  echo "format=4:3" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "encoding=H264" >> /home/pi/rpidatv/scripts/portsdown_config.txt
  echo "" >> /home/pi/rpidatv/scripts/portsdown_config.txt
fi

# Update presets file with limegains for each band
if ! grep -q d1limegain /home/pi/rpidatv/scripts/portsdown_presets.txt; then
  # File needs updating
  printf "Adding band limegains to user's portsdown_presets.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/portsdown_presets.txt
  # Add the 9 new entries and a new line 
  echo "d1limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "d2limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "d3limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "d4limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "d5limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "t1limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "t2limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "t3limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "t4limegain=90" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
  echo "" >> /home/pi/rpidatv/scripts/portsdown_presets.txt
fi

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

# Compile updated pi-sdn that sets swapoff
echo "Installing pi-sdn"
cp -f /home/pi/rpidatv/src/pi-sdn/main.c /home/pi/pi-sdn-build/main.c
cd /home/pi/pi-sdn-build
make
mv pi-sdn /home/pi/
cd /home/pi

# Compile and install the executable for switched repeater streaming (201708150)
echo "Installing switched repeater streaming"
cd /home/pi/rpidatv/src/rptr
make
mv keyedstream /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission (201710080)
echo "Installing keyedtx"
cd /home/pi/rpidatv/src/keyedtx
make
mv keyedtx /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for the Stream Receiver (201807290)
echo "Installing streamrx"
cd /home/pi/rpidatv/src/streamrx
make
mv streamrx /home/pi/rpidatv/bin/
cd /home/pi

# Compile the Signal Generator (201710280)
echo "Installing siggen"
cd /home/pi/rpidatv/src/siggen
make clean
make
sudo make install
cd /home/pi

# Compile the Attenuator Driver (201801060)
echo "Installing atten"
cd /home/pi/rpidatv/src/atten
make
cp /home/pi/rpidatv/src/atten/set_attenuator /home/pi/rpidatv/bin/set_attenuator
cd /home/pi

# Compile the x-y display (201811100)
echo "Installing xy display"
cd /home/pi/rpidatv/src/xy
make
cp -f /home/pi/rpidatv/src/xy/xy /home/pi/rpidatv/bin/xy
cd /home/pi

# Install the components for Lime Grove
cp -r /home/pi/rpidatv/scripts/configs/dvbsdr/ /home/pi/dvbsdr/

# Always auto-logon and run .bashrc (and hence startup.sh) (20180729)
sudo ln -fs /etc/systemd/system/autologin@.service\
 /etc/systemd/system/getty.target.wants/getty@tty1.service

# Restore the user's original siggencal.txt
cp -f -r "$PATHUBACKUP"/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt

# Restore the user's original siggenconfig.txt
cp -f -r "$PATHUBACKUP"/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt

# Restore the user's original touchcal.txt
cp -f -r "$PATHUBACKUP"/touchcal.txt /home/pi/rpidatv/scripts/touchcal.txt

# Restore the user's rtl-fm_presets.txt
cp -f -r "$PATHUBACKUP"/rtl-fm_presets.txt "$PATHSCRIPT"/rtl-fm_presets.txt

if ! grep -q r0gain /home/pi/rpidatv/scripts/rtl-fm_presets.txt; then
  # File needs updating
  printf "Adding preset gains to user's rtl-fm_presets.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  # Add the 9 new entries and a new line 
  echo "r0gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r1gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r2gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r3gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r4gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r5gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r6gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r7gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r8gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "r9gain=30" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
  echo "" >> /home/pi/rpidatv/scripts/rtl-fm_presets.txt
fi

# Restore the user's original portsdown_locators.txt
cp -f -r "$PATHUBACKUP"/portsdown_locators.txt "$PATHSCRIPT"/portsdown_locators.txt

# Restore the user's original rx_presets.txt
cp -f -r "$PATHUBACKUP"/rx_presets.txt "$PATHSCRIPT"/rx_presets.txt

# Restore the user's original stream presets
cp -f -r "$PATHUBACKUP"/stream_presets.txt "$PATHSCRIPT"/stream_presets.txt 

# Update Stream presets if required
if ! grep -q streamurl1 /home/pi/rpidatv/scripts/stream_presets.txt; then
  # File needs updating
  printf "Adding treamurls and streamkeys to user's stream_presets.txt\n"
  # Delete any blank lines
  sed -i -e '/^$/d' /home/pi/rpidatv/scripts/stream_presets.txt
  # Add the 9 new entries and a new line 
  echo "streamurl1=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey1=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl2=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey2=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl3=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey3=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl4=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey4=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl5=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey5=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl6=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey6=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl7=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey7=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamurl8=rtmp://rtmp.batc.org.uk/live" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "streamkey8=callsign-keykey" >> /home/pi/rpidatv/scripts/stream_presets.txt
  echo "" >> /home/pi/rpidatv/scripts/stream_presets.txt
fi

# If user is upgrading a repeater streamer, add the cron job for 12-hourly reboot
if grep -q "startup=Cont_Stream_boot" /home/pi/rpidatv/scripts/portsdown_config.txt; then
  sudo crontab /home/pi/rpidatv/scripts/configs/rptrcron
fi

# Restore the user's original Jetson configuration
cp -f -r "$PATHUBACKUP"/jetson_config.txt "$PATHSCRIPT"/jetson_config.txt

# Restore the user's original User Button scripts
cp -f -r "$PATHUBACKUP"/user_button1.sh "$PATHSCRIPT"/user_button1.sh
cp -f -r "$PATHUBACKUP"/user_button2.sh "$PATHSCRIPT"/user_button2.sh
cp -f -r "$PATHUBACKUP"/user_button3.sh "$PATHSCRIPT"/user_button3.sh
cp -f -r "$PATHUBACKUP"/user_button4.sh "$PATHSCRIPT"/user_button4.sh
cp -f -r "$PATHUBACKUP"/user_button5.sh "$PATHSCRIPT"/user_button5.sh

# Restore the user's original transmit start and transmit stop scripts
cp -f -r "$PATHUBACKUP"/TXstartextras.sh "$PATHSCRIPT"/TXstartextras.sh
cp -f -r "$PATHUBACKUP"/TXstopextras.sh "$PATHSCRIPT"/TXstopextras.sh

# If user is upgrading a keyed streamer, add the cron job for 12-hourly reboot
if grep -q "startup=Keyed_Stream_boot" /home/pi/rpidatv/scripts/portsdown_config.txt; then
  sudo crontab /home/pi/rpidatv/scripts/configs/rptrcron
fi

# Install updated Waveshare driver if required
# First Check user-requested display type
DISPLAY=$(get_config_var display $PCONFIGFILE)
if [ "$DISPLAY" == "Waveshare" ]; then
  # Check updated driver tag (201907070 in config.txt)
  if ! grep -q "201907070" /boot/config.txt; then
    # This section modifies and replaces the end of /boot/config.txt
    # to allow (only) the correct LCD drivers to be loaded at next boot

    # Set constants for the amendment of /boot/config.txt
    PATHCONFIGS="/home/pi/rpidatv/scripts/configs"  ## Path to config files
    lead='^## Begin LCD Driver'                     ## Marker for start of inserted text
    tail='^## End LCD Driver'                       ## Marker for end of inserted text
    CHANGEFILE="/boot/config.txt"                   ## File requiring added text
    APPENDFILE=$PATHCONFIGS"/lcd_markers.txt"       ## File containing both markers
    TRANSFILE=$PATHCONFIGS"/transfer.txt"           ## File used for transfer

    grep -q "$lead" "$CHANGEFILE"     ## Is the first marker already present?
    if [ $? -ne 0 ]; then
      sudo bash -c 'cat '$APPENDFILE' >> '$CHANGEFILE' '  ## If not append the markers
    fi

    # Select the correct driver text
    INSERTFILE=$PATHCONFIGS"/waveshare.txt"

    # Replace whatever is between the markers with the driver text
    sed -e "/$lead/,/$tail/{ /$lead/{p; r $INSERTFILE
      }; /$tail/p; d }" $CHANGEFILE >> $TRANSFILE

    sudo cp "$TRANSFILE" "$CHANGEFILE"          ## Copy from the transfer file
    rm $TRANSFILE                               ## Delete the transfer file
  fi
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

# Save (overwrite) the git source used
echo "${GIT_SRC}" > /home/pi/${GIT_SRC_FILE}

# Reboot
DisplayRebootMsg "Step 10 of 10\nRebooting\n\nUpdate Complete"
printf "\nRebooting\n"

sleep 1
# Turn off swap to prevent reboot hang
sudo swapoff -a
sudo shutdown -r now  # Seems to be more reliable than reboot

exit
