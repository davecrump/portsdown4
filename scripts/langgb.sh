######### GENERAL ##########################
StrNotImplemented="Not Implemented Yet"
StrNotImpMsg="Not implemented yet.  Please press enter to continue"

######## INPUT MENU #####################
StrInputSetupTitle="Input choice"
StrInputSetupDescription="Press down/up arrows to move, space to select"
StrInputSetupCAMH264="Pi Camera, H264 Encoding"
StrInputSetupCAMMPEG_2="Pi Camera, MPEG-2 Encoding (for old Sat RX)"
StrInputSetupFILETS="Play a Transport stream file (.ts)"
StrInputSetupPATERNAUDIO="JPEG Pictures From File (no audio)"
StrInputSetupCARRIER="Carrier with no Modulation"
StrInputSetupTESTMODE="Testmode for Carrier Null"
StrInputSetupIPTSIN="Transport stream from network"
StrInputSetupFILETSName="TS file is now"
StrInputSetupPATERNAUDIOName="JPEG picture is now"
StrInputSetupIPTSINName="Send UDP Stream to port 10000 at "
StrInputSetupIPTSINTitle="Information for UDP Stream"
StrInputSetupANALOGCAM="Analog (EasyCap) Video Input"
StrInputSetupANALOGCAMTitle="Analog input setup"
StrInputSetupANALOGCAMName="Input analog name (/dev/video0)"
StrInputSetupVNC="Show PC Desktop via VNC"
StrInputSetupVNCName="IP of PC using VNC (password datv)"
StrInputSetupVNCTitle="VNC setup"
StrInputSetupDESKTOP="Show Touchscreen or Raspberry Pi Display"
StrInputSetupCONTEST="Show Contest Numbers"

StrPIN_IContext="I GPIO {12,18,40} (12 is default for Portsdown: pin32)"
StrPIN_ITitle="I output GPIO"
StrPIN_QContext="Q GPIO {13,19,41} (13 is default for Portsdown: pin33)"
StrPIN_QTitle="Q output GPIO"

######## CALL MENU ###################"
StrCallContext="CALL Setup"
StrCallTitle="CALL"

StrLocatorContext="Locator Setup"
StrLocatorTitle="Locator"

######## OUTPUT MENU #####################
StrOutputSetupTitle="Output type"
StrOutputSetupContext="Press down/up arrows to move, space to select"
StrOutputSetupIQ="IQ output for Portsdown filter modulator board"
StrOutputSetupRF="UGLY Test Mode.  RF from pin 32. No modulator required."
StrOutputSetupBATC="Stream to BATC.TV"
StrOutputSetupDigithin="Use Digithin Modulator Card"
StrOutputSetupDTX1="Use DTX1 TS Extender Card"
StrOutputSetupDATVExpress="Use DATV Express connected by USB"
StrOutputSetupIP="Transmit UDP Transport Stream across IP network"
StrOutputSetupIPTSOUTName="Destination IP address: 192.168.2.110 for example"
StrOutputSetupIPTSOUTTitle="Destination IP setup"

StrOutputRFFreqContext="Output frequency(MHZ)"
StrOutputRFFreqTitle="RF Frequency"

StrOutputRFGainContext="RF Gain(0=-3.4dbm 7=10.6dbm"
StrOutputRFGainTitle="RF Gain"

StrOutputBATCContext="BATC Stream name"
StrOutputBATCTitle="BATC Setup"

######### SYMBOLRATE AND FEC ##########
StrOutputSymbolrateContext="Symbol Rate (KSymbols/s):125-4000"
StrOutputSymbolrateTitle="Symbol Rate"

StrOutputFECTitle="FEC Setup"
StrOutputFECContext="Press down/up arrows to move, space to select"

StrOutputTitle="Output parameters"
StrOutputContext="Choice:"

StrOutputSR="Symbol Rate Setup"
StrOutputFEC="FEC Setup"
StrOutputMode="Output Mode"

StrPIDSetup="PID setup"
StrPIDSetupTitle="PID setup"
StrPIDSetupContext="Set PMT PID for MPEG-2. For H264 it will be set to Video PID-1"


######### STATUS ##########
StrStatusTitle="Transmitting"

######### SYSTEM SETUP #######
StrSystemTitle="System setup"
StrSystemContext="Choice:"
StrAutostartMenu="Automatic startup"
StrDisplayMenu="Display type"
StrIPMenu="Display Current IP Address"

######## AUTOSTART MENU #########

StrAutostartSetupTitle="Autostart setup"
StrAutostartSetupContext="Choice:"
AutostartSetupPrompt="Log-on to Linux Command Prompt"
AutostartSetupConsole="Log-on to Portsdown Console Menu"
AutostartSetupTX_boot="Boot-up to Transmit"
AutostartSetupDisplay_boot="Boot-up to Touchscreen Display"
AutostartSetupButton_boot="Boot-up to Button Control"

######## TOUCHSCREEN MENU #########

StrDisplaySetupTitle="Touchscreen setup - needs reboot after setting"
StrDisplaySetupContext="Choice:"
DisplaySetupTontec="Tontec 3.5 inches"
DisplaySetupHDMI="HDMI touchscreen"
DisplaySetupRpiLCD="Waveshare 3.5 A LCD (default)"
DisplaySetupRpiBLCD="Waveshare 3.5 B LCD (alternative)"
DisplaySetupRpi4LCD="Waveshare 4 inch LCD (alternative)"
DisplaySetupConsole="Network Console"

StrIPSetupTitle="Setup a static IP"
StrIPSetupContext="Example: 192.168.1.60"

######## LANGUAGE MENU #########

StrLanguageTitle="Language Selection"
StrKeyboardChange="Needs reboot after setting"

# Check Current Version
INSTALLEDVERSION=$(head -c 9 /home/pi/rpidatv/scripts/installed_version.txt)



######## MAIN MENU #########
StrMainMenuTitle="Portsdown DATV TX Version "$INSTALLEDVERSION" By F5OEO and the BATC Team"
StrMainMenuSource="Select Video Source"
StrMainMenuOutput="Configure Output"
StrMainMenuCall="Station call setup"
StrMainMenuSystem="System Setup"
StrMainMenuSystem2="Advanced System Setup"
StrMainMenuReceive="Receive from Stream or with RTL-SDR"
StrMainMenuLanguage="Set Language and Keyboard"
StrMainMenuShutdown="Shutdown and Reboot Options"
StrMainMenuExitTitle="Exit"
StrMainMenuExitContext="Thanks for using RpiDATV... 73's de F5OEO(evaristec@gmail.com)"

########## FILE #################

FileMenuTitle="! ERROR !"
FileMenuContext="Error setting path to image file"





