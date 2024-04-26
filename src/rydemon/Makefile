all:	rydemon

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread -ljpeg -lrt -lm -ldl -lfcgi -rdynamic

rydemon: rydemon.c 
	gcc -Wall -std=gnu11 -D_GNU_SOURCE -DNEON_OPTS -pthread $(INCLUDEFLAGS2) -o  rydemon \
		ffunc.c \
		ffunc.h \
		rydemon.c $(LIBFLAGS2)

install:
	cp rydemon ../../bin/
