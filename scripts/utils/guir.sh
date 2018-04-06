# Run rpidatv gui

cd /home/pi
reset
sudo killall fbcp >/dev/null 2>/dev/null
fbcp &
/home/pi/rpidatv/bin/rpidatvgui

