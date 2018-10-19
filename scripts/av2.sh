#! /bin/bash
# set -x #Uncomment for testing

# Early version of video monitor bash script!

############# SET GLOBAL VARIABLES ####################

PATHRPI="/home/pi/rpidatv/bin"
PATHSCRIPT="/home/pi/rpidatv/scripts"
PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

############# MAKE SURE THAT WE KNOW WHERE WE ARE ##################

cd /home/pi

############ FUNCTION TO READ CONFIG FILE #############################

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

# ########################## KILL ALL PROCESSES BEFORE STARTING  ################

sudo killall -9 ffmpeg >/dev/null 2>/dev/null
sudo killall rpidatv >/dev/null 2>/dev/null
sudo killall hello_encode.bin >/dev/null 2>/dev/null
sudo killall hello_video2.bin >/dev/null 2>/dev/null
sudo killall h264yuv >/dev/null 2>/dev/null
sudo killall -9 avc2ts >/dev/null 2>/dev/null
#sudo killall express_server >/dev/null 2>/dev/null
# Leave Express Server running
sudo killall tcanim >/dev/null 2>/dev/null
# Kill netcat that night have been started for Express Srver
sudo killall netcat >/dev/null 2>/dev/null
sudo killall -9 netcat >/dev/null 2>/dev/null

############ FUNCTION TO IDENTIFY AUDIO DEVICES #############################

detect_audio()
{
  # Returns AUDIO_CARD=1 if any audio dongle, or video dongle with
  # audio, detected.  Else AUDIO_CARD=0

  # Then, depending on the values of  $AUDIO_PREF and $MODE_INPUT it sets
  # AUDIO_CARD_NUMBER (typically 0, 1 2 or 3)
  # AUDIO_CHANNELS (0 for no audio required, 1 for mic, 2 for video stereo capture)
  # AUDIO_SAMPLE (44100 for mic, 48000 for video stereo capture, and ?? for Webcam)

  # Set initial conditions for later testing
  MIC=9
  USBTV=9
  WCAM=9

  printf "Audio selection = $AUDIO_PREF \n"

  # Check on picture input type
  case "$MODE_INPUT" in
  "ANALOGCAM" | "ANALOGMPEG-2" | "ANALOG16MPEG-2" )
     PIC_INPUT="ANALOG"
  ;;
  "WEBCAMH264" | "WEBCAMMPEG-2" | "WEBCAM16MPEG-2" | "WEBCAMHDMPEG-2" \
  | "C920H264" | "C920HDH264" | "C920FHDH264")
    PIC_INPUT="WEBCAM"
  ;;
  *)
    PIC_INPUT="DIGITAL"
    printf "Mode Input is $MODE_INPUT \n"
  ;;
  esac
  printf "Video Input is $PIC_INPUT \n"

  # First check if any audio card is present
  arecord -l | grep -q 'card'
  if [ $? != 0 ]; then   ## not present
    printf "Audio card not present\n"
    # No known card detected so take the safe option and go for beeps
    # unless noaudio selected:
    if [ "$AUDIO_PREF" == "no_audio" ]; then  # no audio
      AUDIO_CARD=0
      AUDIO_CHANNELS=0
      AUDIO_SAMPLE=44100
    else                                      # bleeps
      AUDIO_CARD=0
      AUDIO_CHANNELS=1
      AUDIO_SAMPLE=44100
    fi
  else                   ## card detected
    printf "Audio card present\n"
    # Check for the presence of a dedicated audio device
    arecord -l | grep -E -q "USB Audio Device|USB AUDIO|Head|Sound Device"
    if [ $? == 0 ]; then   ## Present
      # Look for the dedicated USB Audio Device, select the line and take
      # the 6th character.  Max card number = 8 !!
      MIC="$(arecord -l | grep -E "USB Audio Device|USB AUDIO|Head|Sound Device" \
        | head -c 6 | tail -c 1)"
    fi

    # Check for the presence of a Video dongle with audio
    arecord -l | grep -E -q \
      "usbtv|U0x534d0x21|DVC90|Cx231xxAudio|STK1160|U0xeb1a0x2861|AV TO USB|Grabby"
    if [ $? == 0 ]; then   ## Present
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      USBTV="$(arecord -l | grep -E \
        "usbtv|U0x534d0x21|DVC90|Cx231xxAudio|STK1160|U0xeb1a0x2861|AV TO USB|Grabby" \
        | head -c 6 | tail -c 1)"
    fi

    C920Present=0
    # Check for the presence of a C920 Webcam with stereo audio
    arecord -l | grep -E -q \
      "Webcam C920"
    if [ $? == 0 ]; then   ## Present
      C920Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "Webcam C920" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=30
    fi

    # Check for the presence of a C910 Webcam with stereo audio
    arecord -l | grep -E -q \
      "U0x46d0x821"
    if [ $? == 0 ]; then   ## Present
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x821" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=2
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=30
    fi

    # Check for the presence of a C525 or C170 with mono audio
    arecord -l | grep -E -q \
      "Webcam C525|Webcam C170"
    if [ $? == 0 ]; then   ## Present
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "Webcam C525|Webcam C170" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=24
    fi

    # Check for the presence of a C270 with mono audio
    C270Present=0
    arecord -l | grep -E -q \
      "U0x46d0x825"
    if [ $? == 0 ]; then   ## Present
      C270Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x825" \
        | head -c 6 | tail -c 1)"
      WC_AUDIO_CHANNELS=1
      WC_AUDIO_SAMPLE=48000
      WC_VIDEO_FPS=25
    fi

    printf "MIC = $MIC\n"
    printf "USBTV = $USBTV\n"
    printf "WCAM = $WCAM\n"

    # At least one card detected, so sort out what card parameters are used
    case "$AUDIO_PREF" in
      auto)                                                 # Auto selected
        case "$PIC_INPUT" in
          "DIGITAL")                                        # Using Pi Cam video
            if [ "$MIC" != "9" ]; then                      # Mic available
              AUDIO_CARD=1                                  # so use mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=44100
            elif [ "$USBTV" != "9" ] && [ "$MIC" == "9" ]; then # Mic not available, but EasyCap is
              AUDIO_CARD=1                                  # so use EasyCap audio
              AUDIO_CARD_NUMBER=$USBTV
              AUDIO_CHANNELS=2
              AUDIO_SAMPLE=48000
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
          "ANALOG")                                         # Using Easycap video
            if [ "$USBTV" != "9" ]; then                    # Easycap Audio available
              AUDIO_CARD=1                                  # so use EasyCap audio
              AUDIO_CARD_NUMBER=$USBTV
              AUDIO_CHANNELS=2
              AUDIO_SAMPLE=48000
            elif [ "$MIC" != "9" ] && [ "$USBTV" == "9" ]; then # EasyCap not available, but Mic is
              AUDIO_CARD=1                                  # so use Mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=44100
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
          "WEBCAM")                                         # Using Webcam video
            if [ "$WCAM" != "9" ]; then                     # Webcam Audio available
              AUDIO_CARD=1                                  # So use Webcam audio
              AUDIO_CARD_NUMBER=$WCAM
              AUDIO_CHANNELS=$WC_AUDIO_CHANNELS
              AUDIO_SAMPLE=$WC_AUDIO_SAMPLE
            elif [ "$MIC" != "9" ] && [ "$WCAM" == "9" ]; then # Webcam not available, but Mic is
              AUDIO_CARD=1                                  # so use Mic audio
              AUDIO_CARD_NUMBER=$MIC
              AUDIO_CHANNELS=1
              AUDIO_SAMPLE=44100
            else                                            # Neither available
              AUDIO_CARD=0                                  # So no audio
              AUDIO_CHANNELS=0
              AUDIO_SAMPLE=44100
            fi
          ;;
        esac
      ;;
      mic)                                                  # Mic selected
        if [ "$MIC" != "9" ]; then                          # Mic card detected
          AUDIO_CARD=1                                      # so use mic
          AUDIO_CARD_NUMBER=$MIC
          AUDIO_CHANNELS=1
          AUDIO_SAMPLE=44100
        else                                                # No mic card
          AUDIO_CARD=0                                      # So no audio
          AUDIO_CHANNELS=0
          AUDIO_SAMPLE=44100
        fi
      ;;
      video)                                                # EasyCap selected
        if [ "$USBTV" != "9" ]; then                        # EasyCap detected
          AUDIO_CARD=1                                      # So use EasyCap
          AUDIO_CARD_NUMBER=$USBTV
          AUDIO_CHANNELS=2
          AUDIO_SAMPLE=48000
        else                                                # No EasyCap
          AUDIO_CARD=0                                      # So no audio
          AUDIO_CHANNELS=0
          AUDIO_SAMPLE=44100
        fi
      ;;
      webcam)                                               # Webcam selected
        if [ "$WCAM" != "9" ]; then                         # Webcam detected
          AUDIO_CARD=1                                      # So use Webcam
          AUDIO_CARD_NUMBER=$WCAM
          AUDIO_CHANNELS=$WC_AUDIO_CHANNELS
          AUDIO_SAMPLE=$WC_AUDIO_SAMPLE
        else                                                # No Webcam Audio
          AUDIO_CARD=1                                      # so use mic
          AUDIO_CARD_NUMBER=$MIC
          AUDIO_CHANNELS=1
          AUDIO_SAMPLE=44100
        fi
      ;;
      bleeps)
        AUDIO_CARD=0
        AUDIO_CHANNELS=1
        AUDIO_SAMPLE=44100
      ;;
      no_audio)
        AUDIO_CARD=0
        AUDIO_CARD_NUMBER=9
        AUDIO_CHANNELS=0
        AUDIO_SAMPLE=44100
      ;;
      *)                                                    # Unidentified option
        AUDIO_CARD=0                                        # No Audio
        AUDIO_CARD_NUMBER=9
        AUDIO_CHANNELS=0
        AUDIO_SAMPLE=44100
      ;;
    esac
  fi

  printf "AUDIO_CARD = $AUDIO_CARD\n"
  printf "AUDIO_CARD_NUMBER = $AUDIO_CARD_NUMBER \n"
  printf "AUDIO_CHANNELS = $AUDIO_CHANNELS \n"
  printf "AUDIO_SAMPLE = $AUDIO_SAMPLE \n"
}

############ FUNCTION TO IDENTIFY VIDEO DEVICES #############################

detect_video()
{
  # List the video devices, select the 2 lines for any Webcam device, then
  # select the line with the device details and delete the leading tab
  # This selects any device with "Webcam" int its description
  VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/Webcam/,/dev/p' | grep 'dev' | tr -d '\t')"

  if [ "${#VID_WEBCAM}" -lt "10" ]; then # $VID_WEBCAM currently empty

    # List the video devices, select the 2 lines for a C270 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:0825/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  if [ "${#VID_WEBCAM}" -lt "10" ]; then # $VID_WEBCAM currently empty

    # List the video devices, select the 2 lines for a C910 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:0821/,/dev/p' | grep 'dev' | tr -d '\t')"
  fi

  printf "The first Webcam device string is $VID_WEBCAM\n"

  # List the video devices, select the 2 lines for any usb device, then
  # select the line with the device details and delete the leading tab
  VID_USB1="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t' | head -n1)"
  
  printf "The first USB device string is $VID_USB1\n"
 
  VID_USB2="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/usb/,/dev/p' | grep 'dev' | tr -d '\t' | tail -n1)"
  printf "The second USB device string is $VID_USB2\n"

  if [ "$VID_USB1" != "$VID_WEBCAM" ]; then
    VID_USB=$VID_USB1
  printf "The first test passed\n"
  fi

  if [ "$VID_USB2" != "$VID_WEBCAM" ]; then
    VID_USB=$VID_USB2
  printf "The second test passed\n"
  fi  
  printf "The final USB device string is $VID_USB\n"

  # List the video devices, select the 2 lines for any mmal device, then
  # select the line with the device details and delete the leading tab
  VID_PICAM="$(v4l2-ctl --list-devices 2> /dev/null | \
    sed -n '/mmal/,/dev/p' | grep 'dev' | tr -d '\t')"

  if [ "$VID_USB" == '' ]; then
    printf "VID_USB was not found, setting to /dev/video0\n"
    VID_USB="/dev/video0"
  fi
  if [ "$VID_PICAM" == '' ]; then
    printf "VID_PICAM was not found, setting to /dev/video0\n"
    VID_PICAM="/dev/video0"
  fi
  if [ "$VID_WEBCAM" == '' ]; then
    printf "VID_WEBCAM was not found, setting to /dev/video2\n"
    VID_WEBCAM="/dev/video2"
  fi

  printf "The PI-CAM device string is $VID_PICAM\n"
  printf "The USB device string is $VID_USB\n"
  printf "The Webcam device string is $VID_WEBCAM\n"
}

############ READ FROM rpidatvconfig.txt and Set PARAMETERS #######################

MODE_INPUT=ANALOGMPEG-2
CALL=Video
CHANNEL=$CALL
FREQ_OUTPUT=1255
BATC_OUTPUT=test
OUTPUT_BATC="-f flv rtmp://fms.batc.tv/live/$BATC_OUTPUT/$BATC_OUTPUT"

STREAM_URL=rtmp://rtmp.batc.org.uk/live
STREAM_KEY=callsign-keykey
OUTPUT_STREAM="-f flv $STREAM_URL/$STREAM_KEY"

MODE_OUTPUT=IQ
SYMBOLRATEK="10000"
GAIN=7
PIDVIDEO=256
PIDAUDIO=257
PIDPMT=4095
PIDSTART=256
SERVICEID=1
LOCATOR=IO72JB
PIN_I=12
PIN_Q=13

ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
ANALOGCAMINPUT=$(get_config_var analogcaminput $PCONFIGFILE)
ANALOGCAMSTANDARD=$(get_config_var analogcamstandard $PCONFIGFILE)

AUDIO_PREF=auto
CAPTIONON=$(get_config_var caption $PCONFIGFILE)
OPSTD=$(get_config_var outputstandard $PCONFIGFILE)
DISPLAY=$(get_config_var display $PCONFIGFILE)

BAND_LABEL=23_cm
NUMBERS=4444

OUTPUT_IP=""

let SYMBOLRATE=SYMBOLRATEK*1000
FEC=$(get_config_var fec $PCONFIGFILE)
let FECNUM=FEC
let FECDEN=FEC+1

# Look up the capture device names and parameters
detect_audio
detect_video
ANALOGCAMNAME=$VID_USB

MODE_INPUT="ANALOGMPEG-2"

#Adjust the PIDs for avc2ts modes

case "$MODE_INPUT" in
  "CAMH264" | "ANALOGCAM" | "WEBCAMH264" | "CARDH264" | "PATERNAUDIO" \
    | "CONTEST" | "DESKTOP" | "VNC" )
    let PIDPMT=$PIDVIDEO-1
  ;;
esac  


########### Set 480p Output Format if compatible and required ###############

let IMAGE_HEIGHT=576

if [ "$MODE_INPUT" == "CAMMPEG-2" ] || [ "$MODE_INPUT" == "ANALOGMPEG-2" ]; then
  # Set IMAGE_HEIGHT
  if [ "$OPSTD" == "480" ]; then
    let IMAGE_HEIGHT=480
  fi
fi 

######################### Pre-processing for each Output Mode ###############

case "$MODE_OUTPUT" in

  IQ)
    FREQUENCY_OUT=0
    OUTPUT=videots
  ;;


  BATC)
    # Set Output string "-f flv rtmp://fms.batc.tv/live/"$BATC_OUTPUT"/"$BATC_OUTPUT 
    OUTPUT=$OUTPUT_BATC
    # If CAMH264 is selected, temporarily select CAMMPEG-2
    if [ "$MODE_INPUT" == "CAMH264" ]; then
      MODE_INPUT="CAMMPEG-2"
    fi
    # If ANALOGCAM is selected, temporarily select ANALOGMPEG-2
    if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
      MODE_INPUT="ANALOGMPEG-2"
    fi
  ;;

  STREAMER)
    # Set Output string "-f flv "$STREAM_URL"/"$STREAM_KEY
    OUTPUT=$OUTPUT_STREAM
    # If CAMH264 is selected, temporarily select CAMMPEG-2
    if [ "$MODE_INPUT" == "CAMH264" ]; then
      MODE_INPUT="CAMMPEG-2"
    fi
    # If ANALOGCAM is selected, temporarily select ANALOGMPEG-2
    if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
      MODE_INPUT="ANALOGMPEG-2"
    fi
  ;;

  COMPVID)
    # Temporarily set the symbol rate to something reasonable
    SYMBOLRATEK=1000
    let SYMBOLRATE=SYMBOLRATEK*1000
    FREQUENCY_OUT=0
    OUTPUT="/dev/null"
    # Get the Mic and on-board sound card numbers
    MICCARD="$(cat /proc/asound/modules | grep 'usb_audio' | head -c 2 | tail -c 1)"
    RPICARD="$(cat /proc/asound/modules | grep 'bcm2835' | head -c 2 | tail -c 1)"
    # Pass the USB mic input directly to the RPi Audio Out
    arecord -D plughw:"$MICCARD",0 -f cd - | aplay -D plughw:"$RPICARD",0 - &
  ;;

esac

OUTPUT_QPSK="videots"
 MODE_DEBUG=quiet
# MODE_DEBUG=debug

# ************************ CALCULATE SYMBOL RATES ******************

# BITRATE TS THEORIC
let BITRATE_TS=SYMBOLRATE*2*188*FECNUM/204/FECDEN

# echo Theoretical Bitrate TS $BITRATE_TS


# Calculate the Video Bit Rate for Sound/no sound
if [ "$MODE_INPUT" == "CAMMPEG-2" ] || [ "$MODE_INPUT" == "ANALOGMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAMHDMPEG-2" ] || [ "$MODE_INPUT" == "CARDMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAM16MPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
  if [ "$AUDIO_CHANNELS" != 0 ]; then                 # Sound active
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-74000
  else
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-10000
  fi
else 
  let BITRATE_VIDEO=(BITRATE_TS*75)/100-10000
fi

let SYMBOLRATE_K=SYMBOLRATE/1000


# Increase video resolution for CAMHDMPEG-2 and CAM16MPEG-2
if [ "$MODE_INPUT" == "CAMHDMPEG-2" ] && [ "$SYMBOLRATE_K" -gt 999 ]; then
  VIDEO_WIDTH=1280
  VIDEO_HEIGHT=720
  VIDEO_FPS=15
elif [ "$MODE_INPUT" == "CAM16MPEG-2" ] && [ "$SYMBOLRATE_K" -gt 999 ]; then
  VIDEO_WIDTH=1024
  VIDEO_HEIGHT=576
  VIDEO_FPS=15
else
  # Reduce video resolution at low bit rates
  if [ "$BITRATE_VIDEO" -lt 150000 ]; then
    VIDEO_WIDTH=160
    VIDEO_HEIGHT=140
  else
    if [ "$BITRATE_VIDEO" -lt 300000 ]; then
      VIDEO_WIDTH=352
      VIDEO_HEIGHT=288
    else
      VIDEO_WIDTH=720
      VIDEO_HEIGHT=$IMAGE_HEIGHT
    fi
  fi

  # Reduce frame rate at low bit rates
  if [ "$BITRATE_VIDEO" -lt 300000 ]; then
    VIDEO_FPS=15
  else
    # Switch to 30 fps if required
    if [ "$IMAGE_HEIGHT" == "480" ]; then
      VIDEO_FPS=30
    else
      VIDEO_FPS=25
    fi
  fi
fi

# Clean up before starting fifos
sudo rm videoes
sudo rm videots
sudo rm netfifo
mkfifo videoes
mkfifo videots
mkfifo netfifo

echo "************************************"
echo Bitrate TS $BITRATE_TS
echo Bitrate Video $BITRATE_VIDEO
echo Size $VIDEO_WIDTH x $VIDEO_HEIGHT at $VIDEO_FPS fps
echo "************************************"
echo "ModeINPUT="$MODE_INPUT

OUTPUT_FILE="-o videots"

case "$MODE_INPUT" in

  #============================================ ANALOG and WEBCAM H264 =============================================================
  "ANALOGCAM" | "WEBCAMH264")

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    if [ "$MODE_INPUT" == "ANALOGCAM" ]; then
      # Set the EasyCap input and video standard
      if [ "$ANALOGCAMINPUT" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-input="$ANALOGCAMINPUT
      fi
      if [ "$ANALOGCAMSTANDARD" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-standard="$ANALOGCAMSTANDARD
      fi
    else
      # Webcam in use
      # If a C920 put it in the right mode
      if [ $C920Present == 1 ]; then
         v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=600,pixelformat=0
      fi
      ANALOGCAMNAME=$VID_WEBCAM
      #ANALOGCAMNAME="/dev/video2"
    fi

    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi    

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "BATC")
        : # Do nothing.  All done below
      ;;
      "STREAMER")
        : # Do nothing.  All done below
      ;;
      "IP")
        OUTPUT_FILE=""
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          | buffer \
          | sudo /home/pi/limetool/limetx -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT\
      -f $VIDEO_FPS -i 100 $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CHANNEL $OUTPUT_IP &
  ;;


  #============================================ ANALOG AND WEBCAM MPEG-2 INPUT MODES ======================================================
  #============================================ INCLUDES STREAMING OUTPUTS (H264)    ======================================================
  "ANALOGMPEG-2" | "ANALOG16MPEG-2" | "WEBCAMMPEG-2" | "WEBCAM16MPEG-2" | "WEBCAMHDMPEG-2")

    # Set the default sound/video lipsync
    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    # Sort out image size, video delay and scaling
    SCALE=""
    case "$MODE_INPUT" in
    ANALOG16MPEG-2)
      SCALE="scale=512:288,"
    ;;
    WEBCAMMPEG-2)
      if [ $C920Present == 1 ]; then
         v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=0
      fi
      if [ $C270Present == 1 ]; then
        ITS_OFFSET="-00:00:1.8"
      fi
      VIDEO_WIDTH="640"
      VIDEO_HEIGHT="480"
      VIDEO_FPS=$WC_VIDEO_FPS
    ;;
    WEBCAM16MPEG-2)
      if [ $C920Present == 1 ]; then
         v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=0
      fi
      if [ $C270Present == 1 ]; then
        ITS_OFFSET="-00:00:1.8"
      fi
      VIDEO_WIDTH="1280"
      VIDEO_HEIGHT="720"
      VIDEO_FPS=$WC_VIDEO_FPS
      SCALE="scale=1024:576,"
    ;;
    WEBCAMHDMPEG-2)
      if [ $C920Present == 1 ]; then
         v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=0
      fi
      if [ $C270Present == 1 ]; then
        ITS_OFFSET="-00:00:2.0"
      fi
      VIDEO_WIDTH="1280"
      VIDEO_HEIGHT="720"
      VIDEO_FPS=$WC_VIDEO_FPS
    ;;
    esac

    # Turn off the viewfinder (which would show Pi Cam)
    #v4l2-ctl --overlay=0

    # Set up the command for the MPEG-2 Callsign caption
    # Put caption at the bottom of the screen with locator for US users
    if [ "$IMAGE_HEIGHT" == "480" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf: \
          text=\'$CALL-$LOCATOR\': fontcolor=white: fontsize=36: box=1: boxcolor=black@0.5: \
          boxborderw=5: x=(w/2-(text_w/2)): y=(h-text_h-40), "
      else
        CAPTION=""
      fi
    else
      if [ "$CAPTIONON" == "on" ]; then
        CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf: \
          text=\'$CALL\': fontcolor=white: fontsize=36: box=1: boxcolor=black@0.5: \
          boxborderw=5: x=w/10: y=(h/4-text_h)/2, "
      else
        CAPTION=""    
      fi
    fi

    # Select USB Video Dongle or Webcam and set USB Video modes
    if [ "$MODE_INPUT" == "ANALOGMPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
      # Set the EasyCap input 
      if [ "$ANALOGCAMINPUT" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-input="$ANALOGCAMINPUT
      fi
      # Set PAL or NTSC standard
      if [ "$ANALOGCAMSTANDARD" != "-" ]; then
        v4l2-ctl -d $ANALOGCAMNAME "--set-standard="$ANALOGCAMSTANDARD
      fi
    else
      VID_USB=$VID_WEBCAM
    fi

        # ******************************* MPEG-2 ANALOG VIDEO FOR VIDEO MONITOR *******************************

        # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
        # PMT, Vid and Audio PIDs can all be set.

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512 \
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -i $VID_USB -fflags nobuffer \
          \
          -c:v mpeg2video -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15"  \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
          -f mpegts -blocksize 1880 \
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" \
          -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
          -muxrate $BITRATE_TS -y $OUTPUT &

# Works optimum at video bitrate 12021587
# muxrate 16127450

          # Make sure that the screen background is all black
          sudo killall fbi >/dev/null 2>/dev/null
          sudo fbi -T 1 -noverbose -a $PATHSCRIPT"/images/Blank_Black.png"
          (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work

          sudo rm fifo.264
          mkfifo fifo.264

          # read videots and output video es
          /home/pi/rpidatv/bin/ts2es -video videots fifo.264 &

          # Play the es from fifo.264 in MPEG-2 player.
          /home/pi/rpidatv/bin/hello_video2.bin fifo.264


  ;;
esac

# ============================================ END =============================================================

# flow exits from a.sh leaving ffmpeg or avc2ts and rpidatv running
# these processes are killed by menu.sh, rpidatvgui or b.sh on selection of "stop transmit"
