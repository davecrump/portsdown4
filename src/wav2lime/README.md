# wav2lime utility by M0DNY and G8GKQ

For replaying SDRSharp IQ files (RIFF format)
or SDR Console IQ files (RF64 format)

Usage: -i = input file
       -f = frequency in MHz.   Default 437 MHz (RIFF) or as specified in RF64 file
       -g = lime gain 0 to 1.0  Default 0.8
       -u = upsample factor.  Default 1
       -s = scale factor.  Default 1


/home/pi/rpidatv/bin/wav2lime -i /home/pi/hamtv_short.wav -g 0.9 -f 437