/* SPDX-License-Identifier: MIT */
/* Portable DSP spectrum arithmetic. Periodic Hann from the FFT twiddle table. */
#include <stdint.h>
extern void dsp_fft_ordered(float *, const float *, unsigned);
void dsp_spectrum(const volatile uint32_t *raw, float *x, const float *tw,
                  unsigned n, float *power)
{
    /* Signed 14-bit input, N <= 2048: these integer sums are exact and
     * avoid a serial floating-point addition dependency for every sample. */
    int32_t si=0, sq=0;
    for (unsigned i=0;i<n;i++) {
        uint32_t v=raw[i];
        si+=(int32_t)(((v>>16)&0x3fff)^0x2000)-0x2000;
        sq+=(int32_t)((v&0x3fff)^0x2000)-0x2000;
    }
    float mi=(float)si/n, mq=(float)sq/n;
    unsigned j=0;
    for (unsigned i=0;i<n/2;i++) {
        uint32_t a=raw[i], b=raw[i+n/2];
        float ar=(float)((int32_t)(((a>>16)&0x3fff)^0x2000)-0x2000);
        float ai=(float)((int32_t)((a&0x3fff)^0x2000)-0x2000);
        float br=(float)((int32_t)(((b>>16)&0x3fff)^0x2000)-0x2000);
        float bi=(float)((int32_t)((b&0x3fff)^0x2000)-0x2000);
        float c=tw[2*i];
        float wa=0.5f-0.5f*c, wb=0.5f+0.5f*c;
        /* Fuse windowing with the FFT's permutation: no swap pass and
         * no intermediate unwindowed float buffer. */
        /* Opposite halves share a cosine and land in adjacent bit-reversed
         * slots, so one address/permutation update prepares two samples. */
        x[4*j]=(ar-mi)*wa; x[4*j+1]=(ai-mq)*wa;
        x[4*j+2]=(br-mi)*wb; x[4*j+3]=(bi-mq)*wb;
        unsigned bit=n>>2;
        for (; bit && (j & bit); bit>>=1) j^=bit;
        j^=bit;
    }
    dsp_fft_ordered(x,tw,n);
    float scale=1.0f/(4096.0f*n); scale*=scale;
    for(unsigned i=0;i<n;i++) power[i]=(x[2*i]*x[2*i]+x[2*i+1]*x[2*i+1])*scale;
}
