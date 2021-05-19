#!/bin/bash

# Updated by davecrump 202101310 for Portsdown 4 and Knucker

DisplayUpdateMsg() {
  # Delete any old update message image
  rm /home/pi/tmp/update.jpg >/dev/null 2>/dev/null

  # Create the update image in the tempfs folder
  convert -size 800x480 xc:white \
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
  convert -size 800x480 xc:white \
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

## Check which update to load
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
  echo "Updating to latest Production Portsdown RPi 4 build";
elif [ "$GIT_SRC" == "davecrump" ]; then
  echo "Updating to latest development Portsdown RPi 4 build";
else
  echo "Updating to latest ${GIT_SRC} development Portsdown RPi 4 build";
fi

printf "Pausing Streamer or TX if running.\n\n"
sudo killall keyedstream >/dev/null 2>/dev/null
sudo killall keyedtx >/dev/null 2>/dev/null
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

# Make a safe copy of the LongMynd config
cp -f -r "$PATHSCRIPT"/longmynd_config.txt "$PATHUBACKUP"/longmynd_config.txt

# Make a safe copy of the Lime Calibration frequency or status
cp -f -r "$PATHSCRIPT"/limecalfreq.txt "$PATHUBACKUP"/limecalfreq.txt

# Make a safe copy of the Band Viewer config
cp -f -r /home/pi/rpidatv/src/bandview/bandview_config.txt "$PATHUBACKUP"/bandview_config.txt

# Make a safe copy of the User Button scripts
cp -f -r "$PATHSCRIPT"/user_button1.sh "$PATHUBACKUP"/user_button1.sh
cp -f -r "$PATHSCRIPT"/user_button2.sh "$PATHUBACKUP"/user_button2.sh
cp -f -r "$PATHSCRIPT"/user_button3.sh "$PATHUBACKUP"/user_button3.sh
cp -f -r "$PATHSCRIPT"/user_button4.sh "$PATHUBACKUP"/user_button4.sh
cp -f -r "$PATHSCRIPT"/user_button5.sh "$PATHUBACKUP"/user_button5.sh

# Make a safe copy of the transmit start and transmit stop scripts
cp -f -r "$PATHSCRIPT"/TXstartextras.sh "$PATHUBACKUP"/TXstartextras.sh
cp -f -r "$PATHSCRIPT"/TXstopextras.sh "$PATHUBACKUP"/TXstopextras.sh

DisplayUpdateMsg "Step 4 of 10\nUpdating Software Package List\n\nXXXX------"

sudo dpkg --configure -a     # Make sure that all the packages are properly configured
sudo apt-get clean           # Clean up the old archived packages
sudo apt-get update          # Update the package list

DisplayUpdateMsg "Step 5 of 10\nUpdating Software Packages\n\nXXXX------"

# --------- Update Packages ------

sudo apt-get -y dist-upgrade # Upgrade all the installed packages to their latest version

# --------- Install new packages as Required ---------

echo
# Install libiio and dependencies if required (used for Pluto SigGen)
if [ ! -d  /home/pi/libiio ]; then
  echo "Installing libiio and dependencies"
  echo
  sudo apt-get -y install libxml2 libxml2-dev bison flex libcdk5-dev
  sudo apt-get -y install libaio-dev libserialport-dev libxml2-dev libavahi-client-dev
  cd /home/pi
  git clone https://github.com/analogdevicesinc/libiio.git
  cd libiio
  cmake ./
  make all
  sudo make install
  cd /home/pi
else
  echo "Found libiio installed"
  echo
fi

# -----------Update LimeSuite if required -------------

if ! grep -q be27699 /home/pi/LimeSuite/commit_tag.txt; then

  # Remove old LimeSuite
  rm -rf /home/pi/LimeSuite/

  # Install LimeSuite 20.10 as at 25 Jan 21
  # Commit be276996ec3f23b2aadc10543add867d1a55afdd
  echo
  echo "--------------------------------------"
  echo "----- Installing LimeSuite 20.10 -----"
  echo "--------------------------------------"
  cd /home/pi
  wget https://github.com/myriadrf/LimeSuite/archive/be276996ec3f23b2aadc10543add867d1a55afdd.zip -O master.zip
  unzip -o master.zip
  cp -f -r LimeSuite-be276996ec3f23b2aadc10543add867d1a55afdd LimeSuite
  rm -rf LimeSuite-be276996ec3f23b2aadc10543add867d1a55afdd
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
  echo "be27699" >/home/pi/LimeSuite/commit_tag.txt

  # Download the 20.10LimeSDR Mini firmware/gateware version
  echo
  echo "------------------------------------------------------"
  echo "----- Downloading LimeSDR Mini Firmware versions -----"
  echo "------------------------------------------------------"

  # Current Version from LimeSuite 20.10 
  mkdir -p /home/pi/.local/share/LimeSuite/images/20.10/
  wget https://downloads.myriadrf.org/project/limesuite/20.10/LimeSDR-Mini_HW_1.2_r1.30.rpd -O \
    /home/pi/.local/share/LimeSuite/images/20.10/LimeSDR-Mini_HW_1.2_r1.30.rpd
fi


# ---------- Update rpidatv -----------

DisplayUpdateMsg "Step 6 of 10\nDownloading Portsdown SW\n\nXXXXX-----"

cd /home/pi

# Delete previous update folder if downloaded in error
rm -rf portsdown4-master >/dev/null 2>/dev/null

# Download selected source of rpidatv
wget https://github.com/${GIT_SRC}/portsdown4/archive/master.zip -O master.zip

# Unzip and overwrite where we need to
unzip -o master.zip
cp -f -r portsdown4-master/bin rpidatv
cp -f -r portsdown4-master/scripts rpidatv
cp -f -r portsdown4-master/src rpidatv
rm -f rpidatv/video/*.jpg
cp -f -r portsdown4-master/video rpidatv
cp -f -r portsdown4-master/version_history.txt rpidatv/version_history.txt
rm master.zip
rm -rf portsdown4-master
cd /home/pi

DisplayUpdateMsg "Step 7 of 10\nCompiling Portsdown SW\n\nXXXXXX----"

# Compile rpidatv gui
sudo killall -9 rpidatvgui
echo "Installing rpidatvgui"
cd /home/pi/rpidatv/src/gui
#make clean
make
sudo make install
cd /home/pi

# Update limesdr_toolbox
echo "Updating limesdr_toolbox"

cd /home/pi/rpidatv/src/limesdr_toolbox

# Install sub project dvb modulation
# Download and overwrite
wget https://github.com/F5OEO/libdvbmod/archive/master.zip -O master.zip
unzip -o master.zip
rm -rf libdvbmod
cp -f -r libdvbmod-master libdvbmod
rm master.zip
rm -rf libdvbmod-master

# Make libdvbmod
cd libdvbmod/libdvbmod
make
cd ../DvbTsToIQ/
make
cp dvb2iq /home/pi/rpidatv/bin/

#Make limesdr_toolbox
cd /home/pi/rpidatv/src/limesdr_toolbox/
make 
cp limesdr_send /home/pi/rpidatv/bin/
cp limesdr_dump /home/pi/rpidatv/bin/
cp limesdr_stopchannel /home/pi/rpidatv/bin/
cp limesdr_forward /home/pi/rpidatv/bin/
make dvb
cp limesdr_dvb /home/pi/rpidatv/bin/
cd /home/pi

echo
echo "--------------------------------"
echo "----- Updating dvb_t_stack -----"
echo "--------------------------------"
cd /home/pi/rpidatv/src/dvb_t_stack/Release
make clean
make
cp dvb_t_stack /home/pi/rpidatv/bin/dvb_t_stack

# Install the DATV Express firmware files
cd /home/pi/rpidatv/src/dvb_t_stack
sudo cp datvexpress16.ihx /lib/firmware/datvexpress/datvexpress16.ihx
sudo cp datvexpressraw16.rbf /lib/firmware/datvexpress/datvexpressraw16.rbf
cd /home/pi

echo
echo "------------------------------------------"
echo "----- Updating the LongMynd Receiver -----"
echo "------------------------------------------"
cd /home/pi
rm -rf longmynd
cp -r /home/pi/rpidatv/src/longmynd/ /home/pi/
cd longmynd
make
cd /home/pi

echo
echo "------------------------------------------"
echo "----- Compiling the Signal Generator -----"
echo "------------------------------------------"
cd /home/pi/rpidatv/src/siggen
make
sudo make install
cd /home/pi

echo
echo "----------------------------------------"
echo "----- Compiling the ADF4351 driver -----"
echo "----------------------------------------"
cd /home/pi/rpidatv/src/adf4351
make
cp adf4351 ../../bin/
cd /home/pi

# Compile Band Viewer
echo
echo "---------------------------------"
echo "----- Compiling Band Viewer -----"
echo "---------------------------------"
cd /home/pi/rpidatv/src/bandview
make
cp bandview ../../bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission (201710080)
echo "Installing keyedtx"
cd /home/pi/rpidatv/src/keyedtx
make
mv keyedtx /home/pi/rpidatv/bin/
cd /home/pi

# Compile and install the executable for GPIO-switched transmission with touch (202003020)
cd /home/pi/rpidatv/src/keyedtxtouch
make
mv keyedtxtouch /home/pi/rpidatv/bin/
cd /home/pi

# Compile the Attenuator Driver (201801060)
echo "Installing atten"
cd /home/pi/rpidatv/src/atten
make
cp /home/pi/rpidatv/src/atten/set_attenuator /home/pi/rpidatv/bin/set_attenuator
cd /home/pi


DisplayUpdateMsg "Step 8 of 10\nRestoring Config\n\nXXXXXXXX--"

# Restore portsdown_config.txt and portsdown_presets.txt
cp -f -r "$PATHUBACKUP"/portsdown_config.txt "$PATHSCRIPT"/portsdown_config.txt
cp -f -r "$PATHUBACKUP"/portsdown_presets.txt "$PATHSCRIPT"/portsdown_presets.txt

# Restore the user's original siggencal.txt
#cp -f -r "$PATHUBACKUP"/siggencal.txt /home/pi/rpidatv/src/siggen/siggencal.txt

# Restore the user's original siggenconfig.txt
#cp -f -r "$PATHUBACKUP"/siggenconfig.txt /home/pi/rpidatv/src/siggen/siggenconfig.txt

# Restore the user's rtl-fm_presets.txt
cp -f -r "$PATHUBACKUP"/rtl-fm_presets.txt "$PATHSCRIPT"/rtl-fm_presets.txt

# Restore the user's original portsdown_locators.txt
cp -f -r "$PATHUBACKUP"/portsdown_locators.txt "$PATHSCRIPT"/portsdown_locators.txt

# Restore the user's original rx_presets.txt
cp -f -r "$PATHUBACKUP"/rx_presets.txt "$PATHSCRIPT"/rx_presets.txt

# Restore the user's original stream presets
cp -f -r "$PATHUBACKUP"/stream_presets.txt "$PATHSCRIPT"/stream_presets.txt 

# Restore the user's original Jetson configuration
cp -f -r "$PATHUBACKUP"/jetson_config.txt "$PATHSCRIPT"/jetson_config.txt

# Restore the user's original LongMynd config
cp -f -r "$PATHUBACKUP"/longmynd_config.txt "$PATHSCRIPT"/longmynd_config.txt

# Restore the user's original Lime Calibration frequency or status
cp -f -r "$PATHUBACKUP"/limecalfreq.txt "$PATHSCRIPT"/limecalfreq.txt

# Restore the user's original Band Viewer config (but not yet)
# cp -f -r "$PATHUBACKUP"/bandview_config.txt /home/pi/rpidatv/src/bandview/bandview_config.txt 

# Restore the user's original User Button scripts
cp -f -r "$PATHUBACKUP"/user_button1.sh "$PATHSCRIPT"/user_button1.sh
cp -f -r "$PATHUBACKUP"/user_button2.sh "$PATHSCRIPT"/user_button2.sh
cp -f -r "$PATHUBACKUP"/user_button3.sh "$PATHSCRIPT"/user_button3.sh
cp -f -r "$PATHUBACKUP"/user_button4.sh "$PATHSCRIPT"/user_button4.sh
cp -f -r "$PATHUBACKUP"/user_button5.sh "$PATHSCRIPT"/user_button5.sh

# Restore the user's original transmit start and transmit stop scripts
cp -f -r "$PATHUBACKUP"/TXstartextras.sh "$PATHSCRIPT"/TXstartextras.sh
cp -f -r "$PATHUBACKUP"/TXstopextras.sh "$PATHSCRIPT"/TXstopextras.sh

# Add Mic Gain parameter to config file if not included
if ! grep -q micgain "$PATHSCRIPT"/portsdown_config.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' "$PATHSCRIPT"/portsdown_config.txt
  # Add the new entry and a new line 
  echo "micgain=26" >> "$PATHSCRIPT"/portsdown_config.txt
fi

# Add new parameters to config file if not included  202101090
if ! grep -q udpoutport "$PATHSCRIPT"/portsdown_config.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' "$PATHSCRIPT"/portsdown_config.txt
  # Add the new entry and a new line 
  echo "udpoutport=10000" >> "$PATHSCRIPT"/portsdown_config.txt
  echo "udpinport=10000" >> "$PATHSCRIPT"/portsdown_config.txt
  echo "guard=32" >> "$PATHSCRIPT"/portsdown_config.txt
fi

# Add another new parameter to config file if not included  202101180
if ! grep -q qam= "$PATHSCRIPT"/portsdown_config.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' "$PATHSCRIPT"/portsdown_config.txt
  # Add the new entry and a new line 
  echo "qam=qpsk" >> "$PATHSCRIPT"/portsdown_config.txt
fi

# Add new receiver parameters to longmynd_config.txt if not included
if ! grep -q tstimeout "$PATHSCRIPT"/longmynd_config.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' "$PATHSCRIPT"/longmynd_config.txt
  # Add the new entry and a new line 
  echo "tstimeout=5000" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "tstimeout1=10000" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "scanwidth=50" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "scanwidth1=50" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "chan=0" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "chan1=0" >> "$PATHSCRIPT"/longmynd_config.txt
  echo "rxmod=dvbs" >> "$PATHSCRIPT"/longmynd_config.txt
fi

# Add adf4153 reference freq to siggenconfig.txt if not included
if ! grep -q adf4153ref /home/pi/rpidatv/src/siggen/siggenconfig.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' /home/pi/rpidatv/src/siggen/siggenconfig.txt
  # Add the new entry and a new line 
  echo "adf4153ref=20000000" >> /home/pi/rpidatv/src/siggen/siggenconfig.txt
fi

# Add new slo and adf4153 parameters to siggencal.txt if not included
if ! grep -q slopoints /home/pi/rpidatv/src/siggen/siggencal.txt; then
  # File needs updating
  # Delete any blank lines first
  sed -i -e '/^$/d' /home/pi/rpidatv/src/siggen/siggencal.txt
  # Add the new entries and a new line 
  echo "slopoints=2" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "slofreq1=10000000000" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "slolev1=140" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "slofreq2=14000000000" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "slolev1=140" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "adf4153points=2" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "adf4153freq1=500000000" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "adf4153lev1=0" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "adf4153freq2=4000000000" >> /home/pi/rpidatv/src/siggen/siggencal.txt
  echo "adf4153lev2=0" >> /home/pi/rpidatv/src/siggen/siggencal.txt
fi

DisplayUpdateMsg "Step 9 of 10\nFinishing Off\n\nXXXXXXXXX-"

# Update the version number

cp /home/pi/prev_installed_version.txt /home/pi/rpidatv/scripts/prev_installed_version.txt
rm /home/pi/prev_installed_version.txt
rm /home/pi/rpidatv/scripts/installed_version.txt
cp /home/pi/rpidatv/scripts/latest_version.txt /home/pi/rpidatv/scripts/installed_version.txt

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
