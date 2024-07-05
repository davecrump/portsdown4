# This file is sourced from a.sh if JLIME is selected as the output device.

# First sort out the environment variables

  JETSONIP=$(get_config_var jetsonip $JCONFIGFILE)
  JETSONUSER=$(get_config_var jetsonuser $JCONFIGFILE)
  JETSONPW=$(get_config_var jetsonpw $JCONFIGFILE)
JETSONROOTPW=$(get_config_var jetsonrootpw $JCONFIGFILE)
  LKVUDP=$(get_config_var lkvudp $JCONFIGFILE)
  LKVPORT=$(get_config_var lkvport $JCONFIGFILE)
  FORMAT=$(get_config_var format $PCONFIGFILE)
  ENCODING=$(get_config_var encoding $PCONFIGFILE)
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
    VIDEO_HEIGHT=576 #416 # was 405
  else  # SD
    if [ "$BITRATE_VIDEO" -lt 75000 ]; then
      VIDEO_WIDTH=160
      VIDEO_HEIGHT=128
    else
      if [ "$BITRATE_VIDEO" -lt 150000 ]; then
        VIDEO_WIDTH=352
        VIDEO_HEIGHT=288
      else
        VIDEO_WIDTH=720
        VIDEO_HEIGHT=576
      fi
    fi
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

  echo
  echo BITRATETS $BITRATE_TS
  echo VIDEOBITRATE $VIDEOBITRATE
  echo VIDEOPEAKBITRATE $VIDEOPEAKBITRATE
  echo AUDIO_BITRATE $AUDIO_BITRATE
  echo TS_AUDIO_BITRATE $TS_AUDIO_BITRATE
  echo



  case "$ENCODING" in
  "IPTS in")
      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts
      netcat -u -4 -l 10000 \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
ENDSSH
      ) &
EOM
  ;;
  "H265")
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
        if [ \$? == 0 ]; then                            # It's an ATEM but could be V9.0 or V9.5
          HDMI_SRC="ATEM"

          # ATEM pre 9.5:  v4l2-ctl --list-devices returns: "Blackmagic ............"
          # ATEM post 9.5: v4l2-ctl --list-devices returns: "ATEM .........:"

          v4l2-ctl --list-devices | grep -q "^Blackmagic"
          if [ \$? == 0 ]; then                          #################### V9.0   

            VID_DEVICE="\$(v4l2-ctl --list-devices 2> /dev/null | \
              sed -n '/Blackmagic/,/dev/p' | grep 'dev' | tr -d '\t')"

            if [ "\$AUDIO_PREF" == "auto" ]; then
              # Look for the dedicated USB Audio Device, select the line and take
              # the 6th character.  Max card number = 8 !!
              AUDIO_CARD="\$(arecord -l | grep -E "Blackmagic" \
                | head -c 6 | tail -c 1)"
              AUDIO_CHAN=2
            fi
          else                                            ######################## Try V9.5
            VID_DEVICE="\$(v4l2-ctl --list-devices 2> /dev/null | \
              sed -n '/ATEM/,/dev/p' | grep 'dev' | tr -d '\t')"

            if [ "\$AUDIO_PREF" == "auto" ]; then
              # Look for the dedicated USB Audio Device, select the line and take
              # the 6th character.  Max card number = 8 !!
              AUDIO_CARD="\$(arecord -l | grep -E "ATEM" \
                | head -c 6 | tail -c 1)"
              AUDIO_CHAN=2
            fi
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
          -mpegts_service_type "0x1f" -mpegts_flags system_b - \
        | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
      ;;
      ATEM)
        gst-launch-1.0 -vvv -e \
          v4l2src device=\$VID_DEVICE io-mode=2 \
          '!' image/jpeg \
          '!' jpegparse '!' nvjpegdec '!' video/x-raw '!' nvvidconv \
          '!' 'video/x-raw(memory:NVMM),width=(int)$VIDEO_WIDTH,height=(int)$VIDEO_HEIGHT,format=(string)I420' \
          '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=25 \
          '!' 'video/x-h265,width=(int)$VIDEO_WIDTH,height=(int)$VIDEO_HEIGHT,stream-format=(string)byte-stream' '!' queue \
          '!' mux. alsasrc device=hw:\$AUDIO_CARD \
          '!' audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=\$AUDIO_CHAN '!' voaacenc bitrate=20000 \
          '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
        | ffmpeg -i - -ss 5 \
          -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
          -c:a copy -f mpegts \
          -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
          -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
          -mpegts_service_type "0x1f" -mpegts_flags system_b - \
        | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
          -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
      ;;
      LKV)
        gst-launch-1.0 udpsrc address=$LKVUDP port=$LKVPORT \
          '!' video/mpegts '!' tsdemux name=dem dem. '!' queue '!' h264parse '!' omxh264dec \
          '!' nvvidconv interpolation-method=2 \
          '!' 'video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420' \
          '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
          '!' 'video/x-h265,stream-format=(string)byte-stream' '!' mux. dem. '!' queue \
          '!' mpegaudioparse '!' avdec_mp2float '!' audioconvert '!' audioresample \
          '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
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

      AUDIO_CARD="\$(arecord -l | grep -E "USB Audio Device|USB AUDIO|Head|Sound Device" \
        | head -c 6 | tail -c 1)"

      gst-launch-1.0 -q nvarguscamerasrc \
        '!' "video/x-raw(memory:NVMM),width=1920, height=1080, format=(string)NV12,framerate=(fraction)30/1" \
        '!' nvvidconv flip-method=2 '!' "video/x-raw(memory:NVMM), format=(string)I420" \
        '!' nvvidconv '!' 'video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420' \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:\$AUDIO_CARD \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
        -mpegts_service_type "0x1f" -mpegts_flags system_b - \
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

        AUDIO_CARD="\$(arecord -l | grep -E "USB Audio Device|USB AUDIO|Head|Sound Device" \
          | head -c 6 | tail -c 1)"
        AUDIO_CHAN=1

      # PolyComm EagleEye Code.  Uses USB audio dongle
      gst-launch-1.0 -vvv -e \
        v4l2src device=\$VID_DEVICE do-timestamp=true \
        '!' 'image/jpeg,width=$VIDEO_WIDTH,height=$VIDEO_HEIGHT,framerate=25/1' \
        '!' jpegparse '!' jpegdec '!' nvvidconv \
        '!' "video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420" \
        '!' omxh265enc control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE preset-level=3 iframeinterval=100 \
        '!' 'video/x-h265,width=(int)$VIDEO_WIDTH,height=(int)$VIDEO_HEIGHT,stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:\$AUDIO_CARD \
        '!' audio/x-raw,format=S16LE,layout=interleaved,rate=48000,channels=\$AUDIO_CHAN '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" \
        -mpegts_service_type "0x1f" -mpegts_flags system_b - \
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
  ;;
  "H264")
    case "$MODE_INPUT" in
    "JHDMI")
      # Write the assembled Jetson command to a temp file
      /bin/cat <<EOM >$CMDFILE
      (sshpass -p $JETSONPW ssh -o StrictHostKeyChecking=no $JETSONUSER@$JETSONIP 'bash -s' <<'ENDSSH' 
      cd ~/dvbsdr/scripts
      gst-launch-1.0 udpsrc address=$LKVUDP port=$LKVPORT \
        '!' video/mpegts '!' tsdemux name=dem dem. '!' queue '!' h264parse '!' omxh264dec \
        '!' nvvidconv interpolation-method=2 \
        '!' 'video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420' \
        '!' omxh264enc vbv-size=15 control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE \
        insert-sps-pps=1 insert-vui=1 cabac-entropy-coding=1 preset-level=3 profile=8 iframeinterval=100 \
        '!' 'video/x-h264, level=(string)4.1, stream-format=(string)byte-stream' '!' mux. dem. '!' queue \
        '!' mpegaudioparse '!' avdec_mp2float '!' audioconvert '!' audioresample \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
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
        '!' omxh264enc vbv-size=15 control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE \
        insert-sps-pps=1 insert-vui=1 cabac-entropy-coding=1 preset-level=3 profile=8 iframeinterval=100 \
        '!' 'video/x-h264, level=(string)4.1, stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:2 \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=48000, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" - \
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
      gst-launch-1.0 -vvv -e \
        v4l2src device=/dev/video0 do-timestamp=true '!' 'video/x-h264,width=1280,height=720,framerate=30/1' \
        '!' h264parse '!' omxh264dec '!' nvvidconv \
        '!' "video/x-raw(memory:NVMM), width=(int)$VIDEO_WIDTH, height=(int)$VIDEO_HEIGHT, format=(string)I420" \
        '!' omxh264enc vbv-size=15 control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE \
        insert-sps-pps=1 insert-vui=1 cabac-entropy-coding=1 preset-level=3 profile=8 iframeinterval=100 \
        '!' 'video/x-h264, level=(string)4.1, stream-format=(string)byte-stream' '!' queue \
        '!' mux. alsasrc device=hw:2 \
        '!' 'audio/x-raw, format=S16LE, layout=interleaved, rate=32000, channels=2' \
        '!' audioconvert '!' 'audio/x-raw, channels=1' '!' voaacenc bitrate=20000 \
        '!' queue '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
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
        '!' omxh264enc vbv-size=15 control-rate=2 bitrate=$VIDEOBITRATE peak-bitrate=$VIDEOPEAKBITRATE \
        insert-sps-pps=1 insert-vui=1 cabac-entropy-coding=1 preset-level=3 profile=8 iframeinterval=100 \
        '!' 'video/x-h264, level=(string)4.1, stream-format=(string)byte-stream' '!' queue \
        '!' mux. mpegtsmux alignment=7 name=mux '!' fdsink \
      | ffmpeg -i - -ss 8 \
        -c:v copy -max_delay 200000 -muxrate $BITRATE_TS \
        -c:a copy -f mpegts \
        -metadata service_provider="$CHANNEL" -metadata service_name="$CALL" \
        -mpegts_pmt_start_pid $PIDPMT -streamid 0:"$PIDVIDEO" -streamid 1:"$PIDAUDIO" - \
      | ../bin/limesdr_dvb -s "$SYMBOLRATE_K"000 -f $FECNUM/$FECDEN -r $UPSAMPLE -m $MODTYPE -c $CONSTLN $PILOTS $FRAMES \
        -t "$FREQ_OUTPUT"e6 -g $LIME_GAINF -q 1
ENDSSH
      ) &
EOM
    ;;
    esac
  ;;
  esac
  # Run the Command on the Jetson
  source "$CMDFILE"

  # Turn the PTT on after a 15 second delay
  /home/pi/rpidatv/scripts/jetson_lime_ptt.sh &  # PTT on RPi GPIO
  /home/pi/rpidatv/scripts/jetson_tx_on.sh &     # PTT on Jetson GPIO Pin 40
