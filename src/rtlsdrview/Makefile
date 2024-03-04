all:	rtlsdrview

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I.. 
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread `pkg-config --libs librtlsdr` -ljpeg -lrt -lm -ldl -lfftw3 -lusb-1.0 -lwiringPi -lfcgi  -rdynamic 

rtlsdrview: rtlsdrview.c 
	gcc -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS -pthread $(INCLUDEFLAGS2) -o  rtlsdrview \
		screen.c \
		graphics.c \
		rtlsdrfft.c \
		timing.c \
		ffunc.c \
		ffunc.h \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_32.c \
		rtlsdrview.c $(LIBFLAGS2)

install:
	cp rtlsdrview ../../bin/

