all:	dmm

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread  -ljpeg -lrt -lm -ldl -lwiringPi -lfcgi -rdynamic

dmm: dmm.c 
	gcc -Wall $(INCLUDEFLAGS2) -o  dmm \
		ffunc.c \
		ffunc.h \
		font/font.c \
		font/dejavu_sans_18.c \
		font/dejavu_sans_20.c \
		font/dejavu_sans_22.c \
		font/dejavu_sans_28.c \
		font/dejavu_sans_36.c \
		font/dejavu_sans_72.c \
        timing.c \
		dmm.c $(LIBFLAGS2) 

install:
	cp dmm ../../bin/
