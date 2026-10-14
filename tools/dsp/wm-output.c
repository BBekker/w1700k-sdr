/* SPDX-License-Identifier: MIT */
/* Bounded DSP DTCM -> MCU_DMA0 TX0 -> host WFDMA RX0 experiment.
 * No CPU payload reads/copies. Uses the existing host driver's stream ring.
 * Caller seeds both idle DSP power banks; this test alternates them. */
#include <stdint.h>
#include "../../include/hw/mt7996/mcu_dma.h"
#define R(a) (*(volatile uint32_t *)(a))
#define H(a) (*(volatile uint16_t *)(a))
#define S ((volatile uint32_t *)0x0090d000)
#define GO 0x0090f008u
static inline uint32_t cycles(void) { uint32_t v; __asm__ volatile("rdcycle %0":"=r"(v)); return v; }
static void reclaim(void)
{
    uint32_t saved;
    __asm__ volatile("mv %0, gp":"=r"(saved));
    __asm__ volatile("li gp, 0x02212800\n li a0, 0\n li a1, 0\n jalr ra, 0(%0)\n mv gp, %1"
        ::"r"(0x00825010u),"r"(saved)
        :"ra","a0","a1","a2","a3","a4","a5","a6","a7","t0","t1","t2","t3","t4","t5","t6","memory");
}
__attribute__((section(".text.entry")))
uint32_t output_trial(uint32_t bytes,uint32_t reps,uint32_t core)
{
    uint32_t status=0,k=0,total=0,maximum=0,start=0,t;
    for(unsigned i=0;i<16;i++) S[i]=0;
    S[0]=0x444f5554;
    if(!bytes || bytes>8192 || (bytes&15) || !reps || reps>1024 || core>2) return S[1]=1;
    t=cycles();
    while(R(GO)!=0x474f474f) if(cycles()-t>1440000000u) return S[1]=2;
    R(GO)=0;
    uint32_t cpu=R(MT7996_MCU_DMA0_TX0_CIDX);
    if(cpu!=R(MT7996_MCU_DMA0_TX0_DIDX) || cpu!=R(MT7996_MCU_DMA0_TX0_ADAPTER+0x2c)) return S[1]=3;
    uint32_t desc=R(MT7996_MCU_DMA0_TX0_BASE);
    uint32_t slots=R(MT7996_MCU_DMA0_TX0_ADAPTER+8);
    start=cycles();
    for(;k<reps;k++) {
        R(slots+cpu*4)=0;
        R(desc+cpu*16)=0x86200000u+(core<<22)+((k&1)?0xb000:0x5000);
        H(desc+cpu*16+6)=0x4000u|bytes;
        __asm__ volatile("fence w,w":::"memory");
        cpu=(cpu+1)&7; t=cycles();
        R(MT7996_MCU_DMA0_TX0_CIDX)=cpu;
        while(R(MT7996_MCU_DMA0_TX0_DIDX)!=cpu) {
            if(cycles()-t>7200000u) {status=4;goto finish;}
        }
        uint32_t dt=cycles()-t;
        total+=dt;if(dt>maximum) maximum=dt;
    }
finish:
    S[2]=k;S[3]=total;S[4]=maximum;S[5]=cycles()-start;
    S[6]=R(MT7996_MCU_DMA0_TX0_DIDX);S[7]=cpu;
    S[8]=R(MT7996_MCU_DMA0_INT_STA);
    if(!status) {
        R(MT7996_MCU_DMA0_TX0_ADAPTER+0x2c)=cpu;
        R(MT7996_MCU_DMA0_TX0_ADAPTER+0x34)+=k;
        if(k>=8) R(MT7996_MCU_DMA0_TX0_ADAPTER+0x38)+=k-8;
        reclaim();
    }
    S[1]=status;
    return status;
}
