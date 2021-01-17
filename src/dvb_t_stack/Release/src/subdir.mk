################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/dvb_conv.cpp \
../src/dvb_encode.cpp \
../src/dvb_interleave.cpp \
../src/dvb_rs_encoder.cpp \
../src/dvb_t.cpp \
../src/dvb_t_bits.cpp \
../src/dvb_t_enc.cpp \
../src/dvb_t_i.cpp \
../src/dvb_t_linux_fft.cpp \
../src/dvb_t_lpf.cpp \
../src/dvb_t_mod.cpp \
../src/dvb_t_qam_tab.cpp \
../src/dvb_t_stab.cpp \
../src/dvb_t_stack.cpp \
../src/dvb_t_sym.cpp \
../src/dvb_t_tp.cpp \
../src/dvb_t_udp.cpp \
../src/dvbt_modulator.cpp \
../src/express.cpp \
../src/lime.cpp \
../src/mmi.cpp \
../src/pluto.cpp 

OBJS += \
./src/dvb_conv.o \
./src/dvb_encode.o \
./src/dvb_interleave.o \
./src/dvb_rs_encoder.o \
./src/dvb_t.o \
./src/dvb_t_bits.o \
./src/dvb_t_enc.o \
./src/dvb_t_i.o \
./src/dvb_t_linux_fft.o \
./src/dvb_t_lpf.o \
./src/dvb_t_mod.o \
./src/dvb_t_qam_tab.o \
./src/dvb_t_stab.o \
./src/dvb_t_stack.o \
./src/dvb_t_sym.o \
./src/dvb_t_tp.o \
./src/dvb_t_udp.o \
./src/dvbt_modulator.o \
./src/express.o \
./src/lime.o \
./src/mmi.o \
./src/pluto.o 

CPP_DEPS += \
./src/dvb_conv.d \
./src/dvb_encode.d \
./src/dvb_interleave.d \
./src/dvb_rs_encoder.d \
./src/dvb_t.d \
./src/dvb_t_bits.d \
./src/dvb_t_enc.d \
./src/dvb_t_i.d \
./src/dvb_t_linux_fft.d \
./src/dvb_t_lpf.d \
./src/dvb_t_mod.d \
./src/dvb_t_qam_tab.d \
./src/dvb_t_stab.d \
./src/dvb_t_stack.d \
./src/dvb_t_sym.d \
./src/dvb_t_tp.d \
./src/dvb_t_udp.d \
./src/dvbt_modulator.d \
./src/express.d \
./src/lime.d \
./src/mmi.d \
./src/pluto.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -include/usr/local/include/lime/LimeSuite.h -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


