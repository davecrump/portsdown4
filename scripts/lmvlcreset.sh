#!/bin/bash

# Called (as a forked thread) to stop VLC and then restart it
# Only works for a UDP stream otherwise longmynd's output fifo gets blocked

# The -i 1 parameter to put a second between commands is required to ensre reset at low SRs

nc -i 1 127.0.0.1 1111 >/dev/null 2>/dev/null << EOF
stop
next
logout
EOF


exit





