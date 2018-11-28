#  Makefile template for Static library. 
# 1. Compile every *.cpp in the folder 
# 2. All obj files under obj folder
# 3. static library .a at lib folder
# 4. run 'make dirmake' before calling 'make'


CC = g++
OUT_FILE_NAME = libdvbmod.a

CFLAGS= -fPIC -O2 -g -Wall -c -fpermissive
LDFLAGS = -lm -lrt -lpthread 

INC = -I../Import

OBJ_DIR=./obj

OUT_DIR=./lib

# Enumerating of every *.cpp as *.o and using that as dependency.	
# filter list of .c files in a directory.
# FILES =dump_l.c \
#	kter.c \
#
# $(OUT_FILE_NAME): $(patsubst %.c,$(OBJ_DIR)/%.o,$(wildcard $(FILES))) 


# Enumerating of every *.cpp as *.o and using that as dependency
$(OUT_FILE_NAME):\
$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard *.cpp))\
$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard DVB-S/*.cpp)) \
$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard DVB-T/*.cpp)) \
$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard DVB-S2/*.cpp))
	ar -r -o $(OUT_DIR)/$@ $^



#Compiling every *.cpp to *.o
$(OBJ_DIR)/%.o: %.cpp dirmake
	$(CC) -c $(INC) $(CFLAGS) -o $@  $<
	
dirmake:
	@mkdir -p $(OUT_DIR)
	@mkdir -p $(OBJ_DIR)
	
	@mkdir -p $(OBJ_DIR)/DVB-S
	@mkdir -p $(OBJ_DIR)/DVB-S2
	@mkdir -p $(OBJ_DIR)/DVB-T
	
clean:
	rm -f $(OBJ_DIR)/*.o $(OUT_DIR)/$(OUT_FILE_NAME) Makefile.bak
	rm -f $(OBJ_DIR)/DVB-S/*.o
	rm -f $(OBJ_DIR)/DVB-S2/*.o
	rm -f $(OBJ_DIR)/DVB-T/*.o
	
rebuild: clean build
