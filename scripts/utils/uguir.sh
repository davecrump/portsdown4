# Compile rpidatv gui
cd /home/pi/rpidatv/src/gui
make clean
make
sudo make install
cd ../

cd /home/pi
reset
sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/bin/rpidatvgui 1

