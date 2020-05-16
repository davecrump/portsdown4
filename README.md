![portsdown banner](/doc/img/Portsdown_A27.jpg)
# Portsdown Buster Build For RPi 4

**The Portsdown** is a DVB-S and DVB-S2 digital television system for the Raspberry Pi.  The core of the system was written by Evariste Courjaud F5OEO and is maintained by him.  This BATC Version, known as the Portsdown, has been developed by a team of BATC members for use with a LimeSDR Mini or a DATV Express PCB and a MiniTiouner.  The intention is that the design should be reproducible by someone who has never used Linux before.  Detailed instructions on loading the software are listed below, and further details of the complete system design and build are on the BATC Wiki at https://wiki.batc.org.uk/The_Portsdown_Transmitter.  There is a Forum for discussion of the project here: https://forum.batc.org.uk/viewforum.php?f=103

This version is based on Raspbian Buster and is only compatible with the Raspberry Pi 4 and 7 inch screen.  It MUST be installed after the Langstone Microwave Transceiver and is designed to coexist with it and share much of its hardware.  This is not a beginner's version and is at a very immature state of development.  Only a limited subset of the Portsdown features are fully working in this build; currently the receiver is the only working part.

Our thanks to Evariste, Heather and all the other contributors to this community project.  All code within the project is GPL.

# Installation for BATC Portsdown RPi 4 Version

Follow the instructions to load the Langstone system and check that is functioning properly.  Then log in by ssh and:


```sh
Langstone/stop
wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/portsdown-a27/master/install.sh
chmod +x install.sh
./install.sh
```

The initial build can take between 45 minutes and one hour, however it does not need any user input, so go and make a cup of coffee and keep an eye on the touchscreen.  When the build is finished the Pi will reboot and start-up with the touchscreen menu.

- If your ISP is Virgin Media and you receive an error after entering the wget line: 'GnuTLS: A TLS fatal alert has been received.', it may be that your ISP is blocking access to GitHub.  If (only if) you get this error with Virgin Media, paste the following command in, and press return.
```sh
sudo sed -i 's/^#name_servers.*/name_servers=8.8.8.8/' /etc/resolvconf.conf
```
Then reboot, and try again.  The command asks your RPi to use Google's DNS, not your ISP's DNS.

- If your ISP is BT, you will need to make sure that "BT Web Protect" is disabled so that you are able to download the software.

- When it has finished, the installation will reboot and the touchscreen should be activated.  You will need to log in to the console to set up any other displays or advanced options.


# Advanced notes

To load the development version, cut and paste in the following lines:

```sh
Langstone/stop
wget https://raw.githubusercontent.com/davecrump/portsdown-a27/master/install.sh
chmod +x install.sh
./install.sh -d
```

To load a version from your own GitHub repo (github.com/your_account/portsdown-a27), cut, paste and amend the following lines:
```sh
wget https://raw.githubusercontent.com/your_account/portsdown-a27/master/install.sh
chmod +x install.sh
./install.sh -u your_account
```
The alternative github user account will need to include forks of both the portsdown and avc2ts repositories.
