#ifndef _LMRX_UTILS_H__
#define _LMRX_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <fftw3.h>
#include <getopt.h>
#include <linux/input.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <math.h>
#include <wiringPi.h>
#include <sys/stat.h> 
#include <sys/types.h> 

int CalcInputPwr(uint16_t AGC1, uint16_t AGC2);

#endif /* _LMRX_UTILS_H__ */
