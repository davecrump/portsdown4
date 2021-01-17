#ifndef __DVB_T_H__
#define __DVB_T_H__
#include <stdint.h>

#define __STDC_CONSTANT_MACROS

#define __USE_LIME__
#define __USE_PLUTO__
#define __USE_EXPRESS__

#include "fftw3.h"

typedef struct{
	float re;
	float im;
}fcmplx;

typedef struct {
	short re;
	short im;
}scmplx;

#define FLOAT float
typedef enum{FEC_12,FEC_23,FEC_34,FEC_56,FEC_78}Fec;

#define TPS_2K_TAB_LEN 17
#define TPS_8K_TAB_LEN 68
#define PPS_2K_TAB_LEN 45
#define PPS_8K_TAB_LEN 177

#define SYMS_IN_FRAME 68

#ifndef M_PI
#define M_PI 3.14159f
#endif

// Minimum cell nr
#define K2MIN 0
#define K8MIN 0

// Max cell nr
#define K2MAX 1704
#define K8MAX 6816
#define SF_NR 4

// Size of FFT
#define M2KS  2048
#define M4KS  4096
#define M8KS  8192
#define M16KS 16384
#define M32KS 32768
#define M64KS 65536

// Number of data cells
#define M2SI 1512
#define M8SI 6048

// FFT bin number to start at
#define M8KSTART 688
#define M2KSTART 172

#define BIL 126
#define M_QPSK  0
#define M_QAM16 1
#define M_QAM64 2

#define BCH_POLY  0x4377
//#define BCH_RPOLY 0x7761
#define BCH_RPOLY 0x3761

// Definition of mode, these correspond to the values in the TPS
#define FN_1_SP 0
#define FN_2_SP 1
#define FN_3_SP 2
#define FN_4_SP 3

#define CO_QPSK  0
#define CO_16QAM 1
#define CO_64QAM 2

#define SF_NH    0
#define SF_A1    1
#define SF_A2    2
#define SF_A4    3

#define GI_132   0
#define GI_116   1
#define GI_18    2
#define GI_14    3

#define TM_2K    0
#define TM_8K    1

typedef enum{R_PLUTO,R_LIME,R_EXPRESS}RadioType;

typedef struct{
	uint8_t co;// Constellation
	uint8_t sf;// single frequency network
	uint8_t gi;// guard interval
	uint8_t tm;// Transmission mode 2K or 8K
    uint8_t fec;// FEC coderate
	uint8_t ir;// Interpolation ratio
	uint32_t chan_bw_hz;
	uint32_t tx_sample_rate;
	uint32_t chan_capacity;
	char     n_addr[32];
	uint64_t freq;
	float level;
	uint16_t port;
	double sr;
	int br;
	RadioType radio;
}DVBTFormat;

#define AVG_E8 (1.0/320.0)
#define AVG_E2 (1.0/187.0)
//#define AVG_E2 (1.0/150.0)


#define MP_T_SYNC 0x47
#define DVBS_T_ISYNC 0xB8
#define DVBS_T_PAYLOAD_LEN 187
#define MP_T_FRAME_LEN 188
#define DVBS_RS_BLOCK_DATA 239
#define DVBS_RS_BLOCK_PARITY 16
#define DVBS_RS_BLOCK (DVBS_RS_BLOCK_DATA+DVBS_RS_BLOCK_PARITY)
#define DVBS_PARITY_LEN 16
#define DVBS_T_CODED_FRAME_LEN (MP_T_FRAME_LEN+DVBS_PARITY_LEN)
#define DVBS_T_FRAMES_IN_BLOCK 8
#define DVBS_T_BIT_WIDTH 8
#define DVBS_T_SCRAM_SEQ_LENGTH 1503

// Prototypes

// dvb_t_tp.c
void build_tp_block( DVBTFormat *fmt );

// dvb_t_ta.c

// dvb_t_sym.c
void init_reference_frames( void );
void reference_symbol_reset( void );
int reference_symbol_seq_get( void );
int reference_symbol_seq_update( void );

//dvb_t_i.c
void dvb_t_build_p_tables( void );

// dvb_t_enc.c
void init_dvb_t_enc( void );
void dvb_t_enc_config( DVBTFormat *fmt );
void dvb_t_enc_dibit( uint8_t *in, int length );
void dvb_t_encode_and_modulate(uint8_t *in, uint8_t *dibit );

// dvb_t_mod.c
void dvb_t_select_constellation_table( void );
void dvb_t_calculate_guard_period( void );
void dvb_t_modulate(uint8_t *syms );
void dvb_t_write_samples( short *s, int len );
void dvb_t_modulate_init( void );

// dvb_t_linux_fft.c
void dvb_t_fft_open( void );
void dvb_t_fft_close( void );
void dvb_t_fft_modulate( fftw_complex *in, int guard );
void dvb_t_fft_config( DVBTFormat *fmt );

// dvb_t.cpp
void   dvb_t_open( void );
void   dvb_t_close( void );
uint32_t dvb_t_get_sample_rate( void );
double   dvb_t_get_symbol_rate( void );
int dvb_t_get_interpolater(void);
void  dvb_t_encode_and_modulate(uint8_t *tp );
void dvb_t_register_tx(void(*f)(scmplx *s, int len));

// dvb_t_qam_tab.cpp
void build_tx_sym_tabs( DVBTFormat *fm );

// Channel bit rate
uint32_t dvb_t_get_raw_bitrate(void);
uint32_t dvb_t_config_raw_bitrate(DVBTFormat *fmt);

// dvb_t_lpf.cpp
void dvb_t_filter_open(void);
void dvb_t_filter_config(DVBTFormat *fmt);
void dvb_t_filter(scmplx *inout, int len);

// RS functions
void dvb_rs_init(void);
void dvb_rs_encode(uint8_t *inout);

// Interleave
void dvb_interleave_init(void);
void dvb_convolutional_interleave(uint8_t *inout);

// Convolutional coder
//void dvb_conv_ctl(sys_config *info);
void dvb_conv_init(void);
int  dvb_conv_encode_frame(uint8_t *in, uint8_t *out, int len);
void dvb_conv_config(DVBTFormat *fmt);

// Modulators
void dvb_t_fft( fcmplx *in, int guard );
void dvb_t_final_modulate(fftwf_complex *in, int length, int guard);
void dvb_t_final_modulate(fftwf_complex *in, float *taper, int length, int guard);
void dvb_t_clip(fftwf_complex *in, int length);
void dvb_t_modulate_init(void);
void dvb_t_mod_config(DVBTFormat *fmt);
void dvb_t_configure(DVBTFormat *fmt);
scmplx *dvb_t_get_frame(void);
scmplx *dvb_t_get_samples(void);

// dvb.c
void dvb_encode_init(void);
int dvb_encode_frame(uint8_t *tp, uint8_t *dibit);
void dvb_reset_scrambler(void);
void dvb_scramble_transport_packet(uint8_t *in, uint8_t *out);

// UDP interface
int  dvb_t_udp_open(DVBTFormat *fmt);
void dvb_t_udp_close(void);
int  dvb_t_udp_read(uint8_t *b, int len);
int ts_q_read(uint8_t *b, uint32_t len);

// MMI
void cmd_parse(DVBTFormat *fmt, char *cmd);

#endif
