all:	bandview

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread -ljpeg -lrt -lm -ldl -lfftw3f -lLimeSuite -lfcgi  -rdynamic

bandview: bandview.c 
	gcc -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS -pthread $(INCLUDEFLAGS2) -o  bandview \
		screen.c \
		graphics.c \
		lime.c \
		fft.c \
		timing.c \
		ffunc.c \
		ffunc.h \
		temperature.c \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_32.c \
		buffer/buffer_circular.c \
		bandview.c $(LIBFLAGS2)

install:
	cp bandview ../../bin/

