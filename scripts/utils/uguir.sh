# Compile rpidatv gui
cd /home/pi/rpidatv/src/gui
make clean
make
if [ $? != "0" ]; then
  echo
  echo "failed install"
  cd /home/pi
  exit
fi
sudo make install
cd ../

cd /home/pi
reset
sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/scripts/scheduler.sh
