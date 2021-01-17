#include <math.h>
#include "memory.h"
#include "dvb_t.h"

static int m_tx_samples;
static scmplx m_sams[M64KS+M16KS];

#define CLIP_TH 0.707f
//
// Input IQ samples float,
// length is the number of complex samples
// Output complex samples short
//
void dvb_t_clip( fftwf_complex *in, int length )
{
    for( int i = 0; i < length; i++)
    {
        if(fabs(in[i][0]) > CLIP_TH)
        {
           if(in[i][0] > 0 )
                in[i][0] = CLIP_TH;
            else
                in[i][0] = -CLIP_TH;
        }
        if(fabs(in[i][1]) > CLIP_TH)
        {
            if(in[i][1] > 0 )
                in[i][1] = CLIP_TH;
            else
                in[i][1] = -CLIP_TH;
        }
    }
}
scmplx *dvb_t_get_samples(void) {
	return m_sams;
}

extern void(*m_func_tx)(scmplx	*s, int len);


void dvb_t_final_modulate(fftwf_complex *in, int length, int guard)
{
	// Clip the waveform
	dvb_t_clip( in, length);

	//Add the guard prefix and convert to 16 bit fixed point

	fftwf_complex *p = &in[length - guard];
	m_tx_samples = 0;

	for (int i = 0; i < guard; i++)
	{
		m_sams[m_tx_samples].re = (short)(p[i][0] * 0x7FFF);
		m_sams[m_tx_samples].im = (short)(p[i][1] * 0x7FFF);
		m_tx_samples++;
	}

	// Add the main symbol
	for (int i = 0; i < length; i++)
	{
		m_sams[m_tx_samples].re = (short)(in[i][0] * 0x7FFF);
		m_sams[m_tx_samples].im = (short)(in[i][1] * 0x7FFF);
		m_tx_samples++;
	}

	// filter out spill on either side of symbol
	dvb_t_filter(m_sams, m_tx_samples);

	// The samples are now ready
	if (m_func_tx != NULL) {
		m_func_tx(m_sams, m_tx_samples);
		m_tx_samples = 0;
	}
}
//
// This module constructs the final symbol for transmission.
// It applies clipping
// Adds the cyclic prefix (guard period) and main body in 16 bit fixed format
// Filters the signals
//
void dvb_t_final_modulate(fftwf_complex *in, float *taper, int length, int guard)
{
	// Clip the waveform
	dvb_t_clip(in, length);

	// Convert to 16 bit fixed point

	fftwf_complex *p = &in[length - guard];
	m_tx_samples = 0;

	// Add guard prefix
	for (int i = 0; i < guard; i++)
	{
		m_sams[m_tx_samples].re = (short)(p[i][0] * 0x7FFF * taper[m_tx_samples]);
		m_sams[m_tx_samples].im = (short)(p[i][1] * 0x7FFF * taper[m_tx_samples]);
		m_tx_samples++;
	}
	// Add main symbol
	for (int i = 0; i < length; i++)
	{
		m_sams[m_tx_samples].re = (short)(in[i][0] * 0x7FFF * taper[m_tx_samples]);
		m_sams[m_tx_samples].im = (short)(in[i][1] * 0x7FFF * taper[m_tx_samples]);
		m_tx_samples++;
	}
	// filter out spill on either side of symbol
	dvb_t_filter(m_sams, m_tx_samples);
	// The samples are now ready
}

void dvb_t_modulate_init(void)
{
}
