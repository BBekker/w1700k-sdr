/* SPDX-License-Identifier: MIT */
/* Bounded continuous ICAP -> MEM_DMA -> DSP experiment. No host per-block
 * calls, no skipped blocks. Stop before the writer can overwrite unread data.
 * Descriptor ownership follows wm-dma.c; RX buffers are changed only after
 * DDONE, before returning the descriptor. All addresses are fixed reservations.
 * Entry args: milliseconds (1..3000), DSP core mask (1..7), mode (0 DMA only,
 * 1 full spectrum). Host configures the pool route and each DSP's twiddles.
 * Read results directly from WM SRAM; never ask the occupied WM interpreter
 * to execute another program. Interrupts are disabled by the WM call wrapper. */
#include <stdint.h>
#define R(a) (*(volatile uint32_t *)(a))
#define REG(o) R(0x58000000u+(o))
#define TX ((volatile uint32_t *)0x00910800)
#define RX ((volatile uint32_t *)0x00910900)
#define S ((volatile uint32_t *)0x00910b00)
#define BOX(b) ((volatile uint32_t *)(0x86200000u+((b)<<22)))
#define RING 0x100000u
#define GUARD 0x20000u
#define CHUNK 8192u
static inline uint32_t cycles(void) { uint32_t v; __asm__ volatile("rdcycle %0":"=r"(v)); return v; }
static inline void fence(void) { __asm__ volatile("fence iorw, iorw":::"memory"); }
static inline uint32_t dest(uint32_t b) { return 0x8620d000u+(b<<22); }
static inline uint32_t head(void) { return ((R(0x800230b4)<<2)+16u)&(RING-1); }

__attribute__((section(".text.entry")))
uint32_t continuous_capture(uint32_t ms, uint32_t mask, uint32_t mode)
{
    uint32_t cores[3], count=0, pending[3]={0,0,0}, req[3]={0,0,0};
    uint32_t seq=0, done=0, cfg, status=0, prev=0, begin=0, last_poll=0;
    uint32_t elapsed=0, capture_cycles=0, last_dma=0, dma_sum=0, max_lag=0, max_poll=0;
    uint64_t lap=0, written=0;
    for(unsigned i=0;i<48;i++) S[i]=0;
    S[0]=0x44534331; S[4]=mode; S[5]=mask; S[6]=ms;
    if(!ms || ms>3000 || !mask || mask>7 || mode>1 || (R(0x80023090)&3)) return S[3]=10;
    for(unsigned b=0;b<3;b++) if(mask&(1u<<b)) {
        if(BOX(b)[0]!=0x44535031 || BOX(b)[10]!=1 || BOX(b)[1]!=BOX(b)[2]) return S[3]=11;
        cores[count++]=b; req[b]=BOX(b)[1];
    }
    cfg=REG(0x208);
    if((cfg&10) || REG(0x308)!=REG(0x30c)) return S[3]=12;
    REG(0x208)=cfg&~5u;
    for(unsigned i=0;i<8;i++) {
        TX[4*i]=0xe0400000; TX[4*i+1]=0x80000000;
        TX[4*i+2]=TX[4*i+3]=0;
        RX[4*i]=dest(cores[i%count]); RX[4*i+1]=CHUNK<<16;
        RX[4*i+2]=RX[4*i+3]=0;
    }
    fence();
    REG(0x300)=0x00910800; REG(0x304)=8; REG(0x308)=0;
    REG(0x500)=0x00910900; REG(0x504)=8; REG(0x508)=0;
    REG(0x20c)=1; REG(0x280)=1; REG(0x508)=7;
    REG(0x208)=(cfg&~10u)|5u;
    R(0x80023090)=0; R(0x80023098)=0; R(0x8002309c)=RING/4-4;
    R(0x80023090)=0x2400; R(0x88009004)=0xff000000;
    S[2]=1;
    begin=last_poll=cycles();
    R(0x80023090)=0x2401; R(0x80023090)=0x2403;
    unsigned draining=0;
    for(;;) {
        uint32_t now=cycles(), gap=now-last_poll;
        last_poll=now; elapsed=now-begin;
        if(gap>max_poll) max_poll=gap;
        /* A missed whole lap would make a modular pointer ambiguous. At
         * 40 Msps one lap is 6.55 ms; refuse pauses of 3 ms or longer. */
        if(gap>2160000u) { status=6; break; }
        if(!draining) {
            uint32_t pos=head();
            if(pos<prev) lap+=RING;
            prev=pos; written=lap+pos;
            uint64_t lag=written-(uint64_t)seq*CHUNK;
            if(lag>max_lag) max_lag=(uint32_t)lag;
            if(lag>=RING-GUARD) status=2;
            if(status==2 || elapsed>=ms*720000u || S[1]) {
                /* No DMA outstanding here. Capture stops once, at the
                 * requested end; drain all complete blocks afterwards. */
                R(0x80023090)=0x400;
                uint32_t end=head();
                if(end<prev) lap+=RING;
                written=lap+end;
                draining=1; capture_cycles=cycles()-begin; S[2]=2;
            }
        }
        for(unsigned i=0;i<count;i++) {
            unsigned b=cores[i];
            if(pending[b] && BOX(b)[2]==req[b]) {
                fence();
                if(BOX(b)[6] || BOX(b)[9] || BOX(b)[10]!=1) { status=4; goto finish; }
                pending[b]=0; done++;
                S[20+b]++; S[24+b]=BOX(b)[5];
            }
        }
        if(draining && seq==written/CHUNK && done==seq) { if(!status) status=1; break; }
        if(draining && elapsed-capture_cycles>72000000u) { status=5; break; }
        if((uint64_t)(seq+1)*CHUNK>written) continue;
        unsigned b=cores[seq%count];
        if(pending[b]) continue;
        uint32_t slot=seq&7, next=(slot+1)&7;
        TX[4*slot]=0xe0400000u+((seq*CHUNK)&(RING-1));
        TX[4*slot+1]=0x40000000u|(CHUNK<<16);
        fence();
        uint32_t t=cycles(); REG(0x308)=next;
        while(REG(0x30c)!=next || REG(0x50c)!=next || !(RX[4*slot+1]&0x80000000u)) {
            if(cycles()-t>720000u) { status=3; goto finish; }
        }
        fence();
        last_dma=cycles()-t; dma_sum+=last_dma;
        if(((RX[4*slot+1]>>16)&0x3fff)!=CHUNK) { status=7; break; }
        /* Ownership is back with us. Prepare this slot for its next lap,
         * not the already-posted descriptor used by the current transfer. */
        RX[4*slot]=dest(cores[(seq+8)%count]);
        RX[4*slot+1]=CHUNK<<16; fence(); REG(0x508)=slot;
        if(mode) {
            req[b]++; if(!req[b]) req[b]=1;
            BOX(b)[3]=4; BOX(b)[4]=2048; fence(); BOX(b)[1]=req[b];
            pending[b]=1;
        } else { done++; S[20+b]++; }
        seq++;
    }
finish:
    /* Every normal transfer was waited out above. On an engine error,
     * disable it and refuse further transfers; the caller must reload. */
    REG(0x208)=cfg&~5u;
    R(0x80023090)=0x400; R(0x88009004)=0;
    S[7]=seq; S[8]=done; S[9]=(uint32_t)written; S[10]=(uint32_t)(written>>32);
    S[11]=max_lag; S[12]=capture_cycles ? capture_cycles : cycles()-begin;
    S[13]=cycles()-begin; S[14]=dma_sum; S[15]=last_dma; S[16]=max_poll;
    S[17]=REG(0x308); S[18]=REG(0x30c); S[19]=REG(0x50c);
    for(unsigned b=0;b<3;b++) { S[28+b]=pending[b]; S[32+b]=req[b]; }
    S[3]=status; fence(); S[2]=3;
    return status;
}
