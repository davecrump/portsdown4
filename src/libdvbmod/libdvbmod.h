#ifndef LIBDVBMOD_H_
#define LIBDVBMOD_H_
//#define WITH_ARM
#include <stdint.h>
// ********************** S2 MODULATOR 
// Code rates
#define CR_1_4 0
#define CR_1_3 1
#define CR_2_5 2
#define CR_1_2 3
#define CR_3_5 4
#define CR_2_3 5
#define CR_3_4 6
#define CR_4_5 7
#define CR_5_6 8
#define CR_8_9 9
#define CR_7_8 10
#define CR_9_10 11

// Constellation
#define M_QPSK   0
#define M_8PSK   1
#define M_16APSK 2
#define M_32APSK 3

// Rolloff
#define RO_0_35 0
#define RO_0_25 1
#define RO_0_20 2
#define RO_RESERVED 3

typedef struct {
	short re;
	short im;
}sfcmplx;

extern int Dvbs2Init(int SRate,int CodeRate,int Constellation,int PilotesOn,int RollOff,int Upsample=1);
extern int Dvbs2AddTsPacket(uint8_t *Packet);
extern sfcmplx *Dvbs2_get_IQ(void);

extern int DvbsInit(int SRate, int CodeRate, int Constellation = M_QPSK,int Upsample=1);
extern int DvbsAddTsPacket(uint8_t *Packet);
extern sfcmplx *Dvbs_get_IQ(void);
#ifdef WITH_ARM
#ifdef __arm__
extern int DvbsInit(int SRate, int CodeRate);
extern int DvbsAddTsPacket(uint8_t *Packet);
extern sfcmplx *Dvbs_get_IQ(void);
#endif
#endif
#endif
