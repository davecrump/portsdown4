#include <math.h>
#include <memory.h>
#include "dvb_t.h"
// This modules is an LPF filter used for filtering the shoulders and aliases
// on the ouput of the DVB-T iFFT
// It operates in place
//

#define FILTER_N 63

// Filter weights
static int16_t m_w[FILTER_N/2];
// Filter memory
static scmplx *m_m;

static void (*filter)(scmplx *inout, int len);

void dvb_t_update_filter_memory(scmplx *in, int len){
	memcpy(m_m,&m_m[len],sizeof(scmplx)*FILTER_N);
	memcpy(&m_m[FILTER_N],in,sizeof(scmplx)*len);
}
void dvb_t_filter(scmplx *inout, int len){
	dvb_t_update_filter_memory(inout, len);
	filter(inout,len);
}

//
// 1.15 Fixed point filter
// inout = complex samples to be filtered
// coff coefficients
// mm filter memory
// len length of samples to filter
//
//
// Filter version with reduced number of multiplies
//
void dvb_t_filter2(scmplx *inout, int len) {
	int32_t real, imag;
	// Point to coefficients
    int16_t *coffs = m_w;
    scmplx *mm;

	for (int i = 0; i < len; i++) {
		mm = &m_m[i];
		// Filter operation
		scmplx  *h = &mm[(FILTER_N / 2) + 1];
		scmplx  *l = &mm[(FILTER_N / 2) - 1];
		real = coffs[0] * mm[FILTER_N/2].re;
		imag = coffs[0] * mm[FILTER_N/2].im;

		for (int j = 1; j < (FILTER_N/2); j++) {
			real += coffs[j] * (l->re + h->re);
			imag += coffs[j] * (l->im + h->im);
			l--; h++;
		}
		inout[i].re = real >> 15;
		inout[i].im = imag >> 15;
	}
}
//
// Filter program that takes advantage of the zero coefficients in a halfband filter
//
void dvb_t_halfband_filter(scmplx *inout, int len) {
	int32_t real, imag;
	// Point to coefficients
	int16_t *coffs = m_w;
    scmplx *mm;

	for (int i = 0; i < len; i++) {
		mm = &m_m[i];

		// Filter operation
		scmplx  *h = &mm[(FILTER_N / 2) + 1];
		scmplx  *l = &mm[(FILTER_N / 2) - 1];
		real  = coffs[0] * mm[FILTER_N / 2].re;
		imag  = coffs[0] * mm[FILTER_N / 2].im;
		real += coffs[1] * (l->re + h->re);
		imag += coffs[1] * (l->im + h->im);

		for (int j = 3; j < (FILTER_N / 2); j+=2) {
			l-=2; h+=2;
			real += coffs[j] * (l->re + h->re);
			imag += coffs[j] * (l->im + h->im);
		}
		inout[i].re = real >> 15;
		inout[i].im = imag >> 15;
	}
}
/*
void dvb_t_filter(scmplx *inout, int16_t *coff, scmplx *mm, int mlen, int slen) {
	int32_t real, imag;
	int cm = (mlen / 2) + 1;//center tap

	for (int i = 0; i < slen; i++) {

		// Update filter memory
		for (int j = 0; j < mlen - 1; j++) {
			mm[j] = mm[j + 1];
		}
		mm[mlen - 1] = inout[i];

		// Filter operation
		real = coff[0] * mm[0].re;
		imag = coff[0] * mm[0].im;
		for (int j = 1; j < mlen; j++) {
			real += coff[j] * mm[j].re;
			imag += coff[j] * mm[j].im;
		}
		inout[i].re = real >> 15;
		inout[i].im = imag >> 15;
	}
}
*/
//
// Routines to create a LPF (Rectangular Window)
//
void dvb_t_build_lpf_filter(float *filter, float bw, int ntaps) {
	double a;
	double B = bw;// filter bandwidth
	double t = -(ntaps - 1) / 2;// First tap
								// Create the filter
	for (int i = 0; i < ntaps; i++) {
		if (t == 0)
			a = 2.0 * B;
		else
			a = 2.0 * B * sin(M_PI * t * B) / (M_PI * t * B);
		filter[i] = (float)a;
		t = t + 1.0;
	}
}
//
// Set the gain to 1
//
void dvb_t_set_lpf_filter_unity(float *filter, int ntaps) {
	float sum = 0;
	for (int i = 0; i < ntaps; i++) {
		sum += filter[i];
	}
	sum = 1.0f / sum;
	for (int i = 0; i < ntaps; i++) {
		filter[i] *= sum;
	}
}
void dvb_t_convert_to_fixed(float *in, int16_t *out, int n) {
	for (int i = 0; i < n; i++) {
		out[i]= (int16_t)(in[i]*0x7FFF);
	}
}

void dvb_t_filter_open(void) {
	m_m = NULL;
}

//
// The Interpolation ratio is used to create the correct set of filters.
//
void dvb_t_filter_config(DVBTFormat *fmt) {

	float temp[FILTER_N];
	//
	// Only 1/2 the coefficients are used as the filter is symmetrical
	//
	switch(fmt->ir){
	case 1:
		dvb_t_build_lpf_filter(temp, 1.0f,     FILTER_N);
		dvb_t_set_lpf_filter_unity(temp,       FILTER_N);
		dvb_t_convert_to_fixed(&temp[FILTER_N/2], m_w, FILTER_N/2);
		filter = dvb_t_filter2;
		break;
	case 2:
		dvb_t_build_lpf_filter(temp, 0.5f,     FILTER_N);
		dvb_t_set_lpf_filter_unity(temp,       FILTER_N);
		dvb_t_convert_to_fixed(&temp[FILTER_N/2], m_w, FILTER_N/2);
		filter = dvb_t_halfband_filter;
		break;
	case 4:
		dvb_t_build_lpf_filter(temp, 0.25f,    FILTER_N);
		dvb_t_set_lpf_filter_unity(temp,       FILTER_N);
		dvb_t_convert_to_fixed(&temp[FILTER_N/2], m_w, FILTER_N/2);
		filter = dvb_t_filter2;
		break;
	case 8:
		dvb_t_build_lpf_filter(temp, 0.125f,   FILTER_N);
		dvb_t_set_lpf_filter_unity(temp,       FILTER_N);
		dvb_t_convert_to_fixed(&temp[FILTER_N/2], m_w, FILTER_N/2);
		filter = dvb_t_filter2;
		break;
	default:
		dvb_t_build_lpf_filter(temp, 1.0f,     FILTER_N);
		dvb_t_set_lpf_filter_unity(temp,       FILTER_N);
		dvb_t_convert_to_fixed(&temp[FILTER_N/2], m_w, FILTER_N/2);
		filter = dvb_t_filter2;
		break;
	}
	if(m_m != NULL) free(m_m);
	if(fmt->tm == TM_2K) m_m = (scmplx*)malloc(sizeof(scmplx)*fmt->ir*(M2KS+M2KS/4));
	if(fmt->tm == TM_8K) m_m = (scmplx*)malloc(sizeof(scmplx)*fmt->ir*(M8KS+M8KS/4));
}

