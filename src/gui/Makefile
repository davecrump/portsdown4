FFT_C = ./hello_fft/mailbox.c ./hello_fft/gpu_fft.c ./hello_fft/gpu_fft_base.c ./hello_fft/gpu_fft_twiddles.c ./hello_fft/gpu_fft_shaders.c
all:	touchdemo rpidatvtouch4

clean:
	rm -f *.o *.inc *.so *.c~ *.h~
	indent -linux -c 60 -brf -l 132

INCLUDEFLAGS2=-lpthread -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I..
LIBFLAGS2=-L/opt/vc/lib -lbcm_host -lpthread  -ljpeg -lrt -lm -ldl -lfftw3f -lwiringPi

touchdemo : Font.h Graphics.h Touch.h touchdemo.c
	gcc -Wall $(INCLUDEFLAGS2) -o  touchdemo touchdemo.c $(LIBFLAGS2) 

rpidatvtouch4: rpidatvtouch4.c 
	gcc -Wall $(INCLUDEFLAGS2) -o  rpidatvgui rpidatvtouch4.c $(LIBFLAGS2) 

install:
	cp rpidatvgui ../../bin/
