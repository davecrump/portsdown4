all: set_attenuator

CFLAGS	= -Wall -g -O2 -Wno-unused-variable
LDFLAGS	= -lm -lrt -lpthread  -lwiringPi

set_attenuator : pe4312.h pe4312.c pe43713.h pe43713.c hmc1119.h hmc1119.c set_attenuator.c
		$(CC) $(CFLAGS) -o set_attenuator pe4312.c pe43713.c hmc1119.c set_attenuator.c $(LDFLAGS) 
clean:
	rm -f  *.o




