#!/bin/bash

# set -x

cd /home/pi

# Stop Langstone if it is running (it shouldn't be)
/home/pi/Langstone/stop_both

# Keep a copy of the old Config files
cp -f /home/pi/Langstone/Langstone_Lime.conf /home/pi/rpidatv/scripts/configs/Langstone_Lime.conf
cp -f /home/pi/Langstone/Langstone_Pluto.conf /home/pi/rpidatv/scripts/configs/Langstone_Pluto.conf

# Remove the old Langstone build
rm -rf /home/pi/Langstone

# Clone the latest version
git clone http://www.github.com/g4eml/Langstone-V2
mv Langstone-V2 Langstone

# Restore the old Config files
cp -f /home/pi/rpidatv/scripts/configs/Langstone_Lime.conf /home/pi/Langstone/Langstone_Lime.conf 
cp -f /home/pi/rpidatv/scripts/configs/Langstone_Pluto.conf /home/pi/Langstone/Langstone_Pluto.conf 

chmod +x /home/pi/Langstone/build
chmod +x /home/pi/Langstone/run_lime
chmod +x /home/pi/Langstone/stop_lime
chmod +x /home/pi/Langstone/run_pluto
chmod +x /home/pi/Langstone/stop_pluto
chmod +x /home/pi/Langstone/update
chmod +x /home/pi/Langstone/set_pluto
chmod +x /home/pi/Langstone/set_sdr
chmod +x /home/pi/Langstone/set_sound
chmod +x /home/pi/Langstone/run_both
chmod +x /home/pi/Langstone/stop_both

/home/pi/Langstone/build
/home/pi/Langstone/set_sdr

exit

