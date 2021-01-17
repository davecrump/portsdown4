// 
// This is the interface module in and out of the dvb_t
// encoder
//
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/types.h>
#include "dvb_t.h"

// Externally visible

DVBTFormat m_fm;

int m_dvbt_initialised;

// Transmit function
void (*m_func_tx)(scmplx *s, int len);

uint32_t dvb_t_get_sample_rate( void )
{
	return m_fm.tx_sample_rate;;
}
void dvb_t_calc_sample_rate( DVBTFormat *fmt )
{
    double srate = 8.0/7.0;

    fmt->tx_sample_rate = (uint32_t)(srate*fmt->ir*fmt->chan_bw_hz);
}

double dvb_t_get_symbol_rate( void )
{
    double symbol_len = 1;//default value
    double srate = dvb_t_get_sample_rate();
    if( m_fm.tm == TM_2K ) symbol_len = M2KS*m_fm.ir;
    if( m_fm.tm == TM_8K ) symbol_len = M8KS*m_fm.ir;
    switch( m_fm.gi )
    {
        case GI_132:
            symbol_len += symbol_len/32;
            break;
        case GI_116:
            symbol_len += symbol_len/16;
            break;
        case GI_18:
            symbol_len += symbol_len/8;
            break;
        case GI_14:
            symbol_len += symbol_len/4;
            break;
        default:
            symbol_len += symbol_len/4;
            break;
    }
    return srate/symbol_len;
}
//
// Called externally to send the next
// transport frame. It encodes the frame
// then calls the modulator if a complete frame is ready
//
uint8_t dibit[DVBS_T_CODED_FRAME_LEN * 8];

void dvb_t_encode_and_modulate(uint8_t *tp)
{
	int len;
	if (m_dvbt_initialised == 0) return;
	len = dvb_encode_frame(tp, dibit);
	dvb_t_enc_dibit(dibit, len);
}

//
// Pass the function pointer to the routine that transmits samples
//
void dvb_t_register_tx(void(*f)(scmplx *s, int len)) {
	m_func_tx = f;
}
//
// Initialise the DVB-T module
//
//
void dvb_t_open( void )
{
    // Encode the correct parameters

	dvb_interleave_init();
	dvb_rs_init();
	dvb_encode_init();
	dvb_conv_init();

    dvb_t_build_p_tables();
	dvb_t_fft_open();
    init_dvb_t_enc();
    init_reference_frames();
	dvb_t_filter_open();

	m_dvbt_initialised = 0;
	m_func_tx = NULL;

}
void dvb_t_configure(DVBTFormat *fmt)
{
	// Encode the correct parameters

	build_tx_sym_tabs( fmt );
    dvb_t_build_p_tables();
	dvb_t_fft_config( fmt );
	dvb_t_enc_config( fmt );
	dvb_conv_config( fmt );
    build_tp_block( fmt );
    init_reference_frames();
	dvb_t_mod_config( fmt );

	dvb_t_filter_config( fmt );
	dvb_t_config_raw_bitrate( fmt );
	dvb_t_calc_sample_rate( fmt );
    m_fm.tx_sample_rate = fmt->tx_sample_rate;
	m_dvbt_initialised = 1;

}
//
// Returns FFT resources
//
void dvb_t_close( void )
{
    dvb_t_fft_close();
}
