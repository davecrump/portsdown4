# Makefile for longmynd

SRC = main.c nim.c ftdi.c stv0910.c stv0910_utils.c stvvglna.c stvvglna_utils.c stv6120.c stv6120_utils.c ftdi_usb.c fifo.c udp.c beep.c ts.c libts.c
OBJ = ${SRC:.c=.o}

ifndef CC
CC = gcc
endif

# Build parallel
MAKEFLAGS += -j$(shell nproc || printf 1)

COPT = -O3 -march=native -mtune=native
# Help detection for ARM SBCs, using devicetree
F_CHECKDTMODEL = $(if $(findstring $(1),$(shell cat /sys/firmware/devicetree/base/model 2>/dev/null)),$(2))
# Jetson Nano is detected correctly
# Raspberry Pi 1 / Zero is detected correctly
DTMODEL_RPI2 = Raspberry Pi 2 Model B
DTMODEL_RPI3 = Raspberry Pi 3 Model B
DTMODEL_RPI4 = Raspberry Pi 4 Model B
COPT_RPI2 = -mfpu=neon-vfpv4
COPT_RPI34 = -mfpu=neon-fp-armv8
COPT += $(call F_CHECKDTMODEL,$(DTMODEL_RPI2),$(COPT_RPI2))
COPT += $(call F_CHECKDTMODEL,$(DTMODEL_RPI3),$(COPT_RPI34))
COPT += $(call F_CHECKDTMODEL,$(DTMODEL_RPI4),$(COPT_RPI34))
# -funsafe-math-optimizations required for NEON, warning: may lead to loss of floating-point precision
COPT += -funsafe-math-optimizations

CFLAGS += -Wall -Wextra -Wpedantic -Wunused -DVERSION=\"${VER}\" -pthread -D_GNU_SOURCE
LDFLAGS += -lusb-1.0 -lm -lasound

all: _print_banner longmynd fake_read ts_analyse

debug: COPT = -Og
debug: CFLAGS += -ggdb -fno-omit-frame-pointer
debug: all

werror: CFLAGS += -Werror
werror: all

_print_banner:
	@echo "Compiling longmynd with GCC $(shell $(CC) -dumpfullversion) on $(shell $(CC) -dumpmachine)"

fake_read: fake_read.c
	@echo "  CC     "$@
	@${CC} fake_read.c -o $@

ts_analyse: ts_analyse.c libts.o
	@echo "  CC     "$@
	@${CC} ts_analyse.c libts.o -o $@

longmynd: ${OBJ}
	@echo "  LD     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c
	@echo "  CC     "$<
	@${CC} ${COPT} ${CFLAGS} -c -fPIC -o $@ $<

clean:
	@rm -rf longmynd fake_read ts_analyse ${OBJ}

.PHONY: all clean

