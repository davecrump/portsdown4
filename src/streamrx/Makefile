## Makefile for streamrx ##
#
# Phil Crump <phil@philcrump.co.uk>
# and Dave Crump <dave.g8gkq@gmail.com>

BIN = streamrx

CC = gcc

CFLAGS = -Wall

CFLAGS-STATIC = -static

SRCS = main.c

LIBS = -lwiringPi -lpthread -lrt

all:
	$(CC) $(CFLAGS) $(SRCS) -o $(BIN) $(LIBS)
static:
	$(CC) $(CFLAGS) $(CFLAGS-STATIC) $(SRCS) -o $(BIN) $(LIBS)
debug:
	$(CC) $(CFLAGS) -g $(SRCS) -o $(BIN) $(LIBS)
clean:
	rm -fv *.o $(BIN)