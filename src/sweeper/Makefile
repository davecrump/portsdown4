all:	sweeper

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread  -ljpeg -lrt -lm -ldl -lfftw3f -lwiringPi -lLimeSuite -lfcgi -rdynamic

sweeper: mcp3002.h mcp3002.c sweeper.c 
	gcc -Wall -std=gnu11 -D_GNU_SOURCE $(INCLUDEFLAGS2) -o  sweeper \
		ffunc.c \
		ffunc.h \
		timing.c \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_36.c \
		sweeper.c $(LIBFLAGS2) 

install:
	cp sweeper ../../bin/