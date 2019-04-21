![portsdown banner](/doc/img/Portsdown3.jpg)
# portsdown

**The Portsdown** is a DVB-S and DVB-S2 digital television transmitter for the Raspberry Pi 3B and 3B+.  The core of the transmitter was written by Evariste Courjaud F5OEO and is maintained by him.  This BATC Version, known as the Portsdown Transmitter, has been developed by a team of BATC members for use with a LimeSDR Mini, a DTAV Express PCB or an external synthesized oscillator and modulator/filter board to produce a signal suitable for driving a high power amateur television transmitter on the amateur bands.  The intention is that the design should be reproducible by someone who has never used Linux before.  Detailed instructions on loading the software are listed below, and further details of the complete transmitter design and build are on the BATC Wiki at https://wiki.batc.org.uk/The_Portsdown_Transmitter.  There is a Forum for discussion of the project here: https://forum.batc.org.uk/viewforum.php?f=103

The Portsdown software can also be used with a LimeSDR (Mini or USB) or a DATV Express or the bespoke filter-modulator board.

This version is based on Raspbian Stretch and is compatible with the Raspberry Pi 3B and 3B+.  It is the version on which development effort will be focused.  The older Jessie version will still be supported, but may not be updated with as many new features.

Our thanks to Evariste and all the other contributors to this community project.  All code within the project is GPL.

# Installation for BATC Portsdown Transmitter Stretch Version

The preferred installation method only needs a Windows PC connected to the same (internet-connected) network as your Raspberry Pi.  Do not connect a keyboard or HDMI display directly to your Raspberry Pi.

- First download the 2019-04-08 release of Raspbian Stretch Lite on to your Windows PC from here http://downloads.raspberrypi.org/raspbian_lite/images/raspbian_lite-2019-04-09/2019-04-08-raspbian-stretch-lite.zip

- Unzip the image and then transfer it to a Micro-SD Card using Win32diskimager https://sourceforge.net/projects/win32diskimager/

- Before you remove the card from your Windows PC, look at the card with windows explorer; the volume should be labeled "boot".  Create a new empty file called ssh in the top-level (root) directory by right-clicking, selecting New, Text Document, and then change the name to ssh (not ssh.txt).  You should get a window warning about changing the filename extension.  Click OK.  If you do not get this warning, you have created a file called ssh.txt and you need to rename it ssh.  IMPORTANT NOTE: by default, Windows (all versions) hides the .txt extension on the ssh file.  To change this, in Windows Explorer, select File, Options, click the View tab, and then untick "Hide extensions for known file types". Then click OK.

- If you have a Pi Camera and/or touchscreen display, you can connect them now.  Power up the RPi with the new card inserted, and a network connection.  Do not connect a keyboard or HDMI display to the Raspberry Pi. 

- Find the IP address of your Raspberry Pi using an IP Scanner (such as Advanced IP Scanner http://filehippo.com/download_advanced_ip_scanner/ for Windows, or Fing on an iPhone) to get the RPi's IP address 

- From your windows PC use Putty (http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html) to log in to the IP address that you noted earlier.  You will get a Security warning the first time you try; this is normal.

- Log in (user: pi, password: raspberry) then cut and paste the following code in, one line at a time:

```sh
wget https://raw.githubusercontent.com/BritishAmateurTelevisionClub/portsdown/master/install.sh
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

- For French menus and keyboard, replace the last line above with 
```sh
./install.sh fr
```

- When it has finished, the installation will reboot.  If you have a Waveshare or 7 inch touchscreen fitted, the touchscreen should be activated.  You will need to log in to the console to set up any other displays or advanced options.

Note that you do not need to load any touchscreen drivers - if the touchscreen does not work try powering off and on again.

After initial installation, on selecting transmit, the RPi is configured to generate a direct RF output (from GPIO pin 32) on 437 MHz at 333KS using the BATC Portsdown logo image as the source.  

Advanced notes:  To load the development version, cut and paste in the following lines:

```sh
wget https://raw.githubusercontent.com/davecrump/portsdown/master/install.sh
chmod +x install.sh
./install.sh -d
```
