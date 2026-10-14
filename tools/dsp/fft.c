/* SPDX-License-Identifier: MIT */
/* Experimental scalar FFT for the MRV5: no runtime, allocation or vendor code.
 * Interleaved float complex input, unnormalised forward transform. Twiddles
 * are exp(-2*pi*i*k/n), k=0..n/2-1, supplied by the host. */
#include <stdint.h>

void dsp_fft(float *x, const float *twiddle, unsigned n)
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
    for (unsigned size = 2; size <= n; size <<= 1) {
        unsigned half = size >> 1, step = n / size;
        for (unsigned base = 0; base < n; base += size) {
            for (unsigned k = 0; k < half; ++k) {
                unsigned a = 2*(base+k), b = a + 2*half;
                float wr = twiddle[2*k*step], wi = twiddle[2*k*step+1];
                float br = x[b], bi = x[b+1];
                float tr = wr*br-wi*bi, ti = wr*bi+wi*br;
                float ar = x[a], ai = x[a+1];
                x[a] = ar+tr; x[a+1] = ai+ti;
                x[b] = ar-tr; x[b+1] = ai-ti;
            }
        }
    }
}
