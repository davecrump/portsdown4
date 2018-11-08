all:	limetx limerx limerx2tx

#CFLAGS	= -Wall -g -O2 -D DIGITHIN
CFLAGS	= -Wall -g -O2 -Wno-unused-variable -Wno-unused-function 
LDFLAGS	= -lm -lrt -lpthread -lLimeSuite

limetx : limetx.c
		$(CC) $(CFLAGS) -o limetx limetx.c $(LDFLAGS) 

limerx : limerx.c
		$(CC) $(CFLAGS) -o limerx limerx.c $(LDFLAGS) 

limerx2tx : limerx2tx.c
		$(CC) $(CFLAGS) -o limerx2tx limerx2tx.c $(LDFLAGS) 


clean:

	rm -f limetx limerx limerx2tx *.o 
