/* SPDX-License-Identifier: MIT */
/*
 * MT7996 WM/WA MCU core and local peripherals: identity and Andes custom
 * CSRs, cache control (CCTL), physical memory attributes (PMA), cycle
 * counters, the GPT timer block, the EINT controller, the ROM console port,
 * the hang detector, and MCU/WPLL clock control.
 *
 * The WM and WA are AndeStar V5 RV32 cores (Andes A45-class: marchid
 * 0x80000045), 720 MHz. The DSP cores are in dsp.h.
 *
 * ADDRESS SPACES. CSR numbers are per core (WM and WA have the identical
 * set of 329 CSRs; the rest trap with mcause 2, mtval = the instruction).
 * MMIO addresses are on the MCU bus, which the WM and WA see identically.
 * The host reaches MCU-bus MMIO through its BAR remap windows (see the host
 * PCIe headers) or REG_ACCESS (mcu_dic.h).
 *
 * Field names for Andes CSRs follow the public AndeStar V5 / AX45 manuals;
 * those are a related-core reference, so bit meanings not exercised on this
 * part are tagged [inferred].
 *
 * Confidence tags: [measured] verified on hardware, [static] read from
 * firmware/ROM code or vendor register descriptions, [inferred].
 */
#ifndef HW_MT7996_MCU_PERIPH_H
#define HW_MT7996_MCU_PERIPH_H

#include "../hw_bits.h"

#define MT7996_MCU_CLOCK_HZ               720000000u	/* WM 720.0, WA 719.9 MHz [measured] */
#define MT7996_MCU_CYCLES_PER_US          720u

/* ========================================================================
 * Identity CSRs (WM and WA identical) [measured]
 * ======================================================================== */
#define MT7996_MCU_CSR_MISA               0x301u
#define   MT7996_MCU_MISA_VAL             0x40941105u	/* RV32 I M A C S U X; no F/D/V/P */
#define MT7996_MCU_CSR_MVENDORID          0xf11u
#define   MT7996_MCU_MVENDORID_ANDES      0x0000031eu
#define MT7996_MCU_CSR_MARCHID            0xf12u
#define   MT7996_MCU_MARCHID_VAL          0x80000045u
#define MT7996_MCU_CSR_MIMPID             0xf13u
#define   MT7996_MCU_MIMPID_VAL           0x00000620u
#define MT7996_MCU_CSR_MHARTID            0xf14u	/* 0 on both WM and WA */
/*
 * ISA in use: RV32IMC + Andes V5 extensions (CoDense exec.it/ex9.it, GP-
 * relative loads/stores on major opcode 0x2b, bfoz/bfos, where msb < lsb is
 * the insert form). Soft-float, nothing wider than 32 bits, no custom/ACE
 * encodings. [static]
 */

/* ========================================================================
 * Andes configuration CSRs (RO) [measured values]
 * ======================================================================== */
#define MT7996_MCU_CSR_MICM_CFG           0xfc0u	/* I-cache/ILM config */
#define MT7996_MCU_CSR_MDCM_CFG           0xfc1u	/* D-cache/DLM config */
/* Cache geometry decode used by the ROM (both CSRs): */
#define   MT7996_MCU_CM_CFG_SETS          HW_GENMASK(2, 0)	/* 7 = no cache */
#define   MT7996_MCU_CM_CFG_WAYS          HW_GENMASK(5, 3)	/* ways - 1 */
#define   MT7996_MCU_CM_CFG_LSIZE         HW_GENMASK(8, 6)	/* line = 1 << (v + 2), v 1..5 */
#define   MT7996_MCU_CM_CFG_SETS_SMALL    HW_BIT(24)	/* sets = 32 >> SETS, else 64 << SETS */
#define   MT7996_MCU_WM_MICM_CFG_VAL      0x00061319u	/* I$ 128 x 4 x 64 B = 32 KiB */
#define   MT7996_MCU_WM_ICACHE_BYTES      0x8000u
#define   MT7996_MCU_WM_MDCM_CFG_VAL      0x0005931au	/* D$ 256 x 4 x 64 B = 64 KiB */
#define   MT7996_MCU_WA_MICM_CFG_VAL      0x00061318u	/* I$ 16 KiB */
#define   MT7996_MCU_WA_ICACHE_BYTES      0x4000u	/* 64 sets x 4 ways x 64 B */
#define   MT7996_MCU_WA_ICACHE_LINE       64u
#define   MT7996_MCU_WA_MDCM_CFG_VAL      0x00059318u	/* D$ 16 KiB, 4-way */
#define MT7996_MCU_CACHE_LINE             64u	/* both caches, both cores */
#define MT7996_MCU_CSR_MMSC_CFG           0xfc2u
/* 0xc005e028: CoDense/EX9, HSP, EV5PE, LMSLVP, PMNDS, CCTLCSR, PPMA present;
 * EDSP 0, ACE 0. [measured value, decode inferred] */
#define   MT7996_MCU_MMSC_CFG_VAL         0xc005e028u
#define MT7996_MCU_CSR_MMSC_CFG2          0xfc3u
#define   MT7996_MCU_MMSC_CFG2_VAL        0x00000040u	/* New CoDense (ECDV) */

/* ========================================================================
 * Standard machine CSRs (RISC-V privileged spec): the fields used [static]
 * ======================================================================== */
#define MT7996_MCU_CSR_MSTATUS            0x300u
#define   MT7996_MCU_MSTATUS_MIE          HW_BIT(3)	/* interrupts on */
#define   MT7996_MCU_MSTATUS_MPIE         HW_BIT(7)	/* MIE after mret */

/* ========================================================================
 * Andes machine CSRs
 * ======================================================================== */
/* Local memory base registers: base | enable [measured] */
#define MT7996_MCU_CSR_MILMB              0x7c0u	/* WM 0x00800001, WA 0x10000001 */
#define MT7996_MCU_CSR_MDLMB              0x7c1u	/* WM 0x02200001, WA 0x10200001 */
#define   MT7996_MCU_LMB_EN               HW_BIT(0)
#define   MT7996_MCU_LMB_BASE             HW_GENMASK(31, 10)	/* [inferred] */
#define MT7996_MCU_CSR_MNVEC              0x7c3u	/* NMI vector: WM 0x00800000, WA 0x10000000 */
#define MT7996_MCU_CSR_MXSTATUS           0x7c4u	/* reads 0 */
#define MT7996_MCU_CSR_MHSP_CTL           0x7c6u	/* 0: stack protection off */
#define MT7996_MCU_CSR_MSP_BOUND          0x7c7u	/* 0xffffffff */
#define MT7996_MCU_CSR_MSP_BASE           0x7c8u	/* 0xffffffff */
#define MT7996_MCU_CSR_MDCAUSE            0x7c9u
/* 0x7c2 (mecc_code) and 0x7c5 (mpft_ctl) are absent: they trap. [measured] */

#define MT7996_MCU_CSR_MCACHE_CTL         0x7cau
#define   MT7996_MCU_MCACHE_CTL_IC_EN     HW_BIT(0)	/* [inferred] */
#define   MT7996_MCU_MCACHE_CTL_DC_EN     HW_BIT(1)	/* [inferred] */
/* D-cache prefetcher. Firmware leaves it OFF. On: cold pool reads go from
 * 0.632 to 2.01 B/cycle (455 MB/s -> 1.45 GB/s on the WM). [measured] */
#define   MT7996_MCU_MCACHE_CTL_DPREF_EN  HW_BIT(10)
#define   MT7996_MCU_MCACHE_CTL_IC_FWORD  HW_BIT(11)	/* first-word-first [inferred] */
#define   MT7996_MCU_MCACHE_CTL_DC_FWORD  HW_BIT(12)	/* [inferred] */
/* Streaming-store write-around. 1: pool write 1.36 GB/s, copy 595 MB/s with
 * DPREF; 2 and 3 within 7% of 1. [measured] */
#define   MT7996_MCU_MCACHE_CTL_DC_WAROUND HW_GENMASK(14, 13)
#define   MT7996_MCU_MCACHE_CTL_STOCK     (MT7996_MCU_MCACHE_CTL_IC_EN | MT7996_MCU_MCACHE_CTL_DC_EN | \
                                         MT7996_MCU_MCACHE_CTL_IC_FWORD | MT7996_MCU_MCACHE_CTL_DC_FWORD)
                                         /* 0x1803, WM and WA under MediaTek's firmware [measured] */
#define   MT7996_MCU_MCACHE_CTL_STREAM    (MT7996_MCU_MCACHE_CTL_DPREF_EN | \
                                         HW_PREP(MT7996_MCU_MCACHE_CTL_DC_WAROUND, 1))	/* 0x2400 */

/* CCTL: VA ops write the address to BEGINADDR then the opcode to COMMAND;
 * ALL ops write COMMAND only; index ops write a byte offset (way/set/line)
 * to BEGINADDR. */
#define MT7996_MCU_CSR_MCCTLBEGINADDR     0x7cbu
#define MT7996_MCU_CSR_MCCTLCOMMAND       0x7ccu
#define MT7996_MCU_CSR_MCCTLDATA          0x7cdu
#define MT7996_MCU_CSR_UCCTLBEGINADDR     0x80bu
#define MT7996_MCU_CSR_UCCTLCOMMAND       0x80cu
#define   MT7996_MCU_CCTL_L1D_VA_INVAL    0u	/* [measured] ~51 cycles/op */
#define   MT7996_MCU_CCTL_L1D_VA_WB       1u	/* [measured] ~92 cycles/op dirty */
#define   MT7996_MCU_CCTL_L1D_VA_WBINVAL  2u	/* [inferred] */
#define   MT7996_MCU_CCTL_L1D_VA_LOCK     3u	/* [inferred] */
#define   MT7996_MCU_CCTL_L1D_VA_UNLOCK   4u	/* [inferred] */
#define   MT7996_MCU_CCTL_L1D_WBINVAL_ALL 6u	/* [measured] */
#define   MT7996_MCU_CCTL_L1D_WB_ALL      7u	/* [measured] */
#define   MT7996_MCU_CCTL_L1I_VA_INVAL    8u	/* [inferred] */
#define   MT7996_MCU_CCTL_L1D_IX_INVAL    16u	/* [static] ROM handoff */
#define   MT7996_MCU_CCTL_L1D_IX_WB       17u	/* [static] ROM handoff */
#define   MT7996_MCU_CCTL_L1D_IX_WBINVAL  18u	/* [inferred] */
#define   MT7996_MCU_CCTL_L1D_INVAL_ALL   23u	/* [measured] */
#define   MT7996_MCU_CCTL_L1I_IX_INVAL    24u	/* [static] ROM handoff */
/*
 * Measured CCTL cost on the WM (DPREF + WAROUND 1): WB_ALL 22.9k-31.7k
 * cycles with a dirty 64 KiB D$, 45 cycles clean; WBINVAL_ALL clean 7,716;
 * INVAL_ALL ~4,670; WB_ALL / WBINVAL_ALL after an 8 KiB write-around pack
 * 4,540 / 4,680. WA (16 KiB D$): WB_ALL dirty 12,991, WBINVAL_ALL clean
 * 1,741. A read after VA_INVAL re-fetches at the cold rate.
 *
 * Firmware handoff order (ROM): D index-WB over the whole D$, I index-inval,
 * then D index-inval, interrupts off and stackless. [static]
 *
 * Device DMA (MCU_DMA0) does not snoop the D-cache: CPU-written cacheable
 * pool data must be written back (WB_ALL + fence) before a descriptor
 * points at it. Non-cacheable local memory needs nothing. [measured]
 * A streaming reader of the ICAP ring must drop stale lines at least once
 * per ring lap (WBINVAL_ALL every 410 us against an 819 us lap works; per-
 * chunk CCTL starved TX0), and must not read a chunk until its successor is
 * written, since the prefetcher reads past the end. [measured]
 */

#define MT7996_MCU_CSR_MMISC_CTL          0x7d0u
#define   MT7996_MCU_MMISC_CTL_BRPE       HW_BIT(3)	/* branch prediction [inferred] */
#define   MT7996_MCU_MMISC_CTL_MSA_UNA    HW_BIT(6)	/* misaligned access [inferred] */
#define   MT7996_MCU_MMISC_CTL_VAL        (MT7996_MCU_MMISC_CTL_BRPE | MT7996_MCU_MMISC_CTL_MSA_UNA)	/* 0x48 [measured] */
/* Counter control, all 0: mcounterwen 0x7ce, mcounterinten 0x7cf,
 * mcountermask_m/s/u 0x7d1..0x7d3, mcounterovf 0x7d4, mslideleg 0x7d5. */
/* Also present and 0: PMP (pmpcfg0..15 0x3a0..0x3af, pmpaddr0..63
 * 0x3b0..0x3ef), S-mode CSRs incl. satp; trigger module (see below) tselect 0x7a0,
 * tdata1 0x7a1 = 0x21800000, tinfo 0x7a4 = 0x1c. [measured] */
/* mie = 0x00000808 (MSIE | MEIE) in the IRQ5 context; ROM init ORs 0x808. */

/*
 * HAZARD: CSRs 0x345 (mnxti), 0x348, 0x349 (mscratchcsw/l) have read side
 * effects; never read them in a sweep. [static]
 */

/* ========================================================================
 * Counters [measured]
 * ======================================================================== */
#define MT7996_MCU_CSR_MCYCLE             0xb00u	/* core clock, 720 MHz */
#define MT7996_MCU_CSR_MINSTRET           0xb02u
#define MT7996_MCU_CSR_MCYCLEH            0xb80u	/* read h, l, h; retry if h changed */
#define MT7996_MCU_CSR_MINSTRETH          0xb82u
/* mhpmcounter3..31 (0xb03..0xb1f), mhpmevent3..31 (0x323..0x33f) and
 * mcountinhibit (0x320) present, read 0. User mirrors 0xc00/0xc02/0xc80/
 * 0xc82 present. Spin loop measures 2.0 cycles/iteration, IPC 1.0. */
/* HAZARD: time/timeh (0xc01/0xc81) are absent: rdtime traps (illegal
 * instruction). Use mcycle, GPT2, or the DSP block's 1 MHz counter. */

/* ========================================================================
 * Debug trigger module (RISC-V debug 0.13 triggers), WM [measured 2026-10-10]
 * ======================================================================== */
/*
 * 8 triggers (tselect 0..7; writing 8 or 9 wraps to 0 or 1). Per trigger
 * tinfo: 0 = 0x1c (address/data, instruction count, interrupt), 1 = 0xc,
 * 2/5/6 = 0x4, 3/7 = 0x24 (address/data, exception), 4 = 0x14. tdata1 at
 * reset 0x21800000: type 2 (mcontrol), maskmax 12 (NAPOT up to 4 KiB).
 * mcontrol fields that stick on trigger 0: match 0 (equal), 1 (NAPOT),
 * 2 (>=), 3 (<); chain; m; execute, store, load. Fields that read back 0:
 * match 5 (mask-high), timing (only "before" the access), select (no data
 * match), sizelo, action (breakpoint exception only), dmode. tcontrol
 * (mte bit 3, mpte bit 7) is writable; nothing in the ROM, the ROM patch
 * or MediaTek's WM touches the trigger CSRs, tcontrol or mscratch. [measured]
 * So a chained >= / < pair with m, load and store traps every access to an
 * address range before it happens (mcause 3), for a handler in mtvec.
 * HAZARD: the ROM's trap vector treats every exception but ecall as fatal:
 * install a handler before enabling a trigger, and clear every trigger
 * before restoring mtvec.
 */
#define MT7996_MCU_CSR_TSELECT            0x7a0u
#define MT7996_MCU_CSR_TDATA1             0x7a1u
#define MT7996_MCU_CSR_TDATA2             0x7a2u
#define MT7996_MCU_CSR_TDATA3             0x7a3u
#define MT7996_MCU_CSR_TINFO              0x7a4u
#define MT7996_MCU_CSR_TCONTROL           0x7a5u
#define   MT7996_MCU_TCONTROL_MTE         HW_BIT(3)
#define   MT7996_MCU_TCONTROL_MPTE        HW_BIT(7)
#define MT7996_MCU_CSR_MCONTEXT           0x7a8u
#define MT7996_MCU_CSR_SCONTEXT           0x7aau
#define MT7996_MCU_TRIGGERS               8u
#define   MT7996_MCU_MCONTROL_TYPE        HW_GENMASK(31, 28)	/* 2 */
#define   MT7996_MCU_MCONTROL_DMODE       HW_BIT(27)	/* reads 0 */
#define   MT7996_MCU_MCONTROL_MASKMAX     HW_GENMASK(26, 21)	/* 12 */
#define   MT7996_MCU_MCONTROL_HIT         HW_BIT(20)
#define   MT7996_MCU_MCONTROL_SELECT      HW_BIT(19)	/* reads 0 */
#define   MT7996_MCU_MCONTROL_TIMING      HW_BIT(18)	/* reads 0 */
#define   MT7996_MCU_MCONTROL_SIZELO      HW_GENMASK(17, 16)	/* reads 0 */
#define   MT7996_MCU_MCONTROL_ACTION      HW_GENMASK(15, 12)	/* reads 0: breakpoint */
#define   MT7996_MCU_MCONTROL_CHAIN       HW_BIT(11)
#define   MT7996_MCU_MCONTROL_MATCH       HW_GENMASK(10, 7)
#define     MT7996_MCU_MCONTROL_MATCH_EQ  0u
#define     MT7996_MCU_MCONTROL_MATCH_NAPOT 1u
#define     MT7996_MCU_MCONTROL_MATCH_GE  2u
#define     MT7996_MCU_MCONTROL_MATCH_LT  3u
#define   MT7996_MCU_MCONTROL_M           HW_BIT(6)
#define   MT7996_MCU_MCONTROL_S           HW_BIT(4)	/* not exercised */
#define   MT7996_MCU_MCONTROL_U           HW_BIT(3)	/* not exercised */
#define   MT7996_MCU_MCONTROL_EXECUTE     HW_BIT(2)
#define   MT7996_MCU_MCONTROL_STORE       HW_BIT(1)
#define   MT7996_MCU_MCONTROL_LOAD        HW_BIT(0)

/* ========================================================================
 * PMA: Andes physical memory attributes (WM and WA)
 * ======================================================================== */
/* pmacfg0..3: entry i's config is byte (i % 4) of pmacfg(i / 4).
 * pmaaddr0..15. 0xbc4..0xbcf are absent. [measured] */
#define MT7996_MCU_CSR_PMACFG(n)          (0xbc0u + (n))	/* n = 0..3 */
#define MT7996_MCU_CSR_PMAADDR(n)         (0xbd0u + (n))	/* n = 0..15 */
#define MT7996_MCU_PMA_ENTRIES            16u
#define   MT7996_MCU_PMA_ETYP             HW_GENMASK(1, 0)
#define   MT7996_MCU_PMA_ETYP_OFF         0u
#define   MT7996_MCU_PMA_ETYP_TOR         1u
#define   MT7996_MCU_PMA_ETYP_NA4         2u
#define   MT7996_MCU_PMA_ETYP_NAPOT       3u
#define   MT7996_MCU_PMA_MTYP             HW_GENMASK(5, 2)
#define   MT7996_MCU_PMA_MTYP_DEVICE      0u	/* device, non-bufferable */
#define   MT7996_MCU_PMA_MTYP_NC_NB       2u	/* memory, non-cacheable non-bufferable */
#define   MT7996_MCU_PMA_MTYP_NC_B        3u	/* memory, non-cacheable bufferable */
#define   MT7996_MCU_PMA_MTYP_WB_RWA      11u	/* memory, write-back, read+write allocate */
/* NAPOT address word for a naturally aligned power-of-two region (size >= 8):
 * (base >> 2) + (size >> 3) - 1. The lowest-numbered matching entry wins. */
#define MT7996_MCU_PMA_NAPOT(base, size)  (((base) >> 2) + ((size) >> 3) - 1u)
/*
 * Live table, identical on WM and WA and equal to the ROM's init profile
 * (entries 11..15 and pmacfg3 are 0). [measured]
 *   e  pmaaddr      range (end excl.)          cfg   type
 *   0  0x07ffffff   0x00000000-0x40000000      0x0b  non-cacheable: SYSRAM, ULM0/1/2, WA ILM/DLM
 *   1  0x11ffffff   0x40000000-0x50000000      0x03  device
 *   2  0x15ffffff   0x50000000-0x60000000      0x0b  non-cacheable (MCU_DMA blocks)
 *   3  0x181fffff   0x60000000-0x61000000      0x0b
 *   4  0x185fffff   0x61000000-0x62000000      0x03  device
 *   5  0x18ffffff   0x60000000-0x68000000      0x0b  (3/4 take priority)
 *   6  0x1dffffff   0x70000000-0x80000000      0x03  device (CONN_INFRA)
 *   7  0x21ffffff   0x80000000-0x90000000      0x03  device (WF peripherals, PHY)
 *   8  0x27ffffff   0x80000000-0xc0000000      0x0b  (7 takes priority)
 *   9  0x35ffffff   0xd0000000-0xe0000000      0x0b
 *  10  0x3bffffff   0xe0000000-0x100000000     0x2f  write-back: CRAM and the UMAC pool
 * 0x68000000-0x70000000 and 0xc0000000-0xd0000000 are covered by no entry.
 * The ROM writes each row as: pmaaddr, then a fresh read of pmacfg, then a
 * one-byte masked write; pmacfg2[31:24] is left untouched. [static]
 */
/* The ROM's init profile as values: pmaaddr0..10, then pmacfg0..2. [measured] */
#define MT7996_MCU_PMA_PROFILE_ENTRIES    11u
#define MT7996_MCU_PMAADDR0_VAL           0x07ffffffu
#define MT7996_MCU_PMAADDR1_VAL           0x11ffffffu
#define MT7996_MCU_PMAADDR2_VAL           0x15ffffffu
#define MT7996_MCU_PMAADDR3_VAL           0x181fffffu
#define MT7996_MCU_PMAADDR4_VAL           0x185fffffu
#define MT7996_MCU_PMAADDR5_VAL           0x18ffffffu
#define MT7996_MCU_PMAADDR6_VAL           0x1dffffffu
#define MT7996_MCU_PMAADDR7_VAL           0x21ffffffu
#define MT7996_MCU_PMAADDR8_VAL           0x27ffffffu
#define MT7996_MCU_PMAADDR9_VAL           0x35ffffffu
#define MT7996_MCU_PMAADDR10_VAL          0x3bffffffu
#define MT7996_MCU_PMACFG0_VAL            0x0b0b030bu
#define MT7996_MCU_PMACFG1_VAL            0x03030b03u
#define MT7996_MCU_PMACFG2_VAL            0x002f0b0bu
/*
 * Runtime change, done live on the WM with interrupts off and restored
 * cleanly [measured]: pmaaddr11 = 0x3bffffff (entry 11 takes entry 10's
 * range), pmaaddr10 = 0x3812ffff (entry 10 narrows to 0xe0480000-0xe0500000),
 * pmacfg2 = 0x2f0f0b0b (entry 10 cfg 0x0f non-cacheable bufferable, entry 11
 * 0x2f), fence; restore in reverse. The ordering keeps every address covered
 * at every step. Uncached (0x0f) pool stores ran at 0.059 B/cycle (42.7 MB/s)
 * against ~1.9 GB/s cacheable with write-around.
 * HAZARD: a PMA change must keep the instruction-fetch and stack ranges
 * covered with valid attributes at every intermediate step, with no other
 * PMA user (NMI included) active. [static]
 */

/* ========================================================================
 * GPT timer block (MCU bus) [static unless noted]
 * ======================================================================== */
#define MT7996_MCU_GPT_BASE               0x81060000u
#define MT7996_MCU_GPT_STATUS             0x81060000u	/* [0] GPT0, [1] GPT1 event; W1C not established */
#define MT7996_MCU_GPT_ENABLE             0x81060004u	/* [0] GPT0, [1] GPT1 global enable */
/* GPT0/GPT1 control: [0] EN, [1] auto-repeat, [2] speed */
#define MT7996_MCU_GPT_CTRL(t)            (0x81060010u + 0x10u * (t))	/* t = 0, 1, 2 */
#define MT7996_MCU_GPT_RELOAD(t)          (0x81060014u + 0x10u * (t))	/* t = 0, 1 */
#define   MT7996_MCU_GPT_CTRL_EN          HW_BIT(0)
#define   MT7996_MCU_GPT_CTRL_REPEAT      HW_BIT(1)	/* GPT0/1 */
#define   MT7996_MCU_GPT_CTRL_SPEED       HW_BIT(2)	/* GPT0/1 */
#define MT7996_MCU_GPT0_COUNT             0x81060040u
#define MT7996_MCU_GPT1_COUNT             0x81060044u
/* GPT2: free-running 32-bit counter. Control [0] EN, [1] speed select
 * ("speed_32us"). ROM boot does GPT_init(2, 1): three fresh RMWs (clear EN,
 * set speed, set EN). With speed 1 it counts 32,768 Hz (32,713 ticks in a
 * host-timed second, 2026-10-11): cos_delay_time(n) waits n + 1 ticks,
 * (n + 1) x 30.5 us. [measured] */
#define MT7996_MCU_GPT2_CTRL              0x81060030u
#define   MT7996_MCU_GPT2_CTRL_EN         HW_BIT(0)
#define   MT7996_MCU_GPT2_CTRL_SPEED      HW_BIT(1)
#define MT7996_MCU_GPT2_COUNT             0x81060034u	/* RO use */
#define MT7996_MCU_GPT2_HZ                32768u
/* Microseconds of the ROM's cos_delay_time(n), rounded up. */
#define MT7996_ROM_DELAY_TIME_US(n)       ((((n) + 1u) * 1000000u + MT7996_MCU_GPT2_HZ - 1u) / MT7996_MCU_GPT2_HZ)
/* GPT4: control [0] EN, [6:1] divider = (xtal_hz / 1e6) - 1 (39 for the
 * live 40 MHz crystal word, presumably a 1 MHz tick [inferred]). */
#define MT7996_MCU_GPT4_CTRL              0x81060060u
#define   MT7996_MCU_GPT4_CTRL_EN         HW_BIT(0)
#define   MT7996_MCU_GPT4_CTRL_DIV        HW_GENMASK(6, 1)
#define MT7996_MCU_GPT4_COUNT             0x81060068u
/*
 * GPT3 does not exist (the ROM's GPT_init(3) traps; timer ids > 4 trap).
 * ROM boot order: GPT0 = cos timer (with EINT 0), GPT1 = FreeRTOS tick
 * (reload 320, auto-repeat), GPT2 = free counter (speed 1), GPT4 = cos_delay
 * microsecond base; run-time stats re-initialise GPT4.
 * cos_delay_time(n): target = GPT2 + n + 1, poll until (now - target) has
 * bit 31 clear. cos_delay_usec(n): the same on GPT4.
 * HAZARD: neither ROM delay has a timeout: a stopped counter hangs the
 * caller forever; a huge n can complete without the timer moving
 * (half-range compare). [static]
 * HAZARD: do not reinitialise or stop GPT2/GPT4 while any consumer has an
 * interval in flight. [static]
 */

/* ========================================================================
 * EINT controller (MCU bus) [static]: single stores of 1 << (n & 31)
 * ======================================================================== */
#define MT7996_MCU_EINT_BASE              0x81050000u
#define MT7996_MCU_EINT_UNMASK            0x81050108u
#define MT7996_MCU_EINT_MASK              0x8105010cu
#define MT7996_MCU_EINT_CLEAR             0x81050110u

/* ========================================================================
 * ROM console port [name guess] (MCU bus) [static][measured]
 * ======================================================================== */
/*
 * Debug channel 4 of the ROM console: the retail ROM's 0x00811d52(port,
 * half) stores the half word at 0x800a0004 (port 4; other ports
 * 0x800c0004), then polls 0x800a0008 [1] (busy) until it reads 0;
 * 0x00811d80 sends a 32-bit word as two halves, high first. The ROM printf
 * sends text as big-endian 4-character words, '\n' as "\r\n",
 * NUL-terminated, hex digits in upper case. Observed in MediaTek's cold
 * load: the ROM patch's calibration hook printing "==>Patch_HalPhyCalFunc(),
 * FF \r\n" (16 halves, about 37 status reads each; phy_cal.h "Calibration
 * init"). Not needed by our firmware.
 */
#define MT7996_MCU_DBGPORT_DATA           0x800a0004u	/* port 4 */
#define MT7996_MCU_DBGPORT_STATUS         0x800a0008u
#define   MT7996_MCU_DBGPORT_BUSY         HW_BIT(1)

/* ========================================================================
 * Watchdog and hang detector
 * ======================================================================== */
/*
 * No MCU-local watchdog register has been decoded (the ROM's WDT driver is
 * known only by entry points). The host sees WM/WA watchdog events as bits
 * 30/31 of its MCU command word (see the host PCIe headers). [static]
 *
 * Hang detector and debug PC log, CONN_MCU_EXCP_CON block (the same block
 * holds the host debug-interface command words and the host software IRQ,
 * see mcu_dic.h / mcu_irq.h). Never exercised. [static]
 */
#define MT7996_MCU_EXCP_BASE              0x89050000u
#define MT7996_MCU_EXCP_HANG_DET_CTL      0x89050000u
#define   MT7996_MCU_HANG_DET_EN          HW_BIT(0)
#define   MT7996_MCU_HANG_DET_RST_CNT     HW_BIT(1)
#define   MT7996_MCU_HANG_DET_INT_EN      HW_GENMASK(29, 28)
#define   MT7996_MCU_HANG_DET_INT_STS     HW_GENMASK(31, 30)
#define MT7996_MCU_EXCP_HANG_CYC_THRE     0x89050004u
#define MT7996_MCU_EXCP_RO_CHK_PC         0x89050030u
#define MT7996_MCU_EXCP_RO_CYC_CNT        0x89050034u
#define MT7996_MCU_EXCP_PC_LOG_CTL        0x89050100u
#define   MT7996_MCU_PC_LOG_EN            HW_BIT(0)
#define   MT7996_MCU_PC_LOG_SEL_SRC       HW_BIT(1)
#define   MT7996_MCU_PC_LOG_SEL           HW_GENMASK(7, 2)
#define   MT7996_MCU_PC_LOG_THRESHOLD     HW_GENMASK(15, 8)
#define   MT7996_MCU_PC_LOG_IDX           HW_GENMASK(20, 16)	/* RO */
#define MT7996_MCU_EXCP_PC_LOG(n)         (0x89050104u + 4u * (n))	/* n = 0..31 */

/* ========================================================================
 * MCU configuration, WF_MCU_CFG_LS (MCU bus 0x88000000)
 * ======================================================================== */
/*
 * The register layout is the one Linux documents for the same MCU block in
 * the WED WO core (mtk_wed_wo.h, MTK_WO_MCU_CFG_LS_*). The capture route
 * block (cap_route.h) sits at +0x9000 in the same page group.
 *
 * The ROM starts only the WM. The WA stays in reset until something sets
 * WA_CPU_RSTB; MediaTek's WM does it through the ROM routine WifiCPStart
 * (a plain store of 1, retail 0x0083988e) after its OS init, then waits up
 * to ~1 s for the WA to set FW_STATE bit 2 (WifiCPWaitReady). The WA runs
 * from its reset vector, WA ILM 0x10000000, which the ROM's PDA download
 * filled; no boot-address register is written. [static]
 * Measured 2026-10-09: 0 at our WM's start; one store of 1 starts our WA
 * image (its heartbeat runs). [measured]
 * HAZARD: setting WA_CPU_RSTB before the radio is configured (our replay of
 * MediaTek's end state) leaves the receiver without antenna signal (large
 * DC, low floor), even with the WA parked in wfi from its first instruction
 * and even if the bit is cleared again before configuring; set after the
 * configuration, reception is unaffected. MediaTek's WM sets it before its
 * procedures configure the radio. [measured]
 * HAZARD: a WM store into WA ILM while the WA is held in reset stalls the
 * WM. [measured]
 */
#define MT7996_MCU_CFG_LS_BASE            0x88000000u
#define MT7996_MCU_CFG_LS_HW_VER          0x88000000u	/* [static] */
#define MT7996_MCU_CFG_LS_FW_VER          0x88000004u	/* [static] */
#define MT7996_MCU_CFG_LS_WF_MCCR         0x88000014u	/* [inferred, WO layout] */
#define MT7996_MCU_CFG_LS_WF_MCCR_SET     0x88000018u	/* [inferred, WO layout] */
/* The ROM sets bits 10 and 11 at boot (UART_DSN_Init); the WO driver writes
 * 0xc00 here right before releasing its core. [static] */
#define MT7996_MCU_CFG_LS_WF_MCCR_CLR     0x8800001cu
#define   MT7996_MCU_CFG_LS_WF_MCCR_B10   HW_BIT(10)	/* meaning unknown [static] */
#define   MT7996_MCU_CFG_LS_WF_MCCR_B11   HW_BIT(11)	/* meaning unknown [static] */
#define MT7996_MCU_CFG_WM_WA              0x88000050u
#define   MT7996_MCU_CFG_WM_WA_WA_CPU_RSTB HW_BIT(0)	/* 1: the WA runs [measured] */
/* MediaTek's WA sets [3:1] = 1 once it runs, keeping bit 0. [static] */
#define   MT7996_MCU_CFG_WM_WA_WA_STATE   HW_GENMASK(3, 1)	/* [guess] name */
/* WO layout: WM_CPU_RSTB; the WiFi WM writes it 0 and keeps running, so it
 * does not hold this WM. [static] */
#define   MT7996_MCU_CFG_WM_WA_WM_CPU_RSTB HW_BIT(5)
/* WO layout boot addresses (address >> 16); no WiFi image writes them, the
 * hardware reset vectors apply. Never exercised. [inferred] */
#define MT7996_MCU_CFG_LS_WM_BOOT_ADDR    0x88000060u
#define MT7996_MCU_CFG_LS_WA_BOOT_ADDR    0x88000064u

/* ========================================================================
 * MCU clocks
 * ======================================================================== */
/*
 * The clock registers are defined in conn.h: CPU dividers
 * MT7996_WF_WM_CLK_DIV / MT7996_WF_WA_CLK_DIV (rate = 1440 / (D + 2) MHz,
 * live 0x00030001 on both, D = 0: 720 MHz, the firmware maximum; raising
 * it means changing the PLL), the frequency meter MT7996_WF_FREQ_METER and
 * its source select MT7996_WF_FREQ_CAL_SRC, the per-band WPLL request
 * blocks MT7996_WFSYS_ON_LPCR*(n) (ready = bit 26; DIGCK enable/disable =
 * bit 1), the oscillator grant/request words, and the power word at
 * 0x81026004 (live 0x0b201c11).
 *
 * Measured with the meter (counts per 32.768 kHz period; CPU sources are
 * divided by 8): crystal 1220 = 40.0 MHz; WM CPU 2746 and WA CPU 2746 =
 * 90.0 MHz = 720 / 8; PHY source 1220 = 40 MHz; bus 9767 = 320 MHz.
 * Measured with mcycle against the DSP block's 1 MHz counter: WM 720.0,
 * WA 719.9, DSP 640.0 MHz. [measured]
 * The ROM's PLL-source selector value 3 returns the constant 640 MHz without
 * a read. The ROM rate rule: D = (2 * PLL / request) - 2, real =
 * floor(2 * PLL / (D + 2)), one D++ if above the cap. [static]
 * The ROM polls WPLL ready up to 1000 times with 1 us delays. [static]
 * HAZARD: the ROM's meter READY loop has no timeout. [static]
 * HAZARD: disabling a WPLL (or a PHY clock gate) is not quiescence of a
 * producer or DMA engine still clocked by it. [static]
 */

/*
 * Firmware clock words (retail ROM data in ULM2; read live, never hard-code
 * from another ROM revision) [measured]: g_mcu_real_clock_rate 0x022002ec =
 * 720,000,000; cap 0x02200020 = 720 MHz; MCU source PLL 0x02200028 = 720
 * MHz; WA rate 0x022082c4 = 720 MHz; crystal 0x02209c44 = 40,000,000. The
 * Wi-Fi subsystem requests 760 MHz (0x02209abe) and is clamped to the cap.
 * Bus source PLL 640 MHz (crystal 40 MHz -> BPLL 640 MHz). DSPs run at
 * 640 MHz (same source inferred).
 */

#endif /* HW_MT7996_MCU_PERIPH_H */
