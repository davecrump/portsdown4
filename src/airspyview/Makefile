# ========================================================================================
# Makefile for airspyview
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
CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	\
		-D BUILD_DATE="\"$(shell date '+%Y-%m-%d_%H:%M:%S')\""

BIN = airspyview

# ========================================================================================
# Source files

SRCDIR = .

SRC = 	$(SRCDIR)/libairspy/libairspy/src/*.c \
		$(SRCDIR)/airspyview.c\
		$(SRCDIR)/screen.c \
		$(SRCDIR)/graphics.c \
		$(SRCDIR)/airspyfft.c \
		$(SRCDIR)/timing.c \
		$(SRCDIR)/temperature.c \
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

LIBS = -lm -lbcm_host -pthread `pkg-config --libs libairspy` -lusb-1.0 -lfftw3 -lwiringPi -Wl,-Bstatic -Wl,-Bdynamic
LIBS += -L/opt/vc/lib 

CFLAGS += `pkg-config --cflags libairspy`
#
# ========================================================================================
# Makerules

all:
	@pkg-config --modversion "libairspy = 1.0"
	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

#	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) -I $(LIBSDIR) -L $(OBSDIR) $(LIBS)

debug: COPT = -Og -ggdb -fno-omit-frame-pointer -D__DEBUG
debug: all

clean:
	rm -fv $(BIN)

