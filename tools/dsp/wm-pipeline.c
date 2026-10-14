/* SPDX-License-Identifier: MIT */
/* Autonomous ping-pong snapshot pipeline. Input: ICAP -> MEM_DMA -> DSP.
 * Output: DSP DTCM -> MCU_DMA0 -> host DMA ring. Neither WM nor the host
 * copies IQ/powers through MMIO. Two raw and two power banks per DSP.
 * Snapshots are explicitly NOT continuous RF coverage: capture waits for
 * input-buffer credit. max_blocks=0 runs until the host pump's STOP word.
 * Only its GO/STOP/status mailbox may be accessed while WM is occupied. */
#include "wm-output.c"
#define MEM(o) R(0x58000000u+(o))
#define TX ((volatile uint32_t *)0x00910800)
#define RX ((volatile uint32_t *)0x00910900)
#define BOX(b) ((volatile uint32_t *)(0x86200000u+((b)<<22)))
#define C0 R(0x80023090)
#define GRANT R(0x88009004)
static inline void fence(void) { __asm__ volatile("fence iorw,iorw":::"memory"); }
static inline uint32_t raw_at(unsigned b,unsigned bank) { return 0x86200000u+(b<<22)+(bank?0x7000:0xd000); }
static inline uint32_t power_at(unsigned b,unsigned bank) { return 0x86200000u+(b<<22)+(bank?0xb000:0x5000); }

/* Local state belongs only to WM. DSP mailboxes transfer ownership through
 * request/ack; DMA transfers it through DIDX + RX DDONE. */
__attribute__((section(".text.entry.pipeline")))
uint32_t pipeline(uint32_t max_blocks,uint32_t mask)
{
    unsigned cores[3],count=0;
    uint32_t raw[3][2]={{0}},power[3][2]={{0}},rawseq[3][2]={{0}},incycles[3][2]={{0}};
    uint32_t req[3]={0},pending[3]={0},workbank[3]={0},worktime[3]={0};
    uint32_t nextfft[3]={0},outseq[8]={0},outpower[8]={0};
    uint32_t seq=0,computed=0,published=0,delivered=0,packets=0,status=0;
    uint32_t input_sum=0,output_sum=0,output_time[3][2]={{0}};
    uint32_t cap=0,cap_time=0,dma=0,dma_time=0,dma_seq=0,stopping=0;
    uint32_t cfg,desc,slots,cpu,retired,start,t;
    for(unsigned i=0;i<32;i++) S[i]=0;
    S[0]=0x44505031;S[2]=1;
    if(!mask || mask>7 || (C0&3) || GRANT) return S[1]=1;
    for(unsigned b=0;b<3;b++) if(mask&(1u<<b)) {
        if(BOX(b)[0]!=0x44535031 || BOX(b)[10]!=1 || BOX(b)[1]!=BOX(b)[2]) return S[1]=2;
        nextfft[b]=count;cores[count++]=b;req[b]=BOX(b)[1];
    }
    cfg=MEM(0x208);
    if((cfg&10) || MEM(0x308)!=MEM(0x30c)) return S[1]=3;
    cpu=R(MT7996_MCU_DMA0_TX0_CIDX);retired=cpu;
    if(cpu!=R(MT7996_MCU_DMA0_TX0_DIDX) || cpu!=R(MT7996_MCU_DMA0_TX0_ADAPTER+0x2c)) return S[1]=4;
    desc=R(MT7996_MCU_DMA0_TX0_BASE);slots=R(MT7996_MCU_DMA0_TX0_ADAPTER+8);
    MEM(0x208)=cfg&~5u;
    for(unsigned i=0;i<8;i++) {
        TX[4*i]=0xe0400000;TX[4*i+1]=0x80000000;TX[4*i+2]=TX[4*i+3]=0;
        /* Bank assignment is fixed by sequence, including each descriptor's
         * next lap. It is changed only when the RX slot is owned by WM. */
        RX[4*i]=raw_at(cores[i%count],(i/count)&1);
        RX[4*i+1]=8192u<<16;RX[4*i+2]=RX[4*i+3]=0;
    }
    fence();
    MEM(0x300)=0x00910800;MEM(0x304)=8;MEM(0x308)=0;
    MEM(0x500)=0x00910900;MEM(0x504)=8;MEM(0x508)=0;
    MEM(0x20c)=1;MEM(0x280)=1;MEM(0x508)=7;MEM(0x208)=(cfg&~10u)|5;
    t=cycles();
    while(R(GO)!=0x474f474f) if(cycles()-t>1440000000u) {status=5;goto finish;}
    R(GO)=0;start=cycles();S[2]=2;
    for(;;) {
        uint32_t now=cycles();
        if(R(GO)==0x53544f50 || (max_blocks && seq>=max_blocks)) stopping=1;
        /* Reclaim output banks only when the power packet has left TX0.
         * A header completion alone cannot release its following payload. */
        uint32_t didx=R(MT7996_MCU_DMA0_TX0_DIDX);
        while(retired!=didx) {
            if(outpower[retired]) {
                uint32_t s=outseq[retired],b=cores[s%count],bank=(s/count)&1;
                power[b][bank]=0;delivered++;
                output_sum+=now-output_time[b][bank];
            }
            retired=(retired+1)&7;
        }
        /* FFT completion releases raw input, but not the output bank. */
        for(unsigned i=0;i<count;i++) {
            unsigned b=cores[i];
            if(pending[b] && BOX(b)[2]==req[b]) {
                fence();
                if(BOX(b)[6] || BOX(b)[9] || BOX(b)[10]!=1) {status=6;goto finish;}
                unsigned bank=workbank[b];
                raw[b][bank]=0;power[b][bank]=2;pending[b]=0;computed++;
                volatile uint32_t *h=(volatile uint32_t *)(0x86200100u+(b<<22)+64*bank);
                h[0]=0x44535050;h[1]=rawseq[b][bank];h[2]=b;h[3]=bank;
                h[4]=2048;h[5]=BOX(b)[5];h[6]=incycles[b][bank];h[7]=req[b];
                for(unsigned j=8;j<16;j++) h[j]=0;
                fence();
            } else if(pending[b] && now-worktime[b]>7200000u) {status=7;goto finish;}
        }
        /* Publish in input sequence order: 64-byte metadata then 8192 powers.
         * Reserve two descriptors before publishing either of the pair. */
        {
            unsigned b=cores[published%count],bank=(published/count)&1;
            if(power[b][bank]==2 && rawseq[b][bank]==published && ((cpu-retired)&7)<=5) {
                output_time[b][bank]=now;
                for(unsigned part=0;part<2;part++) {
                    R(slots+4*cpu)=0;
                    R(desc+16*cpu)=part?power_at(b,bank):0x86200100u+(b<<22)+64*bank;
                    H(desc+16*cpu+6)=0x4000u|(part?8192:64);
                    outseq[cpu]=published;outpower[cpu]=part;
                    fence();cpu=(cpu+1)&7;R(MT7996_MCU_DMA0_TX0_CIDX)=cpu;packets++;
                }
                power[b][bank]=3;published++;
            }
        }
        /* Input DMA runs while other banks are being transformed or sent. */
        if(dma) {
            unsigned slot=dma_seq&7,next=(slot+1)&7;
            if(MEM(0x30c)==next && MEM(0x50c)==next && (RX[4*slot+1]&0x80000000u)) {
                fence();
                if(((RX[4*slot+1]>>16)&0x3fff)!=8192) {status=8;goto finish;}
                unsigned b=cores[dma_seq%count],bank=(dma_seq/count)&1;
                raw[b][bank]=2;incycles[b][bank]=now-dma_time;input_sum+=now-dma_time;
                RX[4*slot]=raw_at(cores[(dma_seq+8)%count],((dma_seq+8)/count)&1);
                RX[4*slot+1]=8192u<<16;fence();MEM(0x508)=slot;dma=0;
            } else if(now-dma_time>7200000u) {status=9;goto finish;}
        }
        for(unsigned i=0;i<count;i++) {
            unsigned b=cores[i],bank=(nextfft[b]/count)&1;
            if(!pending[b] && raw[b][bank]==2 && power[b][bank]==0 && rawseq[b][bank]==nextfft[b]) {
                req[b]++;if(!req[b])req[b]=1;
                BOX(b)[3]=7;BOX(b)[4]=2048;BOX(b)[11]=bank?3:0;
                fence();BOX(b)[1]=req[b];pending[b]=1;workbank[b]=bank;worktime[b]=now;
                raw[b][bank]=3;power[b][bank]=1;nextfft[b]+=count;
            }
        }
        if(cap && !(C0&2)) {
            C0=0x400;GRANT=0;
            unsigned b=cores[seq%count],bank=(seq/count)&1,slot=seq&7;
            raw[b][bank]=1;rawseq[b][bank]=seq;
            TX[4*slot+1]=0x60000000u;fence();
            dma_seq=seq++;dma_time=cycles();dma=1;cap=0;MEM(0x308)=(slot+1)&7;
        } else if(cap && now-cap_time>7200000u) {status=10;goto finish;}
        /* ICAP's single snapshot source cannot be overwritten until MEM_DMA
         * has read it. Raw destination credit lets the next capture overlap
         * the previous FFT and output, rather than a host read blocking it. */
        if(!stopping && !cap && !dma && (!max_blocks || seq<max_blocks)) {
            unsigned b=cores[seq%count],bank=(seq/count)&1;
            if(raw[b][bank]==0 && power[b][bank]==0) {
                C0=0;C0=0x400;GRANT=0xff000000;C0=0x401;C0=0x403;
                cap=1;cap_time=cycles();
            }
        }
        if(stopping && !cap && !dma && delivered==seq) break;
        if(published>delivered && now-output_time[cores[delivered%count]][(delivered/count)&1]>7200000u) {status=11;goto finish;}
        S[3]=seq;S[4]=computed;S[5]=delivered;
    }
    S[11]=cycles()-start;
finish:
    /* Drain host TX before retiring capture. No buffer is freed on a timeout. */
    t=cycles();
    while(R(MT7996_MCU_DMA0_TX0_DIDX)!=cpu) if(cycles()-t>7200000u) {status=12;break;}
    MEM(0x208)=cfg&~5u;C0=0x400;GRANT=0;
    if(!status) {
        R(MT7996_MCU_DMA0_TX0_ADAPTER+0x2c)=cpu;
        R(MT7996_MCU_DMA0_TX0_ADAPTER+0x34)+=packets;
        if(packets>=8)R(MT7996_MCU_DMA0_TX0_ADAPTER+0x38)+=packets-8;
        reclaim();
    }
    S[3]=seq;S[4]=computed;S[5]=delivered;S[6]=input_sum;S[7]=output_sum;
    S[8]=packets;S[9]=R(MT7996_MCU_DMA0_INT_STA);S[10]=MEM(0x110);
    S[1]=status;fence();S[2]=3;return status;
}
