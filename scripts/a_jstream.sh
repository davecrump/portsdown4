# This file is sourced from a.sh if JSTREAM is selected as the output device.

# But is not yet ready for release

# First sort out the environment variables

  JETSONIP=$(get_config_var jetsonip $JCONFIGFILE)
  JETSONUSER=$(get_config_var jetsonuser $JCONFIGFILE)
  JETSONPW=$(get_config_var jetsonpw $JCONFIGFILE)
  LKVUDP=$(get_config_var lkvudp $JCONFIGFILE)
  LKVPORT=$(get_config_var lkvport $JCONFIGFILE)
  FORMAT=$(get_config_var format $PCONFIGFILE)
  CMDFILE="/home/pi/tmp/jetson_command.txt"
  CHANNEL="Portsdown_Jetson"

  # Set the video format
  if [ "$FORMAT" == "1080p" ]; then
    VIDEO_WIDTH=1920
    VIDEO_HEIGHT=1080
  elif [ "$FORMAT" == "720p" ]; then
    VIDEO_WIDTH=1280
    VIDEO_HEIGHT=720
  elif [ "$FORMAT" == "16:9" ]; then
    VIDEO_WIDTH=1024
    VIDEO_HEIGHT=576
  else  # SD
    VIDEO_WIDTH=720
    VIDEO_HEIGHT=576
  fi

  # Calculate the exact TS Bitrate for Lime
  BITRATE_TS="$($PATHRPI"/dvb2iq" -s $SYMBOLRATE_K -f $FECNUM"/"$FECDEN \
    -d -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES )"

  AUDIO_BITRATE=20000
  let TS_AUDIO_BITRATE=AUDIO_BITRATE*14/10
  let VIDEOBITRATE=(BITRATE_TS-12000-TS_AUDIO_BITRATE)*650/1000    # Dave
  #let VIDEOBITRATE=(BITRATE_TS-12000-TS_AUDIO_BITRATE)*650/1000    # Evariste
  #let VIDEOBITRATE=(BITRATE_TS-12000-TS_AUDIO_BITRATE)*600/1000  # Mike
  let VIDEOPEAKBITRATE=VIDEOBITRATE*110/100

MODE_INPUT=JHDMI

    case "$MODE_INPUT" in
    "JHDMI")
      # H265 HDMI checks for a Elgato CamLink 4K, ATEM Mini or LKV373A
      # It checks for each in turn and uses the first that it finds

      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts

      # Pass audio selection to Jetson (auto or mic only)
      AUDIO_PREF=$AUDIO_PREF
      AUDIO_CARD=0
   
      if [ "\$AUDIO_PREF" == "mic" ]; then
        arecord -l | grep -E -q "USB Audio Device|USB AUDIO|Head|Sound Device"
        if [ \$? == 0 ]; then   ## Present
          # Look for the dedicated USB Audio Device, select the line and take
          # the 6th character.  Max card number = 8 !!
          AUDIO_CARD="\$(arecord -l | grep -E "USB Audio Device|USB AUDIO|Head|Sound Device" \
            | head -c 6 | tail -c 1)"
          AUDIO_CHAN=1
        fi
      fi

      lsusb | grep -q "Elgato"
      if [ \$? == 0 ]; then
        HDMI_SRC="CamLink"

        VID_DEVICE="\$(v4l2-ctl --list-devices 2> /dev/null | \
          sed -n '/Link/,/dev/p' | grep 'dev' | tr -d '\t')"

        if [ "\$AUDIO_PREF" == "auto" ]; then
          # Look for the dedicated USB Audio Device, select the line and take
          # the 6th character.  Max card number = 8 !!
          AUDIO_CARD="\$(arecord -l | grep -E "C4K" \
            | head -c 6 | tail -c 1)"
          AUDIO_CHAN=2
        fi
      else
        lsusb | grep -q "Blackmagic"
        if [ \$? == 0 ]; then
          HDMI_SRC="ATEM"

          VID_DEVICE="\$(v4l2-ctl --list-devices 2> /dev/null | \
            sed -n '/Blackmagic/,/dev/p' | grep 'dev' | tr -d '\t')"

          if [ "\$AUDIO_PREF" == "auto" ]; then
            # Look for the dedicated USB Audio Device, select the line and take
            # the 6th character.  Max card number = 8 !!
            AUDIO_CARD="\$(arecord -l | grep -E "Blackmagic" \
              | head -c 6 | tail -c 1)"
            AUDIO_CHAN=2
          fi
        else
          HDMI_SRC="LKV"
        fi
      fi

      case \$HDMI_SRC in
      CamLink)
        gst-launch-1.0 -vvv -e \
          v4l2src device=\$VID_DEVICE \
          '!' nvvidconv \
          '!' "video/x-raw(memory:NVMM)", width=$VIDEO_WIDTH, height=$VIDEO_HEIGHT, "format=(string)UYVY" '!' nvvidconv \
          '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
          '!' video/x-h265, width=$VIDEO_WIDTH, height=$VIDEO_HEIGHT, "stream-format=(string)byte-stream" '!' queue \
          '!' mux. alsasrc device=hw:\$AUDIO_CARD \
          '!' audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=\$AUDIO_CHAN '!' voaacenc bitrate=20000 \
          '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
        | ffmpeg -i - -ss 8 \
          -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
          -c:a copy -f mpegts \
          -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -mpegts_flags system_b - \
        | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
      ;;
      ATEM)                              # This works!!
        gst-launch-1.0 -vvv -e \
          v4l2src device=\$VID_DEVICE \
          '!' jpegparse '!' jpegdec '!' video/x-raw '!' nvvidconv \
          '!' "video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)NV12, framerate=(fraction)24/1" \
          '!' omxh264enc control-rate=2 bitrate=1500000 '!' 'video/x-h264, stream-format=(string)byte-stream' \
          '!' h264parse '!' queue '!' flvmux name=muxer alsasrc device=hw:\$AUDIO_CARD \
          '!' audioresample ! "audio/x-raw,rate=48000" ! queue  \
          '!' voaacenc bitrate=32000 ! aacparse ! queue ! muxer. muxer. \
          '!' rtmpsink location="$STREAM_URL/$STREAM_KEY"
      ;;
      esac
ENDSSH
      ) &
EOM

    ;;
    "JCAM")
      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts

      gst-launch-1.0 -q nvarguscamerasrc \
        '!' "video/x-raw(memory:NVMM),width=$VIDEO_WIDTH, height=$VIDEO_HEIGHT, format=(string)NV12,framerate=(fraction)25/1" \
        '!' nvvidconv flip-method=2 '!' "video/x-raw(memory:NVMM), format=(string)I420" \
        '!' nvvidconv '!' 'video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420' \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:2 \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
        -mpegts_flags system_b- \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
ENDSSH
      ) &
EOM
    ;;
    "JWEBCAM")
      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts

      lsusb | grep -q "095d:3001"
      if [ \$? == 0 ]; then
        USBCAM_TYPE="EagleEye"
        VID_DEVICE="\$(v4l2-ctl --list-devices 2> /dev/null | \
          sed -n '/EagleEye/,/dev/p' | grep 'dev' | tr -d '\t')"
      else
        USBCAM_TYPE="C920"
      fi

      if [[ "\$USBCAM_TYPE" == "EagleEye" ]]; then

      # PolyComm EagleEye Code.  Uses USB audio dongle
      gst-launch-1.0 -vvv -e \
        v4l2src device=\$VID_DEVICE do-timestamp=true '!' 'image/jpeg,width=1280,height=720,framerate=25/1' \
        '!' jpegparse '!'jpegdec '!' nvvidconv \
        '!' "video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420" \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,width=(int)$VIDEO_WIDTH,height=(int)$VIDEO_HEIGHT,stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:2 \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" -mpegts_flags system_b - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1

      else

      # C920 code
      gst-launch-1.0 -vvv -e \
        v4l2src device=/dev/video0 do-timestamp=true '!' 'video/x-h264,width=1280,height=720,framerate=30/1' \
        '!' h264parse '!' omxh264dec '!' nvvidconv \
        '!' "video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420" \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,width=(int)$VIDEO_WIDTH,height=(int)$VIDEO_HEIGHT,stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:2 \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=32000, channels=2' \
        '!' audioconvert '!' 'audio/x-raw, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
        -mpegts_flags system_b - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1

      fi

ENDSSH
      ) &
EOM
    ;;
    "JCARD")
      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts
      gst-launch-1.0 -v -e \
        filesrc location=/home/nano/dvbsdr/scripts/tcfw.jpg '!' jpegdec '!' imagefreeze \
        '!' textoverlay text="$CALL" \
        valignment=bottom ypad=40 halignment=center font-desc="Sans, 25" draw-outline="false" draw-shadow="false" \
        '!' 'video/x-raw,width=1280,height=720,framerate=30/1' \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,width=(int)1280,height=(int)720,stream-format=(string)byte-stream' '!' queue \
        '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
        -mpegts_flags system_b - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
ENDSSH
      ) &
EOM
    ;;
    esac

  # Run the Command on the Jetson
  source "$CMDFILE"

