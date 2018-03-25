######### GENERAL ##########################
StrNotImplemented="Noch nicht implementiert"
StrNotImpMsg="Noch nicht implementiert.  Weiter mit Enter"

######## INPUT MENU #####################
StrInputSetupTitle="Eingabe Auswahl"
StrInputSetupDescription="Navigieren mit Pfeiltasten Hoch/Runter, Leertaste zum Markieren"
StrInputSetupCAMH264="Pi Kamera, H264 Encoding"
StrInputSetupCAMMPEG_2="Pi Kamera, MPEG-2 Encoding (für alte Sat RX)"
StrInputSetupFILETS="Wiedergabe Transport stream file (.ts)"
StrInputSetupPATERNAUDIO="JPEG Bild aus Datei (kein Audio)"
StrInputSetupCARRIER="Träger ohne Modulation"
StrInputSetupTESTMODE="Testmodus für Carrier Null"
StrInputSetupIPTSIN="Transport Stream aus dem Netzwerk"
StrInputSetupFILETSName="TS Datei ist nun"
StrInputSetupPATERNAUDIOName="JPEG Bild ist nun"
StrInputSetupIPTSINName="Sende UDP Stream an Port 10000 mit "
StrInputSetupIPTSINTitle="Information for UDP Stream"
StrInputSetupANALOGCAM="Analoger (EasyCap) Video Eingang"
StrInputSetupANALOGCAMTitle="Einstellungen Analogeingang"
StrInputSetupANALOGCAMName="Eingang Analog Name (/dev/video0)"
StrInputSetupVNC="Zeige PC Desktop via VNC"
StrInputSetupVNCName="IP des PC welcher VNC nutzt (password datv)"
StrInputSetupVNCTitle="VNC Einstellungen"
StrInputSetupDESKTOP="Zeige Touchscreen oder Raspberry Pi Display"
StrInputSetupCONTEST="Zeige Contest Nummern"

StrPIN_IContext="I GPIO {12,18,40} (12 Voreinstellung für Portsdown: pin32)"
StrPIN_ITitle="I Output GPIO"
StrPIN_QContext="Q GPIO {13,19,41} (13 Voreinstellung für Portsdown: pin33)"
StrPIN_QTitle="Q Output GPIO"

######## CALL MENU ###################"
StrCallContext="Rufzeichen Eingabe"
StrCallTitle="CALL"

StrLocatorContext="Locator Eingabe"
StrLocatorTitle="Locator"

######## OUTPUT MENU #####################
StrOutputSetupTitle="Ausgabemodus"
StrOutputSetupContext="Navigieren mit Pfeiltasten Hoch/Runter, Leertaste zum Markieren"
StrOutputSetupIQ="IQ Ausgabe zur Portsdown Filter Modulator Karte"
StrOutputSetupRF="UGLY Test Mode.  HF auf Pin 32. Kein Modulator benötigt."
StrOutputSetupBATC="Stream zu BATC.TV"
StrOutputSetupDigithin="Benutze Digithin Modulator Karte"
StrOutputSetupDTX1="Benutze DTX1 TS Extender Karte"
StrOutputSetupDATVExpress="Benutze DATV Express verbunden über USB"

StrOutputSetupIP="Senden über IP Netzwerk"
StrOutputSetupIPTSOUTName="IP Adresse: 230.0.0.1 zum Beispiel"
StrOutputSetupIPTSOUTTitle="Einstellungen Netzwerkausgabe"

StrOutputRFFreqContext="Sendefrequenzy(MHz)"
StrOutputRFFreqTitle="HF Frequenz"

StrOutputRFGainContext="HF Pegel(0=-3.4dbm 7=10.6dbm)"
StrOutputRFGainTitle="HF Pegel"

StrOutputBATCContext="BATC Stream Name"
StrOutputBATCTitle="BATC Setup"

######### SYMBOLRATE AND FEC ##########
StrOutputSymbolrateContext="Symbolrate (KSymbols/s):125-4000"
StrOutputSymbolrateTitle="Symbolrate"

StrOutputFECTitle="FEC Einstellung"
StrOutputFECContext="Navigieren mit Pfeiltasten Hoch/Runter, Leertaste zum Markieren"

StrOutputTitle="Ausgabe Parameter"
StrOutputContext="Auswahl:"

StrOutputSR="Symbolrate Einstellungen"
StrOutputFEC="FEC Einstellungen"
StrOutputMode="Ausgabemodus"

StrPIDSetup="PID Einstellungen"
StrPIDSetupTitle="PID Einstellungen"
StrPIDSetupContext="Eingabe der PMT PID. Video PID wird PMT PID + 1, Audio PID wird PMT + 2"


######### STATUS ##########
StrStatusTitle="Sende"

######### SYSTEM SETUP #######
StrSystemTitle="Systemeinstellungen"
StrSystemContext="Auswahl:"
StrAutostartMenu="Einstellungen Autostart"
StrDisplayMenu="Displaytyp"
StrIPMenu="Zeige aktuelle IP Adresse an"

######## AUTOSTART MENU #########

StrAutostartSetupTitle="Einstellungen Autostart"
StrAutostartSetupContext="Auswahl:"
AutostartSetupPrompt="Einloggen zum Linux Command Prompt"
AutostartSetupConsole="Einloggen zum Portsdown Menu"
AutostartSetupTX_boot="Booten zum direkten Senden"
AutostartSetupDisplay_boot="Booten bis zum Touchscreen Display"
AutostartSetupButton_boot="Booten bis zur Tastenbedienung"

######## TOUCHSCREEN MENU #########

StrDisplaySetupTitle="Touchscreen Auswahl - Nach Änderung Reboot notwendig"
StrDisplaySetupContext="Auswahl:"
DisplaySetupTontec="Tontec 3,5 Zoll"
DisplaySetupHDMI="HDMI Touchscreen"
DisplaySetupRpiLCD="Waveshare 3,5 A LCD (standard)"
DisplaySetupRpiBLCD="Waveshare 3,5 B LCD (alternative)"
DisplaySetupRpi4LCD="Waveshare 4 inch LCD (alternative)"
DisplaySetupConsole="Network Console"

StrIPSetupTitle="Statische IP setzen"
StrIPSetupContext="Example: 192.168.1.60"

######## LANGUAGE MENU #########

StrLanguageTitle="Sprachauswahl"
StrKeyboardChange="Nach Änderung Reboot notwendig"

# Check Current Version
INSTALLEDVERSION=$(head -c 9 /home/pi/rpidatv/scripts/installed_version.txt)



######## MAIN MENU #########
StrMainMenuTitle="Portsdown DATV TX Version "$INSTALLEDVERSION" von F5OEO und dem BATC Team"
StrMainMenuSource="Auswahl Videoquelle"
StrMainMenuOutput="Einstellungen Ausgabemodus"
StrMainMenuCall="Einstellung Rufzeichen"
StrMainMenuSystem="System-Einstellungen"
StrMainMenuSystem2="erweiterte Einstellungen"
StrMainMenuReceive="Empfang mit RTL-SDR"
StrMainMenuLanguage="Sprach und Tastaturauswahl"
StrMainMenuShutdown="Abschalt und Reboot Optionen"
StrMainMenuExitTitle="Beenden"
StrMainMenuExitContext="Danke für die Nutzung von RpiDATV... 73's de F5OEO(evaristec@gmail.com)"

########## FILE #################

FileMenuTitle="! Fehler !"
FileMenuContext="Fehler beim Setzen des Pfades zur Datei"





