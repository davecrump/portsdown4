EXE = beacon
EXE_SERVER = server
CC = gcc 
LD = gcc 
CFLAGS = -fPIC -Wall
INCS = 
LINKFLAGS =
LIB_OBJS = sdr_interface.o transport.o websocket_server.o protocol.o
LIB_OBJS_SERVER = tcp_server.o audio.o
ALL_EXE = beacon server
all: $(ALL_EXE)
$(EXE): $(LIB_OBJS)
	@$(LD) $(LINKFLAGS) -o $(EXE) $(LIB_OBJS) -lc -lm -lsdrplay_api -lwebsockets
$(EXE_SERVER): $(LIB_OBJS_SERVER)
	@$(LD) $(LINKFLAGS) -o $(EXE_SERVER) $(LIB_OBJS_SERVER) -lasound
%.o: %.c
	@echo "Compiling" $<
	@$(CC) $(CFLAGS) $(INCS) -c $<
clean: 
	rm *.o
	rm $(ALL_EXE)
