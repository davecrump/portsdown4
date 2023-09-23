# ========================================================================================
# Makefile for sdrplayview
# ========================================================================================

# ========================================================================================
# Compile flags

ifneq (, $(shell which gcc-8))
  CC = gcc-8
else
  CC = gcc
endif

COPT = -O3 -march=native -mtune=native
# CFLAGS = -Wall -Wextra -Wpedantic -std=gnu11 -D_GNU_SOURCE -pthread 
CFLAGS = -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS 


BIN = sdrplayview

# ========================================================================================
# Source files

SRCDIR = .

SRC =	$(SRCDIR)/sdrplayview.c\
		$(SRCDIR)/screen.c \
		$(SRCDIR)/graphics.c \
		$(SRCDIR)/sdrplayfft.c \
		$(SRCDIR)/timing.c \
		$(SRCDIR)/ffunc.c \
		$(SRCDIR)/ffunc.h \
		$(SRCDIR)/font/font.c \
		$(SRCDIR)/font/dejavu_sans_18.c \
		$(SRCDIR)/font/dejavu_sans_20.c \
		$(SRCDIR)/font/dejavu_sans_22.c \
		$(SRCDIR)/font/dejavu_sans_28.c \
		$(SRCDIR)/font/dejavu_sans_32.c 

# ========================================================================================
# External Libraries

# LIBSDIR = libwebsockets/build/include
# OBSDIR = libwebsockets/build/lib

LIBS = -lm -lc -lsdrplay_api -lbcm_host -pthread -lusb-1.0 -lfftw3 -lwiringPi -Wl,-Bstatic -Wl,-Bdynamic
LIBS += -L/opt/vc/lib -lrt -ldl -lfcgi -rdynamic  


#
# ========================================================================================
# Makerules

all:
	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

#	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) -I $(LIBSDIR) -L $(OBSDIR) $(LIBS)

debug: COPT = -Og -ggdb -fno-omit-frame-pointer -D__DEBUG
debug: all

clean:
	rm -fv $(BIN)

