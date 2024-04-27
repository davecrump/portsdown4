all:	pluto_noise_meter

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread -ljpeg -lrt -lm -ldl -liio -lfftw3f -lwiringPi -lLimeSuite -lfcgi -rdynamic

pluto_noise_meter: pluto_noise_meter.c 
	gcc -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS -pthread $(INCLUDEFLAGS2) -o  pluto_noise_meter \
		screen.c \
		graphics.c \
		pluto.c \
		fft.c \
		timing.c \
		temperature.c \
		ffunc.c \
		ffunc.h \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_32.c \
		font/dejavu_sans_72.c \
		pluto_noise_meter.c $(LIBFLAGS2)

install:
	cp pluto_noise_meter ../../bin/
