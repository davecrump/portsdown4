 ../avc2ts/avc2ts -t 2 -m 403200 -b 220000 -x 320 -y 240 -f 25 -d 800 -o /dev/stdout |../libdvbmod/DvbTsToIQ/dvb2iq  -s 250 -f 7/8 -r 4 -m DVBS | sudo ./limesdr_send -f 437e6 -b 2.5e6 -s 250000 -g 0.7 -p 0.05 -a BAND2 -r 4 -l 102400

