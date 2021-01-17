#include <math.h>
#include <stdio.h>
#include <memory.h>
#include "dvb_t.h"


static fftwf_complex *m_in, *m_out;
static fftwf_plan m_plan;
static int m_size;
static int (*fft)( fcmplx *in );

//
// Taper table
//
float m_taper[M32KS+(M32KS/4)];
#define MAXFFT M32KS

void create_taper_table(int N, int IR, int GI)
{
    N         = N*IR;// FFT size
    int CP    = N/GI;//Cyclic prefix
    int ALPHA = 32;// Rx Alpha
    int NT    = 2*(N/(ALPHA*2));//Taper length
    int idx   = 0;
    int n     = -NT/2;

    for( int i = 0; i < NT; i++ ){
        m_taper[idx++] = (float)(0.5f*(1.0f+cosf(((float)M_PI*n)/NT)));
        n++;
    }
    for( int i = 0; i < (N+CP)-(2*NT); i++ ){
        m_taper[idx++] = 1.0;
    }
    for( int i = 0; i < NT; i++ ){
        m_taper[idx++] = m_taper[i];
    }
}
//
// sinc correction coefficients
//
static float m_c[M32KS];

void create_correction_table( int N, int IR )
{
    float x,sinc;
    float f = 0.0;
    float fsr = (float)dvb_t_get_sample_rate();
    float fstep = fsr / (N*IR);

    for( int i = 0; i < N/2; i++ )
    {
        if( f == 0 )
            sinc = 1.0f;
        else{
            x = (float)M_PI*f/fsr;
            sinc = sinf(x)/x;
        }
        m_c[i+(N/2)]   = 1.0f/sinc;
        m_c[(N/2)-i-1] = 1.0f/sinc;
//		m_c[i + (N / 2)] = 1.0f;
//		m_c[(N / 2) - i - 1] = 1.0f;
		f += fstep;
    }
}
//
// 2K FFT x1 interpolation
//
static int fft_2k_x1( fcmplx *in )
{

    // Copy the data into the correct bins
    int i,m;
	
	m = (M2KS/2);

    for( i = 0; i < (M2KS); i++ )
    {
		m_in[m][0] = in[i].re*m_c[i];
		m_in[m][1] = in[i].im*m_c[i];
        m = (m+1)%(M2KS);
    }
    fftwf_execute(m_plan);
    return M2KS;
}
//
// 2K FFT x2 Interpolation
//
static int fft_2k_x2( fcmplx *in )
{
    // Zero the unused parts of the array
    //memset(&m_in[M4KS/4],0,sizeof(fftwf_complex)*M4KS/2);

    int i,m;
    m = (M4KS)-(M2KS/2);
    for( i = 0; i < (M2KS); i++ )
    {
        m_in[m][0] = in[i].re*m_c[i];
        m_in[m][1] = in[i].im*m_c[i];
        m = (m+1)%(M4KS);
    }
    fftwf_execute(m_plan);
    return M4KS;
}
//
// 2K FFT x4 interpolation
//
static int fft_2k_x4( fcmplx *in )
{
	// Zero the unused parts of the array
	//memset(&m_in[M8KS / 8], 0, sizeof(fftwf_complex)*M8KS * 6 / 8);

	int i, m;
	m = (M8KS)-(M2KS / 2);
	for (i = 0; i < (M2KS); i++)
	{
		m_in[m][0] = in[i].re*m_c[i];
		m_in[m][1] = in[i].im*m_c[i];
		m = (m + 1) % M8KS;
	}

    fftwf_execute(m_plan);
	return M8KS;

}
static int fft_2k_x8( fcmplx *in )
{
	int i, m;
	m = (M16KS)-(M2KS / 2);
	for (i = 0; i < (M2KS); i++)
	{
		m_in[m][0] = in[i].re*m_c[i];
		m_in[m][1] = in[i].im*m_c[i];
		m = (m + 1) % M16KS;
	}
    fftwf_execute(m_plan);
	return M16KS;
}
//
// 8K FFT x1 interpolation
//
static int fft_8k_x1( fcmplx *in )
{
    // Copy the data into the correct bins

    int i,m;
    m = (M8KS/2);
    for( i = 0; i < (M8KS); i++ )
    {
        m_in[m][0] = in[i].re*m_c[i];
        m_in[m][1] = in[i].im*m_c[i];
        m = (m+1)%M8KS;
    }
    fftwf_execute(m_plan);
	return M8KS;

}
//
// 8K x2 interpolation
//
static int fft_8k_x2( fcmplx *in )
{
    // Zero the unused parts of the array
    //memset(&m_in[M8KS/2],0,sizeof(fftwf_complex)*M8KS);
    // Copy the data into the correct bins

    int i,m;
    m = (M16KS)-(M8KS/2);
    for( i = 0; i < (M8KS); i++ )
    {
        m_in[m][0] = in[i].re*m_c[i];
        m_in[m][1] = in[i].im*m_c[i];
        m = (m+1)%(M16KS);
    }
    fftwf_execute(m_plan);
	return M16KS;
}
//
// 8K x4 interpolation
//
static int fft_8k_x4( fcmplx *in )
{
	// Zero the unused parts of the array
	//memset(&m_in[M8KS / 2], 0, sizeof(fftwf_complex)*(M16KS+M8KS));
	// Copy the data into the correct bins

	int i, m;
	m = (M32KS)-(M8KS / 2);
	for (i = 0; i < (M8KS); i++)
	{
		m_in[m][0] = in[i].re*m_c[i];
		m_in[m][1] = in[i].im*m_c[i];
		m = (m + 1) % (M32KS);
	}
    fftwf_execute(m_plan);
	return M32KS;
}
static int fft_8k_x8( fcmplx *in )
{
	int i, m;
	m = (M64KS)-(M8KS / 2);
	for (i = 0; i < (M8KS); i++)
	{
		m_in[m][0] = in[i].re*m_c[i];
		m_in[m][1] = in[i].im*m_c[i];
		m = (m + 1) % (M64KS);
	}
    fftwf_execute(m_plan);
	return M64KS;
}

void dvb_t_fft( fcmplx *in, int guard )
{
	int size = fft(in);
	dvb_t_final_modulate(m_out, size, guard);
}

void dvb_t_fft_open( void )
{
	m_plan 	= NULL;
	m_in 	= NULL;
	m_out 	= NULL;
}
void dvb_t_fft_config( DVBTFormat *fmt )
{

	if(m_plan != NULL) fftwf_destroy_plan(m_plan);
	if(m_in   != NULL) fftwf_free(m_in);
	if(m_out  != NULL) fftwf_free(m_out);

	int N;

	if( fmt->tm == TM_2K)
    {
		if(fmt->ir == 1){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M2KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M2KS);
		    m_plan  = fftwf_plan_dft_1d(M2KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M2KS);
			fft = fft_2k_x1;
		}
		if(fmt->ir == 2){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M4KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M4KS);
		    m_plan  = fftwf_plan_dft_1d(M4KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M4KS);
			fft = fft_2k_x2;
		}
		if(fmt->ir == 4){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M8KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M8KS);
		    m_plan  = fftwf_plan_dft_1d(M8KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M8KS);
			fft = fft_2k_x4;
		}
		if(fmt->ir == 8){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M16KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M16KS);
		    m_plan  = fftwf_plan_dft_1d(M16KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M16KS);
			fft = fft_2k_x8;
		}
		N = M2KS;
		m_size = fmt->ir*N;
    }
    if( fmt->tm == TM_8K)
    {
		if (fmt->ir == 1){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M8KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M8KS);
		    m_plan  = fftwf_plan_dft_1d(M8KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M8KS);
			fft = fft_8k_x1;
		}
		if (fmt->ir == 2){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M16KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M16KS);
		    m_plan  = fftwf_plan_dft_1d(M16KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M16KS);
			fft = fft_8k_x2;
		}
		if (fmt->ir == 4){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M32KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M32KS);
		    m_plan  = fftwf_plan_dft_1d(M32KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M32KS);
			fft = fft_8k_x4;
		}
		if (fmt->ir == 8){
		    m_in    = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M64KS);
		    m_out   = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * M64KS);
		    m_plan  = fftwf_plan_dft_1d(M64KS,  m_in, m_out, FFTW_BACKWARD, FFTW_ESTIMATE);
		    // Clear input buffer
			memset(m_in,0,sizeof(fftwf_complex)*M64KS);
			fft = fft_8k_x8;
		}
		N = M8KS;
		m_size = fmt->ir*N;
    }
    int GI;
    switch(fmt->gi)
    {
        case GI_132:
            GI = 32;
            break;
        case GI_116:
            GI = 16;
            break;
        case GI_18:
            GI = 8;
            break;
        case GI_14:
            GI = 4;
            break;
        default:
            GI = 4;
            break;
    }

    create_correction_table( N, fmt->ir);
    create_taper_table(      N, fmt->ir, GI );
}
void dvb_t_fft_close( void )
{
	if(m_plan != NULL) fftwf_destroy_plan(m_plan);
	if(m_in   != NULL) fftwf_free(m_in);
	if(m_out  != NULL) fftwf_free(m_out);
}
