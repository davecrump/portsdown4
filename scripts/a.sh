#! /bin/bash
# set -x #Uncomment for testing

# Version 201811170

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
sudo killall h264yuv >/dev/null 2>/dev/null
sudo killall -9 avc2ts >/dev/null 2>/dev/null
sudo killall -9 avc2ts.old >/dev/null 2>/dev/null
#sudo killall express_server >/dev/null 2>/dev/null
# Leave Express Server running
sudo killall tcanim >/dev/null 2>/dev/null
sudo killall tcanim1v16 >/dev/null 2>/dev/null
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
  printf "Video Input is $PIC_INPUT \n"
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

    # Check for the presence of a C310 with mono audio
    C310Present=0
    arecord -l | grep -E -q \
      "U0x46d0x81b"
    if [ $? == 0 ]; then   ## Present
      C310Present=1
      # Look for the video dongle, select the line and take
      # the 6th character.  Max card number = 8 !!
      WCAM="$(arecord -l | grep -E \
        "U0x46d0x81b" \
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
  # This selects any device with "Webcam" in its description
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

  if [ "${#VID_WEBCAM}" -lt "10" ]; then # $VID_WEBCAM currently empty

    # List the video devices, select the 2 lines for a C310 device, then
    # select the line with the device details and delete the leading tab
    VID_WEBCAM="$(v4l2-ctl --list-devices 2> /dev/null | \
      sed -n '/046d:081b/,/dev/p' | grep 'dev' | tr -d '\t')"
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
  printf "The first test passed"
  fi

  if [ "$VID_USB2" != "$VID_WEBCAM" ]; then
    VID_USB=$VID_USB2
  printf "The second test passed"
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

MODE_INPUT=$(get_config_var modeinput $PCONFIGFILE)
TSVIDEOFILE=$(get_config_var tsvideofile $PCONFIGFILE)
PATERNFILE=$(get_config_var paternfile $PCONFIGFILE)
UDPOUTADDR=$(get_config_var udpoutaddr $PCONFIGFILE)
CALL=$(get_config_var call $PCONFIGFILE)
CHANNEL=$CALL
FREQ_OUTPUT=$(get_config_var freqoutput $PCONFIGFILE)

STREAM_URL=$(get_config_var streamurl $PCONFIGFILE)
STREAM_KEY=$(get_config_var streamkey $PCONFIGFILE)
OUTPUT_STREAM="-f flv $STREAM_URL/$STREAM_KEY"

MODE_OUTPUT=$(get_config_var modeoutput $PCONFIGFILE)
SYMBOLRATEK=$(get_config_var symbolrate $PCONFIGFILE)
GAIN=$(get_config_var rfpower $PCONFIGFILE)
PIDVIDEO=$(get_config_var pidvideo $PCONFIGFILE)
PIDAUDIO=$(get_config_var pidaudio $PCONFIGFILE)
PIDPMT=$(get_config_var pidpmt $PCONFIGFILE)
PIDSTART=$(get_config_var pidstart $PCONFIGFILE)
SERVICEID=$(get_config_var serviceid $PCONFIGFILE)
LOCATOR=$(get_config_var locator $PCONFIGFILE)
PIN_I=$(get_config_var gpio_i $PCONFIGFILE)
PIN_Q=$(get_config_var gpio_q $PCONFIGFILE)

ANALOGCAMNAME=$(get_config_var analogcamname $PCONFIGFILE)
ANALOGCAMINPUT=$(get_config_var analogcaminput $PCONFIGFILE)
ANALOGCAMSTANDARD=$(get_config_var analogcamstandard $PCONFIGFILE)
VNCADDR=$(get_config_var vncaddr $PCONFIGFILE)

AUDIO_PREF=$(get_config_var audio $PCONFIGFILE)
CAPTIONON=$(get_config_var caption $PCONFIGFILE)
OPSTD=$(get_config_var outputstandard $PCONFIGFILE)
DISPLAY=$(get_config_var display $PCONFIGFILE)

BAND_LABEL=$(get_config_var labelofband $PCONFIGFILE)
NUMBERS=$(get_config_var numbers $PCONFIGFILE)

OUTPUT_IP=""

let SYMBOLRATE=SYMBOLRATEK*1000

# Set the FEC
FEC=$(get_config_var fec $PCONFIGFILE)
case "$FEC" in
  "1" | "2" | "3" | "5" | "7" )
    let FECNUM=FEC
    let FECDEN=FEC+1
  ;;
  "14" )
    FECNUM="1"
    FECDEN="4"
  ;;
  "13" )
    FECNUM="1"
    FECDEN="3"
  ;;
  "12" )
    FECNUM="1"
    FECDEN="2"
  ;;
  "35" )
    FECNUM="3"
    FECDEN="5"
  ;;
  "23" )
    FECNUM="2"
    FECDEN="3"
  ;;
  "34" )
    FECNUM="3"
    FECDEN="4"
  ;;
  "56" )
    FECNUM="5"
    FECDEN="6"
  ;;
  "89" )
    FECNUM="8"
    FECDEN="9"
  ;;
  "91" )
    FECNUM="9"
    FECDEN="10"
  ;;
esac

# Calculate Bits per symbol
MODULATION=$(get_config_var modulation $PCONFIGFILE)
case "$MODULATION" in
  "DVB-S" )
    BITSPERSYMBOL="2"
    MODTYPE="DVBS"
    CONSTLN="QPSK"
  ;;
  "S2QPSK" )
    BITSPERSYMBOL="2"
    MODTYPE="DVBS2"
    CONSTLN="QPSK"
  ;;
  "8PSK" )
    BITSPERSYMBOL="3"
    MODTYPE="DVBS2"
    CONSTLN="8PSK"
  ;;
  "16APSK" )
    BITSPERSYMBOL="4"
    MODTYPE="DVBS2"
    CONSTLN="16APSK"
  ;;
  "32APSK" )
    BITSPERSYMBOL="5"
    MODTYPE="DVBS2"
    CONSTLN="32APSK"
  ;;
esac

# Look up the capture device names and parameters
detect_audio
detect_video
ANALOGCAMNAME=$VID_USB

#Adjust the PIDs for avc2ts modes

case "$MODE_INPUT" in
  "CAMH264" | "ANALOGCAM" | "WEBCAMH264" | "CARDH264" | "PATERNAUDIO" \
    | "CONTEST" | "DESKTOP" | "VNC" )
    let PIDPMT=$PIDVIDEO-1
  ;;
esac

########### Redirect old BATC Streamer mode to Streamer mode ###############

if [ "$MODE_OUTPUT" == "BATC" ]; then
    MODE_OUTPUT="STREAMER"
fi

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
    MODE=IQ
    $PATHSCRIPT"/ctlfilter.sh"
    $PATHSCRIPT"/ctlvco.sh"
    #GAIN=0
  ;;

  QPSKRF)
    FREQUENCY_OUT=$FREQ_OUTPUT
    OUTPUT=videots
    MODE=RF
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
    # If CARDH264 is selected, temporarily select CARDMPEG-2
    if [ "$MODE_INPUT" == "CARDH264" ]; then
      MODE_INPUT="CARDMPEG-2"
    fi

  ;;

  DIGITHIN)
    FREQUENCY_OUT=0
    OUTPUT=videots
    DIGITHIN_MODE=1
    MODE=DIGITHIN
    $PATHSCRIPT"/ctlfilter.sh"
    $PATHSCRIPT"/ctlvco.sh"
    #GAIN=0
  ;;

  DTX1)
    MODE=PARALLEL
    FREQUENCY_OUT=2
    OUTPUT=videots
    DIGITHIN_MODE=0
    #GAIN=0
  ;;

  DATVEXPRESS)
    if pgrep -x "express_server" > /dev/null
    then
      # Express already running
      :
    else
      # Stopped, so make sure the control file is not locked and start it
      # From its own folder otherwise it doesnt read the config file
      sudo rm /tmp/expctrl >/dev/null 2>/dev/null
      cd /home/pi/express_server
      sudo nice -n -40 /home/pi/express_server/express_server  >/dev/null 2>/dev/null &
      cd /home/pi
      sleep 5
    fi
    # Set output for ffmpeg (avc2ts uses netcat to pipe output from videots)
     OUTPUT="udp://127.0.0.1:1314?pkt_size=1316&buffer_size=1316"
    FREQUENCY_OUT=0  # Not used in this mode?
    # Calculate output freq in Hz using floating point
    FREQ_OUTPUTHZ=`echo - | awk '{print '$FREQ_OUTPUT' * 1000000}'`
    echo "set freq "$FREQ_OUTPUTHZ >> /tmp/expctrl
    echo "set fec "$FECNUM"/"$FECDEN >> /tmp/expctrl
    echo "set srate "$SYMBOLRATE >> /tmp/expctrl
    # Set the ports
    $PATHSCRIPT"/ctlfilter.sh"

    # Set the output level
    GAIN=$(get_config_var explevel $PCONFIGFILE);

    # Set Gain
    echo "set level "$GAIN >> /tmp/expctrl

    # Make sure that carrier mode is off
    echo "set car off" >> /tmp/expctrl
  ;;

  IP)
    FREQUENCY_OUT=0
    # Set Output parameters for H264 (avc2ts) modes:
    OUTPUT_IP="-n"$UDPOUTADDR":10000"
    # Set Output parameters for MPEG-2 (ffmpeg) modes:
    OUTPUT="udp://"$UDPOUTADDR":10000?pkt_size=1316&buffer_size=1316" # Try this
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

  LIMEMINI)
    OUTPUT=videots
    # CALCULATE FREQUENCY IN K
    FREQ_OUTPUTK=`echo - | awk '{print '$FREQ_OUTPUT' * 1000}'`
    LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
    $PATHSCRIPT"/ctlfilter.sh"
  ;;

  LIMEUSB)
    OUTPUT=videots
    # CALCULATE FREQUENCY IN K
    FREQ_OUTPUTK=`echo - | awk '{print '$FREQ_OUTPUT' * 1000}'`
    LIME_GAIN=$(get_config_var limegain $PCONFIGFILE)
    $PATHSCRIPT"/ctlfilter.sh"
  ;;

esac

OUTPUT_QPSK="videots"
# MODE_DEBUG=quiet
 MODE_DEBUG=debug

# ************************ CALCULATE BITRATES ******************

# Maximum BITRATE:
let BITRATE_TS=SYMBOLRATE*BITSPERSYMBOL*188*FECNUM/204/FECDEN

echo Theoretical Bitrate TS $BITRATE_TS

# Calculate the Video Bit Rate for MPEG-2 Sound/no sound
if [ "$MODE_INPUT" == "CAMMPEG-2" ] || [ "$MODE_INPUT" == "ANALOGMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAMHDMPEG-2" ] || [ "$MODE_INPUT" == "CARDMPEG-2" ] \
  || [ "$MODE_INPUT" == "CAM16MPEG-2" ] || [ "$MODE_INPUT" == "ANALOG16MPEG-2" ]; then
  if [ "$AUDIO_CHANNELS" != 0 ]; then                 # Sound active
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-74000
  else
    let BITRATE_VIDEO=(BITRATE_TS*75)/100-10000
  fi
else # h264
  let BITRATE_VIDEO=(BITRATE_TS*75)/100-10000
fi

let SYMBOLRATE_K=SYMBOLRATE/1000

# Increase video resolution for CAMHDMPEG-2 and CAM16MPEG-2
if [ "$MODE_INPUT" == "CAMHDMPEG-2" ] && [ "$BITRATE_VIDEO" -gt 500000 ]; then
  VIDEO_WIDTH=1280
  VIDEO_HEIGHT=720
  VIDEO_FPS=15
elif [ "$MODE_INPUT" == "CAM16MPEG-2" ] && [ "$BITRATE_VIDEO" -gt 500000 ]; then
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

# Set h264 aac audio bitrate for avc2ts
BITRATE_AUDIO=24000

# Set IDRPeriod for avc2ts
# Default is 100, ie one every 4 sec at 25 fps
IDRPERIOD=100

# Clean up before starting fifos
sudo rm videoes
sudo rm videots
sudo rm netfifo
sudo rm audioin.wav

mkfifo videoes
mkfifo videots
mkfifo netfifo
mkfifo audioin.wav

echo "************************************"
echo Bitrate TS $BITRATE_TS
echo Bitrate Video $BITRATE_VIDEO
echo Size $VIDEO_WIDTH x $VIDEO_HEIGHT at $VIDEO_FPS fps
echo "************************************"
echo "ModeINPUT="$MODE_INPUT

OUTPUT_FILE="-o videots"

case "$MODE_INPUT" in

  #============================================ H264 PI CAM INPUT MODE =========================================================
  "CAMH264")

    # Check PiCam is present to prevent kernel panic    
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -ne 0 ]; then
      exit
    fi

    # Free up Pi Camera for direct OMX Coding by removing driver
    sudo modprobe -r bcm2835_v4l2

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "STREAMER")
        : # Do nothing - this option should never be called (see MPEG-2)
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
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "COMPVID")
        OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************
      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 0 -e $ANALOGCAMNAME -p $PIDPMT -s $CHANNEL $OUTPUT_IP > /dev/null &
    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************
      arecord -f S16_LE -r 48000 -c 2 -B 100 -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

      let BITRATE_VIDEO=$BITRATE_VIDEO-30000  # Make room for audio

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 0 -e $ANALOGCAMNAME -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
        -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
    fi
  ;;

  #============================================ MPEG-2 PI CAM INPUT MODE =============================================================
  "CAMMPEG-2"|"CAMHDMPEG-2"|"CAM16MPEG-2")

# Set up the command for the MPEG-2 Callsign caption
# Note that spaces are not allowed in the CAPTION string below!

# Put caption at the bottom of the screen with locator for US users
if [ "$IMAGE_HEIGHT" == "480" ]; then
  if [ "$CAPTIONON" == "on" ]; then
    CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf:\
text=$CALL-$LOCATOR:fontcolor=white:fontsize=36:box=1:boxcolor=black@0.5:boxborderw=5:\
x=(w/2-(text_w/2)):y=(h-text_h-40)"
    VF="-vf "
  else
    CAPTION=""
    VF=""    
  fi
else
  if [ "$CAPTIONON" == "on" ]; then
    CAPTION="drawtext=fontfile=/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf:\
text=$CALL:fontcolor=white:fontsize=36:box=1:boxcolor=black@0.5:boxborderw=5:\
x=w/10:y=(h/4-text_h)/2"
    VF="-vf "
  else
    CAPTION=""
    VF=""    
  fi
fi

    # Size the viewfinder
    if [ "$DISPLAY" == "Element14_7" ]; then
      v4l2-ctl --set-fmt-overlay=left=0,top=0,width=736,height=416 # For 800x480 framebuffer
    else
      v4l2-ctl --set-fmt-overlay=left=0,top=0,width=656,height=512 # For 720x576 framebuffer
    fi
    v4l2-ctl -p $VIDEO_FPS

    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "STREAMER")
        ITS_OFFSET="-00:00:00"
      ;;
      "IP")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "COMPVID")
        : # Do nothing
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    case "$MODE_OUTPUT" in
      "STREAMER")
        # No code for beeps here
        sudo modprobe bcm2835_v4l2
        if [ "$AUDIO_CARD" == 0 ]; then
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
            -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i /dev/video0 \
            $VF $CAPTION -framerate 25 \
            -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 576k \
            -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        else
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -f v4l2 -input_format h264 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i /dev/video0 -thread_queue_size 2048 \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            $VF $CAPTION -framerate 25 \
            -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
            -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi
      ;;
      *)
        if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

          # ******************************* MPEG-2 VIDEO WITH NO AUDIO ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i /dev/video0 -fflags nobuffer \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 \
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

          # ******************************* MPEG-2 VIDEO WITH BEEP ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i /dev/video0 -fflags nobuffer \
            \
            -f lavfi -ac 1 \
            -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        else

          # ******************************* MPEG-2 VIDEO WITH AUDIO ************************************

          # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
          # PMT, Vid and Audio PIDs can all be set. 

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
            -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
            -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
            -i /dev/video0 -fflags nobuffer \
            \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            $VF $CAPTION -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &
        fi
      ;;
    esac
  ;;

#============================================ H264 TCANIM =============================================================

  "PATERNAUDIO")

    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi    

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "IP")
        OUTPUT_FILE=""
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
      ;;
      "COMPVID")
        OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo  $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Stop fbcp, becuase it conficts with avc2ts desktop
    killall fbcp

    # Start the animated test card
    $PATHRPI"/tcanim1v16" $PATERNFILE"/*10" "48" "72" "CQ" "CQ CQ CQ DE "$CALL" IN $LOCATOR - DATV $SYMBOLRATEK KS FEC "$FECNUM"/"$FECDEN &

    # Generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 TCANIM, NO AUDIO ************************************

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 3 -p $PIDPMT -s $CHANNEL $OUTPUT_IP > /dev/null &
    else
      # ******************************* H264 TCANIM WITH AUDIO ************************************

      arecord -f S16_LE -r 48000 -c 2 -B 100 -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

      let BITRATE_VIDEO=$BITRATE_VIDEO-30000  # Make room for audio

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 3 -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
        -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
    fi
  ;;

#============================================ VNC =============================================================

  "VNC")
    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi    

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "IP")
        OUTPUT_FILE=""
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "COMPVID")
        OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &;;
      esac

    # Now generate the stream
    #$PATHRPI"/avc2ts.old" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
    #  -f $VIDEO_FPS -i 100 $OUTPUT_FILE -t 4 -e $VNCADDR -p $PIDPMT -s $CHANNEL $OUTPUT_IP &

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************
      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 4 -e $VNCADDR -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
        > /dev/null &
    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************
      arecord -f S16_LE -r 48000 -c 2 -B 100 -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

      let BITRATE_VIDEO=$BITRATE_VIDEO-30000  # Make room for audio

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 4 -e $VNCADDR -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
        -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
    fi

  ;;

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
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
      > /dev/null &
    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************
      arecord -f S16_LE -r 48000 -c 2 -B 100 -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

      let BITRATE_VIDEO=$BITRATE_VIDEO-30000  # Make room for audio

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 2 -e $ANALOGCAMNAME -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
      -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
    fi
  ;;

#============================================ DESKTOP MODES H264 =============================================================

  "DESKTOP" | "CONTEST" | "CARDH264")

    if [ "$MODE_INPUT" == "CONTEST" ]; then
      # Delete the old numbers image
      rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null

      # Create the numbers image in the tempfs folder
      convert -size 720x576 xc:white \
        -gravity North -pointsize 125 -annotate 0 "$CALL" \
        -gravity Center -pointsize 200 -annotate 0 "$NUMBERS" \
        -gravity South -pointsize 75 -annotate 0 "$LOCATOR   ""$BAND_LABEL" \
        /home/pi/tmp/contest.jpg

      # Modify size to fill 7 inch screen if required
      if [ "$DISPLAY" == "Element14_7" ]; then
        convert /home/pi/tmp/contest.jpg -resize '800x480!' /home/pi/tmp/contest.jpg
      fi

      # Display the numbers on the desktop
      sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    elif [ "$MODE_INPUT" == "CARDH264" ]; then
      rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
      rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      if [ "$CAPTIONON" == "on" ]; then
        convert -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png -geometry +0+475 -composite /home/pi/tmp/tcf2.jpg
      else
        cp /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      fi

      # Modify size to fill 7 inch screen if required
      if [ "$DISPLAY" == "Element14_7" ]; then
        convert /home/pi/tmp/tcf2.jpg -resize '800x480!' /home/pi/tmp/tcf2.jpg
      fi

      sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    fi

    # If PiCam is present unload driver   
    vcgencmd get_camera | grep 'detected=1' >/dev/null 2>/dev/null
    RESULT="$?"
    if [ "$RESULT" -eq 0 ]; then
      sudo modprobe -r bcm2835_v4l2
    fi    

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "IP")
        OUTPUT_FILE=""
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < videots &
      ;;
      "COMPVID")
        OUTPUT_FILE="/dev/null" #Send avc2ts output to /dev/null
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo nice -n -30 $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;

    esac

    # Pause to allow test card to be displayed
    sleep 1

    #  Before fbcp is stopped because it conflicts with avc2ts
    killall fbcp

    # Now generate the stream

    if [ "$AUDIO_CARD" == 0 ]; then
      # ******************************* H264 VIDEO, NO AUDIO ************************************
      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 3 -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
        > /dev/null  &
    else
      # ******************************* H264 VIDEO WITH AUDIO ************************************
      arecord -f S16_LE -r 48000 -c 2 -B 100 -D plughw:$AUDIO_CARD_NUMBER,0 > audioin.wav &

      let BITRATE_VIDEO=$BITRATE_VIDEO-30000  # Make room for audio

      $PATHRPI"/avc2ts" -b $BITRATE_VIDEO -m $BITRATE_TS -x $VIDEO_WIDTH -y $VIDEO_HEIGHT \
        -f $VIDEO_FPS -i $IDRPERIOD $OUTPUT_FILE -t 3 -p $PIDPMT -s $CHANNEL $OUTPUT_IP \
      -a audioin.wav -z $BITRATE_AUDIO > /dev/null &
    fi
  ;;

# *********************************** TRANSPORT STREAM INPUT THROUGH IP ******************************************

  "IPTSIN")

    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 nc -u -4 127.0.0.1 1314 < videots &
      ;;
      "COMPVID")
        : # Do nothing.  Mode does not work yet
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream

    PORT=10000
    netcat -u -4 -l $PORT > videots &
  ;;

  # *********************************** TRANSPORT STREAM INPUT FILE ******************************************

  "FILETS")
    # Set up means to transport of stream out of unit
    case "$MODE_OUTPUT" in
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        sudo nice -n -30 netcat -u -4 127.0.0.1 1314 < $TSVIDEOFILE &
        #sudo nice -n -30 cat $TSVIDEOFILE | sudo nice -n -30 netcat -u -4 127.0.0.1 1314 & 
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i $TSVIDEOFILE -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        sudo $PATHRPI"/rpidatv" -i $TSVIDEOFILE -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -l -x $PIN_I -y $PIN_Q &;;
    esac
  ;;

  # *********************************** CARRIER  ******************************************

  "CARRIER")
    case "$MODE_OUTPUT" in
      "DATVEXPRESS")
        echo "set car on" >> /tmp/expctrl
        echo "set ptt tx" >> /tmp/expctrl
      ;;
      *)
        # sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c "carrier" -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &

        # Temporary fix for swapped carrier and test modes:
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c "tesmode" -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac
  ;;

  # *********************************** TESTMODE  ******************************************
  "TESTMODE")
    # sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c "tesmode" -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &

    # Temporary fix for swapped carrier and test modes:
    sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c "carrier" -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
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
      if [ $C310Present == 1 ]; then
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
      if [ $C310Present == 1 ]; then
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
      if [ $C310Present == 1 ]; then
        ITS_OFFSET="-00:00:2.0"
      fi
      VIDEO_WIDTH="1280"
      VIDEO_HEIGHT="720"
      VIDEO_FPS=$WC_VIDEO_FPS
    ;;
    esac

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

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

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "IP" | "STREAMER" | "COMPVID")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream

    case "$MODE_OUTPUT" in
      "STREAMER")
        if [ "$AUDIO_CARD" == "0" ]; then
          # No audio
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048\
            -f v4l2 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_USB \
            -framerate 25 -c:v h264_omx -b:v 576k \
            -vf "$CAPTION""$SCALE"yadif=0:1:0 -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
       else
          # With audio
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048\
            -f v4l2 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $VID_USB \
            -f alsa -thread_queue_size 2048 -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            -framerate 25 -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k \
            -vf "$CAPTION""$SCALE"yadif=0:1:0 -g 25 \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi
      ;;

      *) # Transmitting modes

      if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

        # ******************************* MPEG-2 ANALOG VIDEO WITH NO AUDIO ************************************

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512 \
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
          -i $VID_USB -fflags nobuffer \
          \
          -c:v mpeg2video  -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
          -f mpegts -blocksize 1880 \
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
          -muxrate $BITRATE_TS -y $OUTPUT &

      elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

        # ******************************* MPEG-2 ANALOG VIDEO WITH BEEP ************************************

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
          -i $VID_USB -fflags nobuffer \
          \
          -f lavfi -ac 1 \
          -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
          \
          -c:v mpeg2video -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
          -f mpegts -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
          -muxrate $BITRATE_TS -y $OUTPUT &

      else

        # ******************************* MPEG-2 ANALOG VIDEO WITH AUDIO ************************************

        # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
        # PMT, Vid and Audio PIDs can all be set.

        sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET"\
          -analyzeduration 0 -probesize 2048  -fpsprobesize 0 -thread_queue_size 512\
          -f v4l2 -framerate $VIDEO_FPS -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT"\
          -i $VID_USB -fflags nobuffer \
          \
          -thread_queue_size 512 \
          -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
          -i hw:$AUDIO_CARD_NUMBER,0 \
          \
          -c:v mpeg2video -vf "$CAPTION""$SCALE""format=yuva420p,hqdn3d=15" \
          -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
          -f mpegts -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
          -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
          -mpegts_service_id $SERVICEID \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
          -muxrate $BITRATE_TS -y $OUTPUT &

      fi
    ;;
    esac
  ;;
  #============================================ MPEG-2 STATIC TEST CARD MODE =============================================================
  "CARDMPEG-2" | "CONTESTMPEG-2" | "CARD16MPEG-2"| "CARDHDMPEG-2"  )

    if [ "$MODE_INPUT" == "CONTESTMPEG-2" ]; then
      # Delete the old numbers image
      rm /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null

      # Create the numbers image in the tempfs folder
      convert -size 720x576 xc:white \
        -gravity North -pointsize 125 -annotate 0 "$CALL" \
        -gravity Center -pointsize 200 -annotate 0 "$NUMBERS" \
        -gravity South -pointsize 75 -annotate 0 "$LOCATOR   ""$BAND_LABEL" \
        /home/pi/tmp/contest.jpg
        IMAGEFILE="/home/pi/tmp/contest.jpg"

      # Display the numbers on the desktop
      sudo fbi -T 1 -noverbose -a /home/pi/tmp/contest.jpg >/dev/null 2>/dev/null
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    
    elif [ "$MODE_INPUT" == "CARDMPEG-2" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
        convert -size 720x80 xc:transparent -fill white -gravity Center -pointsize 40 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcf.jpg /home/pi/tmp/caption.png -geometry +0+475 -composite /home/pi/tmp/tcf2.jpg
        IMAGEFILE="/home/pi/tmp/tcf2.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcf2.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcf.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcf.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    elif [ "$MODE_INPUT" == "CARD16MPEG-2" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
        convert -size 1024x80 xc:transparent -fill white -gravity Center -pointsize 50 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcfw16.jpg /home/pi/tmp/caption.png -geometry +0+478 -composite /home/pi/tmp/tcfw162.jpg
        IMAGEFILE="/home/pi/tmp/tcfw162.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw162.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcfw16.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcfw16.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    elif [ "$MODE_INPUT" == "CARDHDMPEG-2" ]; then
      if [ "$CAPTIONON" == "on" ]; then
        rm /home/pi/tmp/caption.png >/dev/null 2>/dev/null
        rm /home/pi/tmp/tcfw2.jpg >/dev/null 2>/dev/null
        convert -size 1280x80 xc:transparent -fill white -gravity Center -pointsize 65 -annotate 0 $CALL /home/pi/tmp/caption.png
        convert /home/pi/rpidatv/scripts/images/tcfw.jpg /home/pi/tmp/caption.png -geometry +0+608 -composite /home/pi/tmp/tcfw2.jpg
        IMAGEFILE="/home/pi/tmp/tcfw2.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/tmp/tcfw2.jpg >/dev/null 2>/dev/null
      else
        IMAGEFILE="/home/pi/rpidatv/scripts/images/tcfw.jpg"
        sudo fbi -T 1 -noverbose -a /home/pi/rpidatv/scripts/images/tcfw.jpg >/dev/null 2>/dev/null
      fi
      (sleep 1; sudo killall -9 fbi >/dev/null 2>/dev/null) &  ## kill fbi once it has done its work
    fi

  # Turn the viewfinder off
  v4l2-ctl --overlay=0

    # If sound arrives first, decrease the numeric number to delay it
    # "-00:00:0.?" works well at SR1000 on IQ mode
    # "-00:00:0.2" works well at SR2000 on IQ mode
    ITS_OFFSET="-00:00:0.2"

    # Set up the means to transport the stream out of the unit
    case "$MODE_OUTPUT" in
      "STREAMER")
        ITS_OFFSET="-00:00:00"
      ;;
      "IP")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "COMPVID")
        : # Do nothing
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now generate the stream
    case "$MODE_OUTPUT" in
      "STREAMER")
        # No code for beeps here
        if [ "$AUDIO_CARD" == 0 ]; then
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -thread_queue_size 2048 \
            -f image2 -loop 1 \
            -i $IMAGEFILE \
            -framerate 25 -video_size 720x576 -c:v h264_omx -b:v 576k \
            $VF $CAPTION \
            \
            -f flv $STREAM_URL/$STREAM_KEY &
        else
          $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -f image2 -loop 1 \
            -i $IMAGEFILE \
            \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            -framerate 25 -c:v h264_omx -b:v 512k \
            -ar 22050 -ac $AUDIO_CHANNELS -ab 64k            \
            -f flv $STREAM_URL/$STREAM_KEY &
        fi
      ;;
      *)
        if [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "0" ]; then

          # ******************************* MPEG-2 CARD WITH NO AUDIO ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -f image2 -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 \
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        elif [ "$AUDIO_CARD" == "0" ] && [ "$AUDIO_CHANNELS" == "1" ]; then

          # ******************************* MPEG-2 CARD WITH BEEP ************************************

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG \
            -f image2 -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -f lavfi -ac 1 \
            -i "sine=frequency=500:beep_factor=4:sample_rate=44100:duration=0" \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO\
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &

        else

          # ******************************* MPEG-2 CARD WITH AUDIO ************************************

          # PCR PID ($PIDSTART) seems to be fixed as the same as the video PID.  
          # PMT, Vid and Audio PIDs can all be set. 

          sudo nice -n -30 $PATHRPI"/ffmpeg" -loglevel $MODE_DEBUG -itsoffset "$ITS_OFFSET" \
            -thread_queue_size 512 \
            -f image2 -loop 1 \
            -framerate 5 -video_size "$VIDEO_WIDTH"x"$VIDEO_HEIGHT" \
            -i $IMAGEFILE \
            \
            -thread_queue_size 512 \
            -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
            -i hw:$AUDIO_CARD_NUMBER,0 \
            \
            -vf fps=10 -b:v $BITRATE_VIDEO -minrate:v $BITRATE_VIDEO -maxrate:v  $BITRATE_VIDEO \
            -f mpegts  -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
            -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
            -mpegts_service_id $SERVICEID \
            -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
            -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
            -muxrate $BITRATE_TS -y $OUTPUT &
        fi
      ;;
    esac
  ;;
# ============================================ H264 FROM C920 =============================

  "C920H264" | "C920HDH264" | "C920FHDH264")

    # Turn off the viewfinder (which would show Pi Cam)
    v4l2-ctl --overlay=0

    # Set up the means to transport the stream out of the unit
    # Not compatible with COMPVID or STREAMER output modes
    case "$MODE_OUTPUT" in
      "IP")
        : # Do nothing
      ;;
      "DATVEXPRESS")
        echo "set ptt tx" >> /tmp/expctrl
        # ffmpeg sends the stream directly to DATVEXPRESS
      ;;
      "LIMEMINI")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      "LIMEUSB")
        /home/pi/libdvbmod/DvbTsToIQ/dvb2iq -i videots -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
          -m $MODTYPE -c $CONSTLN \
          | buffer \
          | sudo $PATHRPI"/limetx" -s $SYMBOLRATE_K -f $FREQ_OUTPUTK -g $LIME_GAIN &
      ;;
      *)
        # For IQ, QPSKRF, DIGITHIN and DTX1 rpidatv generates the IQ (and RF for QPSKRF)
        sudo $PATHRPI"/rpidatv" -i videots -s $SYMBOLRATE_K -c $FECNUM"/"$FECDEN -f $FREQUENCY_OUT -p $GAIN -m $MODE -x $PIN_I -y $PIN_Q &
      ;;
    esac

    # Now set the camera resolution and H264 output
    case "$MODE_INPUT" in
      C920H264)
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=640,height=480,pixelformat=1
#       v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=800,height=600,pixelformat=1
      ;;
      C920HDH264)
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1280,height=720,pixelformat=1
      ;;
      C920FHDH264)
        v4l2-ctl --device="$VID_WEBCAM" --set-fmt-video=width=1920,height=1080,pixelformat=1
      ;;
    esac

    # And send the camera stream with audio to the fifo

    sudo nice -n -30 $PATHRPI"/ffmpeg" \
      -f v4l2 -vcodec h264 \
      -i "$VID_WEBCAM" \
      -f alsa -ac $AUDIO_CHANNELS -ar $AUDIO_SAMPLE \
      -i hw:$AUDIO_CARD_NUMBER,0 \
      -vcodec copy \
      -f mpegts -blocksize 1880 -acodec mp2 -b:a 64K -ar 44100 -ac $AUDIO_CHANNELS\
      -mpegts_original_network_id 1 -mpegts_transport_stream_id 1 \
      -mpegts_service_id $SERVICEID \
      -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
      -metadata service_provider=$CALL -metadata service_name=$CHANNEL \
      -muxrate $BITRATE_TS -y $OUTPUT &

  ;;
esac

# ============================================ END =============================================================

# flow exits from a.sh leaving ffmpeg or avc2ts and rpidatv running
# these processes are killed by menu.sh, rpidatvgui or b.sh on selection of "stop transmit"
