/* SPDX-License-Identifier: MIT */
/* One bounded, stopped 2048-sample capture -> MEM_DMA -> DSP spectrum.
 * Own firmware only, no stream pump, caller configures the tested pool route.
 * Link at 0x00912000; never patch an executing helper. Output 0x0090b000..cfff
 * is reserved until the host has consumed it. No raw payload CPU copy.
 */
#include "wm-dma.c"
#define R(a) (*(volatile uint32_t *)(a))
#define BOX ((volatile uint32_t *)0x86200000)
__attribute__((section(".text.entry")))
uint32_t spectrum_capture(void)
{
    if(R(0x80023090)&3 || BOX[0]!=0x44535031 || BOX[10]!=1 || BOX[1]!=BOX[2]) return 10;
    uint32_t t=cycles();
    R(0x80023090)=0; R(0x80023090)=0x400;
    R(0x88009004)=0xff000000;
    R(0x80023090)=0x401; R(0x80023090)=0x403;
    while(R(0x80023090)&2) if(cycles()-t>TIMEOUT) {
        R(0x80023090)=0x400; R(0x88009004)=0; return 11;
    }
    R(0x80023090)=0x400; R(0x88009004)=0;
    uint32_t rc=dma_trial(0xe0400000,0x8620d000,8192,1);
    if(rc) return 20+rc;
    uint32_t req=BOX[1]+1; if(!req) req=1;
    BOX[3]=4; BOX[4]=2048; fence(); BOX[1]=req;
    t=cycles();
    while(BOX[2]!=req) if(cycles()-t>TIMEOUT) return 30;
    fence();
    if(BOX[6] || BOX[9] || BOX[10]!=1) return 31;
    /* Only FFT powers cross back to the host. IQ stays inside the radio. */
    for(uint32_t i=0;i<2048;i++) {
        uint32_t v=R(0x86205000+4*i);
        /* Validate float32 power bits here instead of a Python per-bin loop. */
        if((v&0x80000000u) || (v&0x7f800000u)==0x7f800000u) return 32;
        R(0x0090b000+4*i)=v;
    }
    /* One compact host-visible record replaces two WM-interpreter commands
     * per FFT. It is stable with the powers until the next helper call. */
    R(0x0090d000)=0x31504644; /* DFP1 */
    R(0x0090d004)=req;
    R(0x0090d008)=BOX[5];
    R(0x0090d00c)=R(0x00910a04);
    fence();
    return 0;
}
