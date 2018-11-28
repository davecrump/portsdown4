all:	dvb2iq 

CC = g++
CFLAGS	= -Wall -g -O2  
LDFLAGS	= -lm -lrt -lfftw3 -lpthread 

.cpp.o:
	$(CC) $(CFLAGS) -c $<

dvb2iq : DvbTsToIQ.o ../libdvbmod/lib/libdvbmod.a 
		$(CC) $(CFLAGS) $(LDFLAGS) -o dvb2iq  DvbTsToIQ.o ../libdvbmod/lib/libdvbmod.a 

clean:
	rm -f *.o dvb2iq