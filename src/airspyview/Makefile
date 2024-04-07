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

CFLAGS = -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS `pkg-config --cflags libairspy`

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
		$(SRCDIR)/ffunc.c \
		$(SRCDIR)/ffunc.h \
		$(SRCDIR)/temperature.c \
		$(SRCDIR)/font/font.c \
		$(SRCDIR)/font/dejavu_sans_18.c \
		$(SRCDIR)/font/dejavu_sans_20.c \
		$(SRCDIR)/font/dejavu_sans_22.c \
		$(SRCDIR)/font/dejavu_sans_28.c \
		$(SRCDIR)/font/dejavu_sans_32.c 

# ========================================================================================
# External Libraries


LIBS = -Wl,--copy-dt-needed-entries -lm -lbcm_host -pthread `pkg-config --libs libairspy` -lusb-1.0 -lfftw3 -lwiringPi -Wl,-Bstatic -Wl,-Bdynamic -L/opt/vc/lib -lfcgi -rdynamic

#
# ========================================================================================
# Makerules

all:
	@pkg-config --modversion "libairspy = 1.0"
	$(CC) $(COPT) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)



