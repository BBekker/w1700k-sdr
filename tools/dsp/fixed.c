/* SPDX-License-Identifier: MIT */
/* Experimental fixed-point spectrum using the MRV5 dual accumulators.
 * Samples retain four fractional bits; every FFT stage is normalized.
 * This is an opt-in numerical/performance experiment, not the viewer default.
 * Full-scale IQ, DC subtraction and the Hann keep products within 36 bits. */
#include "mac.h"

static inline int32_t half(int32_t x) { return (x+1)>>1; }
static inline int32_t quarter(int32_t x) { return (x+2)>>2; }

static void fixed_fft(int32_t *x, const int32_t *tw, unsigned n)
{
    unsigned size=4;
    if(n & 0xaaaaaaaau) {
        for(unsigned i=0;i<2*n;i+=4) {
            int32_t ar=x[i], ai=x[i+1], br=x[i+2], bi=x[i+3];
            x[i]=half(ar+br); x[i+1]=half(ai+bi);
            x[i+2]=half(ar-br); x[i+3]=half(ai-bi);
        }
        size=8;
    }
    for(;size<=n;size<<=2) {
        unsigned q=size/4, step=n/size;
        for(unsigned k=0;k<q;k++) {
            unsigned t=k*step, t3=3*t;
            int32_t w1r=tw[2*t], w1i=tw[2*t+1];
            int32_t w2r=tw[4*t], w2i=tw[4*t+1];
            int32_t w3r, w3i;
            if(t3>=n/2) { w3r=-tw[2*(t3-n/2)]; w3i=-tw[2*(t3-n/2)+1]; }
            else { w3r=tw[2*t3]; w3i=tw[2*t3+1]; }
            for(unsigned base=k;base<n;base+=size) {
                int32_t *a=x+2*base, *b=a+4*q, *c=a+2*q, *d=a+6*q;
                int32_t ar=a[0], ai=a[1], br,bi,cr,ci,dr,di;
                mrv_cmul(b[0],b[1],w1r,w1i,15,&br,&bi);
                mrv_cmul(c[0],c[1],w2r,w2i,15,&cr,&ci);
                mrv_cmul(d[0],d[1],w3r,w3i,15,&dr,&di);
                int32_t acr=ar+cr, aci=ai+ci, amcr=ar-cr, amci=ai-ci;
                int32_t bdr=br+dr, bdi=bi+di, bmdr=br-dr, bmdi=bi-di;
                a[0]=quarter(acr+bdr); a[1]=quarter(aci+bdi);
                b[0]=quarter(acr-bdr); b[1]=quarter(aci-bdi);
                c[0]=quarter(amcr+bmdi); c[1]=quarter(amci-bmdr);
                d[0]=quarter(amcr-bmdi); d[1]=quarter(amci+bmdr);
            }
        }
    }
}

void dsp_fixed_spectrum(const volatile uint32_t *raw, int32_t *x, const float *tw,
                        unsigned n, float *power, int32_t *qtw)
{
    /* Included in the experimental timing; a persistent plan could cache it. */
    for(unsigned i=0;i<n;i++) qtw[i]=(int32_t)(tw[i]*32768.0f);
    int32_t si=0,sq=0;
    for(unsigned i=0;i<n;i++) {
        uint32_t v=raw[i];
        si+=(int32_t)(((v>>16)&0x3fff)^0x2000)-0x2000;
        sq+=(int32_t)((v&0x3fff)^0x2000)-0x2000;
    }
    int32_t mi=si*16/(int32_t)n, mq=sq*16/(int32_t)n;
    unsigned j=0;
    for(unsigned i=0;i<n;i++) {
        uint32_t v=raw[i];
        int32_t re=16*((int32_t)(((v>>16)&0x3fff)^0x2000)-0x2000)-mi;
        int32_t im=16*((int32_t)((v&0x3fff)^0x2000)-0x2000)-mq;
        int32_t c=qtw[2*(i&(n/2-1))];
        if(i>=n/2) c=-c;
        int32_t w=32768-c;
        mrv_cmul(re,im,w,0,16,&x[2*j],&x[2*j+1]);
        unsigned bit=n>>1;
        for(;bit && (j&bit);bit>>=1) j^=bit;
        j^=bit;
    }
    fixed_fft(x,qtw,n);
    for(unsigned i=0;i<n;i++) {
        float re=(float)x[2*i]*(1.0f/65536.0f), im=(float)x[2*i+1]*(1.0f/65536.0f);
        power[i]=re*re+im*im;
    }
}
