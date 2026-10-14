/* SPDX-License-Identifier: MIT */
/* Mixed radix-4/2 FFT for the MRV5: no runtime, allocation or vendor code.
 * Interleaved float complex input, unnormalised forward transform. Twiddles
 * are exp(-2*pi*i*k/n), k=0..n/2-1, supplied by the host. */
#include <stdint.h>
#include "butterfly.h"

void dsp_fft_permute(float *x, unsigned n)
{
    unsigned j = 0;
    for (unsigned i = 1; i < n; ++i) {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float re = x[2*i], im = x[2*i+1];
            x[2*i] = x[2*j]; x[2*i+1] = x[2*j+1];
            x[2*j] = re; x[2*j+1] = im;
        }
    }
}

/* Input already in binary bit-reversed order. */
void dsp_fft_ordered(float *x, const float *twiddle, unsigned n)
{
    if(n<2) return;
    /* An odd log2 needs one radix-2 stage. The remaining stages combine
     * pairs of radix-2 passes, saving one complex multiply per butterfly
     * and an intermediate round trip through DTCM. Binary bit reversal
     * is retained by exchanging the middle two radix-4 inputs. */
    unsigned size = 16;
    if (n & 0xaaaaaaaau) {
        if(n>=8) {
            first8(x,n);
            size=32;
        } else {
            float ar=x[0], ai=x[1], br=x[2], bi=x[3];
            x[0]=ar+br; x[1]=ai+bi;
            x[2]=ar-br; x[3]=ai-bi;
            return;
        }
    } else {
        for(float *p=x;p<x+2*n;p+=8)
            store4(p,p+4,p+2,p+6,p[0],p[1],p[4],p[5],p[2],p[3],p[6],p[7]);
    }
    for (; size <= n; size <<= 2) {
        unsigned q=size/4, step=n/size;
        for(unsigned base=0;base<n;base+=size) {
            float *a=x+2*base, *b=a+4*q, *c=a+2*q, *d=a+6*q;
            store4(a,b,c,d,a[0],a[1],b[0],b[1],c[0],c[1],d[0],d[1]);
        }
        for (unsigned k=1; k<q; ++k) {
            unsigned t=k*step, t3=3*t;
            float w1r=twiddle[2*t], w1i=twiddle[2*t+1];
            float w2r=twiddle[4*t], w2i=twiddle[4*t+1];
            float w3r, w3i;
            if (t3 >= n/2) {
                w3r=-twiddle[2*(t3-n/2)]; w3i=-twiddle[2*(t3-n/2)+1];
            } else {
                w3r=twiddle[2*t3]; w3i=twiddle[2*t3+1];
            }
            for (unsigned base=k; base<n; base+=size) {
                float *a=x+2*base, *b=a+4*q, *c=a+2*q, *d=a+6*q;
                float ar=a[0], ai=a[1];
                float br=b[0]*w1r-b[1]*w1i, bi=b[0]*w1i+b[1]*w1r;
                float cr=c[0]*w2r-c[1]*w2i, ci=c[0]*w2i+c[1]*w2r;
                float dr=d[0]*w3r-d[1]*w3i, di=d[0]*w3i+d[1]*w3r;
                float acr=ar+cr, aci=ai+ci, amcr=ar-cr, amci=ai-ci;
                float bdr=br+dr, bdi=bi+di, bmdr=br-dr, bmdi=bi-di;
                a[0]=acr+bdr; a[1]=aci+bdi;
                b[0]=acr-bdr; b[1]=aci-bdi;
                c[0]=amcr+bmdi; c[1]=amci-bmdr;
                d[0]=amcr-bmdi; d[1]=amci+bmdr;
            }
        }
    }
}

void dsp_fft(float *x, const float *twiddle, unsigned n)
{
    dsp_fft_permute(x, n);
    dsp_fft_ordered(x, twiddle, n);
}
