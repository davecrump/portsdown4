#! /bin/bash

# Script to list clients on LAN
# needs apt-get install arp-scan

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
########################################################################

PCONFIGFILE="/home/pi/rpidatv/scripts/portsdown_config.txt"

# Check 3 SUBNETS on 6 network interfaces

SUBNET_SERIAL=1
SUBNET_1="null"
SUBNET_2="null"
SUBNET_3="null"

# Check first entry
SUBNET_TEXT=$(hostname -I | awk '{print $1}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then   # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  SUBNET_1="$SUBNET_TEXT""0/24"
  echo $(hostname -I | awk '{print $1}') This Raspberry Pi
  SUBNET_SERIAL=2
fi

# Check second entry
SUBNET_TEXT=$(hostname -I | awk '{print $2}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then                    # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  if [ $SUBNET_SERIAL == 1 ]; then      # First hit
    SUBNET_1="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $2}') This Raspberry Pi
    SUBNET_SERIAL=2
  fi
  if [ $SUBNET_SERIAL == 2 ]; then      # Second hit
    SUBNET_2="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $2}') This Raspberry Pi
    SUBNET_SERIAL=3
  fi
fi

# Check third entry
SUBNET_TEXT=$(hostname -I | awk '{print $3}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then                    # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  if [ $SUBNET_SERIAL == 1 ]; then      # First hit
    SUBNET_1="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $3}') This Raspberry Pi
    SUBNET_SERIAL=2
  fi
  if [ $SUBNET_SERIAL == 2 ]; then      # Second hit
    SUBNET_2="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $3}') This Raspberry Pi
    SUBNET_SERIAL=3
  fi
  if [ $SUBNET_SERIAL == 3 ]; then      # Third hit
    SUBNET_3="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $3}') This Raspberry Pi
    SUBNET_SERIAL=4
  fi
fi

# Check fourth entry
SUBNET_TEXT=$(hostname -I | awk '{print $4}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then                    # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  if [ $SUBNET_SERIAL == 1 ]; then      # First hit
    SUBNET_1="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $4}') This Raspberry Pi
    SUBNET_SERIAL=2
  fi
  if [ $SUBNET_SERIAL == 2 ]; then      # Second hit
    SUBNET_2="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $4}') This Raspberry Pi
    SUBNET_SERIAL=3
  fi
  if [ $SUBNET_SERIAL == 3 ]; then      # Third hit
    SUBNET_3="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $4}') This Raspberry Pi
    SUBNET_SERIAL=4
  fi
fi

# Check fifth entry
SUBNET_TEXT=$(hostname -I | awk '{print $5}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then                    # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  if [ $SUBNET_SERIAL == 1 ]; then      # First hit
    SUBNET_1="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $5}') This Raspberry Pi
    SUBNET_SERIAL=2
  fi
  if [ $SUBNET_SERIAL == 2 ]; then      # Second hit
    SUBNET_2="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $5}') This Raspberry Pi
    SUBNET_SERIAL=3
  fi
  if [ $SUBNET_SERIAL == 3 ]; then      # Third hit
    SUBNET_3="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $5}') This Raspberry Pi
    SUBNET_SERIAL=4
  fi
fi

# Check sixth entry
SUBNET_TEXT=$(hostname -I | awk '{print $6}')
echo $SUBNET_TEXT | grep -E -q "192.168."
if [ $? == 0 ]; then                    # It's a 192.168.*.* network
  SUBNET_TEXT=$(echo $SUBNET_TEXT | sed 's![^.]*$!!')
  if [ $SUBNET_SERIAL == 1 ]; then      # First hit
    SUBNET_1="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $6}') This Raspberry Pi
    SUBNET_SERIAL=2
  fi
  if [ $SUBNET_SERIAL == 2 ]; then      # Second hit
    SUBNET_2="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $6}') This Raspberry Pi
    SUBNET_SERIAL=3
  fi
  if [ $SUBNET_SERIAL == 3 ]; then      # Third hit
    SUBNET_3="$SUBNET_TEXT""0/24"
    echo $(hostname -I | awk '{print $6}') This Raspberry Pi
    SUBNET_SERIAL=4
  fi
fi

# Check the Pluto
PLUTOIP=$(get_config_var plutoip $PCONFIGFILE)
ping -c 1 -w 1 $PLUTOIP >/dev/null 2>/dev/null
if [ $? == 0 ]; then
  echo $PLUTOIP Configured address for Pluto SDR
fi

# Check the arp entries
if [ "$SUBNET_1" != "null" ]; then
  sudo arp-scan $SUBNET_1 | while read line
  do \
  ( echo $line | grep -E -q "192.168."
    if [ $? == 0 ]; then
      echo $line | grep -E -q "28:cd:c1:|2c:cf:67:|b8:27:eb:|d8:3a:dd:|dc:a6:32|e4:5f:01:"
      if [ $? == 0 ]; then
        corrected_line=$(echo $line | sed 's/(Unknown)/Raspberry Pi/g')
        echo $corrected_line
      else
        echo $line
      fi
    fi
   )
  done
fi

if [ "$SUBNET_2" != "null" ]; then
  sudo arp-scan $SUBNET_2 | while read line
  do \
  ( echo $line | grep -E -q "192.168."
    if [ $? == 0 ]; then
      echo $line | grep -E -q "28:cd:c1:|2c:cf:67:|b8:27:eb:|d8:3a:dd:|dc:a6:32|e4:5f:01:"
      if [ $? == 0 ]; then
        corrected_line=$(echo $line | sed 's/(Unknown)/Raspberry Pi/g')
        echo $corrected_line
      else
        echo $line
      fi
    fi
   )
  done
fi

if [ "$SUBNET_3" != "null" ]; then
  sudo arp-scan $SUBNET_3 | while read line
  do \
  ( echo $line | grep -E -q "192.168."
    if [ $? == 0 ]; then
      echo $line | grep -E -q "28:cd:c1:|2c:cf:67:|b8:27:eb:|d8:3a:dd:|dc:a6:32|e4:5f:01:"
      if [ $? == 0 ]; then
        corrected_line=$(echo $line | sed 's/(Unknown)/Raspberry Pi/g')
        echo $corrected_line
      else
        echo $line
      fi
    fi
   )
  done
fi

