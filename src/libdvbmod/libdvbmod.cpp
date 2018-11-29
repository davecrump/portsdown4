#include "libdvbmod.h"
#include "./DVB-S2/DVBS2.h"
#include "./DVB-S/dvbs.h"
#include "math.h"
#include <stdlib.h> 


#define UPSAMPLE_MAX 4
size_t m_upsample=1;
float CoefFec[12] = { 1 / 4.0,1 / 3.0,2 / 5.0,1 / 2.0,3 / 5.0,2 / 3.0,3 / 4.0,4 / 5.0,5 / 6.0,8 / 9.0,7 / 8.0,9 / 10.0 };
DVBS2 DvbS2Modulator;

size_t DVBS2Length=0;
static sfcmplx *dvbs_symbols_short;

int Dvbs2Init(int SRate,int CodeRate,int Constellation,int PilotesOn,int RollOff,int Upsample)
{
	DVB2FrameFormat S2Format;
	S2Format.frame_type = FRAME_NORMAL;
	S2Format.code_rate = CodeRate;
	S2Format.constellation = Constellation;
	S2Format.pilots = PilotesOn;
	S2Format.broadcasting = 1;
	S2Format.roll_off = RollOff;
	S2Format.null_deletion = 0;
	S2Format.dummy_frame = 0;
	DvbS2Modulator.s2_set_configure(&S2Format);
	m_upsample=Upsample;
	dvbs_symbols_short=(sfcmplx*)malloc(FRAME_SIZE_NORMAL*m_upsample*sizeof(sfcmplx));
	return (int)(SRate*DvbS2Modulator.s2_get_efficiency());
}

int Dvbs2AddTsPacket(uint8_t *Packet)
{
	DVBS2Length=DvbS2Modulator.s2_add_ts_frame(Packet);
	return DVBS2Length*m_upsample;
	
}

sfcmplx *Dvbs2_get_IQ(void)
{
	if(m_upsample>1)
	{
		static sfcmplx Zero;
		Zero.im=0;
		Zero.re=0;
		sfcmplx *FrameS2IQ=(sfcmplx*)DvbS2Modulator.pl_get_frame();
		for(size_t i=0;i<DVBS2Length;i++)
		{
			for(size_t j=0;j<m_upsample;j++)
				dvbs_symbols_short[i*m_upsample+j]=(j==0)?FrameS2IQ[i]:Zero;
		}
		return dvbs_symbols_short;
	}
	else
	{	
		
		return (sfcmplx*)DvbS2Modulator.pl_get_frame();

	}
	
}

//******************* DVB-S **********************************
#define SMAG 0x3FFF

sfcmplx m_qpsk[4];
sfcmplx m_8psk[8];

void InitConstellation(void)
{
	#define CP 0x7FFF
	double r0, r1, r2, r3;
	double m = 1.0;
	r0 = 0.9;// I am not sure why this needs to be 0.9 but 32APSK does not work if == 1.0
	r1 = m;

	// QPSK
	m_qpsk[0].re = (short)((r1*cos(M_PI / 4.0))*CP);
	m_qpsk[0].im = (short)((r1*sin(M_PI / 4.0))*CP);
	m_qpsk[1].re = (short)((r1*cos(7 * M_PI / 4.0))*CP);
	m_qpsk[1].im = (short)((r1*sin(7 * M_PI / 4.0))*CP);
	m_qpsk[2].re = (short)((r1*cos(3 * M_PI / 4.0))*CP);
	m_qpsk[2].im = (short)((r1*sin(3 * M_PI / 4.0))*CP);
	m_qpsk[3].re = (short)((r1*cos(5 * M_PI / 4.0))*CP);
	m_qpsk[3].im = (short)((r1*sin(5 * M_PI / 4.0))*CP);

	//8PSK
	m_8psk[0].re = (short)((r1*cos(M_PI / 4.0))*CP);
	m_8psk[0].im = (short)((r1*sin(M_PI / 4.0))*CP);
	m_8psk[1].re = (short)((r1*cos(0.0))*CP);
	m_8psk[1].im = (short)((r1*sin(0.0))*CP);
	m_8psk[2].re = (short)((r1*cos(4 * M_PI / 4.0))*CP);
	m_8psk[2].im = (short)((r1*sin(4 * M_PI / 4.0))*CP);
	m_8psk[3].re = (short)((r1*cos(5 * M_PI / 4.0))*CP);
	m_8psk[3].im = (short)((r1*sin(5 * M_PI / 4.0))*CP);
	m_8psk[4].re = (short)((r1*cos(2 * M_PI / 4.0))*CP);
	m_8psk[4].im = (short)((r1*sin(2 * M_PI / 4.0))*CP);
	m_8psk[5].re = (short)((r1*cos(7 * M_PI / 4.0))*CP);
	m_8psk[5].im = (short)((r1*sin(7 * M_PI / 4.0))*CP);
	m_8psk[6].re = (short)((r1*cos(3 * M_PI / 4.0))*CP);
	m_8psk[6].im = (short)((r1*sin(3 * M_PI / 4.0))*CP);
	m_8psk[7].re = (short)((r1*cos(6 * M_PI / 4.0))*CP);
	m_8psk[7].im = (short)((r1*sin(6 * M_PI / 4.0))*CP);
}

static uint8_t dvbs_dibit[DVBS_RS_BLOCK*16];
static sfcmplx dvbs_symbol[DVBS_RS_BLOCK*16*4];




static uint8_t InterMedBuffer[DVBS_RS_BLOCK * 16 * 4];


static int LenFrame = 0;
static int m_Constellation = M_QPSK;
int ConstellationEffiency[] = { 2,3,4,5 }; //Nb Bit per Symbol


int DvbsInit(int SRate, int CodeRate, int Constellation ,int Upsample)
{
	InitConstellation ();
	m_Constellation = Constellation;
	dvb_encode_init(CodeRate);
	int NetBitrate = SRate * 188 * CoefFec[CodeRate] / 204 * ConstellationEffiency[Constellation];
	m_upsample=Upsample;
	dvbs_symbols_short=(sfcmplx*)malloc(DVBS_RS_BLOCK*16*m_upsample*sizeof(sfcmplx));
	return NetBitrate;
}

int DvbsAddTsPacket(uint8_t *Packet)
{
	
	switch (m_Constellation)
	{
		case M_QPSK:LenFrame = dvb_encode_frame(Packet, dvbs_dibit); break;
		case M_8PSK://EXPERIMENTAL DVBS
		{
			LenFrame += dvb_encode_frame(Packet, InterMedBuffer + LenFrame);
			if (LenFrame % 3 == 0) return (LenFrame*2/3*m_upsample); else return 0;
		}
		break;
	}
	return LenFrame*m_upsample;
}

sfcmplx *Dvbs_get_IQ(void)
{
	int psklen = 0;
	static sfcmplx Zero;
	Zero.im=0;
	Zero.re=0;
	switch (m_Constellation)
	{
		case M_QPSK:
		{
			for (size_t i = 0; i < (size_t)LenFrame; i++)
			{
				for(size_t j=0;j<m_upsample;j++)
				{
					dvbs_symbols_short[i*m_upsample+j] =(j==0)?m_qpsk[dvbs_dibit[i]]:Zero;
					psklen++;
				}
			}
			
		}
		break;
		case M_8PSK:
		{
			
			for (int i = 0; i < LenFrame; i += 3)
			{
				dvbs_symbol[psklen++] = m_8psk[(InterMedBuffer[i] << 1) + ((InterMedBuffer[i + 1] & 2) >> 1)];
				dvbs_symbol[psklen++] = m_8psk[((InterMedBuffer[i + 1] & 1) << 2) + ((InterMedBuffer[i + 2]))];
			}
			
		}
		break;
	}
		LenFrame = 0;
		return dvbs_symbols_short;
}



// **************** DVB-S ARM **********************************
#ifdef WITH_ARM



extern void viterbi_init(int SetFEC);
extern uint16_t viterbi(uint8_t *in, uint8_t *out);
extern void 	dvbsenco_init(void);
extern uint8_t*	dvbsenco(uint8_t*);
uint8_t BuffIQ[204 * 2]; 
int NbIQOutput = 0;
sfcmplx sBuffIQ[204 * 8];


int DvbsInit(int SRate, int CodeRate)
{
	dvbsenco_init();
	viterbi_init(CodeRate);
	
	return 0;
}


int DvbsAddTsPacket(uint8_t *Packet)
{
	uint8_t *p = dvbsenco(Packet);
	NbIQOutput = viterbi(p, BuffIQ);
	return NbIQOutput;
}

extern sfcmplx *Dvbs_get_IQ(void)
{
	for (int i = 0; i < NbIQOutput;i++)
	{
		for (int j = 0; j < 4; j += 2)
		{
			sBuffIQ[i * 4 + j] = tx_lookup[((BuffIQ[i] >> j) & 0x3)];
		}
	}
}
#endif
