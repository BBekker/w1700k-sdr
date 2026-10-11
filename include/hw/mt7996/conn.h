/* SPDX-License-Identifier: MIT */
/*
 * MT7996 connectivity infrastructure and top-level WiFi-subsystem control:
 * CONN_INFRA (clocks, low power, hardware semaphores, A-die clock gate and
 * A-die SPI masters), the CBTOP 0x70xxxxxx block (WF subsystem reset, strap,
 * revision), WF_TOP_MISC_ON / _OFF (oscillator grants, power requesters,
 * bus sleep protection, clock dividers and meter), the WFSYS_ON low-power
 * block and the ROM's GCI register-field layer.
 *
 * ADDRESS SPACE: device bus addresses as the MCU uses them.
 *   CONN_INFRA       0x7c000000-0x7c3fffff (MCU view). The host reaches it
 *                    through the L1 window as 0x18000000 + offset (host.h);
 *                    a raw 0x7c... base programmed into L1 reads 0. The
 *                    first 64 KiB is also BAR 0xf0000, 0x7c020000 is BAR
 *                    0xd0000 and 0x7c060000 BAR 0xe0000 (fixed map).
 *   CBTOP            0x70000000-0x77ffffff, host via L1 (same value).
 *   WF_TOP_MISC_OFF  0x80020000, host via fixed BAR 0xb0000 (64 KiB).
 *   WF_TOP_MISC_ON   0x81020000, host via fixed BAR 0xc0000 (64 KiB).
 *   WFSYS_ON         0x81030000, host via L2 only.
 * The WF_TOP_MISC windows and L2 are subject to 0xdead2117 poisoning
 * (host.h); the firmware register proxy (mcu_cmd.h) reaches all of these.
 *
 * Unbounded hardware polls in the ROM are noted per register: a host-side
 * reimplementation must bound them.
 */
#ifndef HW_MT7996_CONN_H
#define HW_MT7996_CONN_H

#include "../hw_bits.h"

#define MT7996_CONN_INFRA_BASE			0x7c000000u
#define MT7996_CONN_INFRA_SIZE			0x00400000u
#define MT7996_CONN_INFRA_L1_BASE		0x18000000u	/* host L1 view */

/* ======================================================================== */
/* CONN_INFRA configuration (0x7c000000 page)                               */
/* ======================================================================== */

/* Low-power setting words written by the ROM connsys start-up (+0x50 = 0,
 * +0x54 = 7). Read 0x00000000 / 0x00000007. RW.             [measured, static] */
#define MT7996_CONN_CFG_LP0			0x7c000050u
#define MT7996_CONN_CFG_LP1			0x7c000054u

/* Three words that each read 0x0000000f (bits 3:0 only), unchanged across a
 * firmware bring-up. NOT the oscillator mask (that is MT7996_CONN_OSC_MASK);
 * writes of bits 20/22/23/31 at +0x104 do not latch.             [measured] */
#define MT7996_CONN_CFG_100			0x7c000100u
#define MT7996_CONN_CFG_104			0x7c000104u
#define MT7996_CONN_CFG_108			0x7c000108u
#define   CONN_CFG_10X_VALUE			0x0000000fu

/* WFSYS software reset. RW (b4 RO). Reads 0x11 in every state observed
 * (pre-firmware, firmware up, after WF subsystem reset, poisoned). A pulse
 * 0x11 -> 0x10 -> 0x11 takes effect and INIT_DONE returns. Firmware never
 * writes it; the ROM start-up clears bit 5.                 [measured, static]
 * A firmware-proxy SET here was refused with status 0x00010003. [measured] */
#define MT7996_CONN_WFSYS_SW_RST		0x7c000140u
#define   CONN_WFSYS_SW_RST_B			HW_BIT(0)	/* 1 = released */
#define   CONN_WFSYS_SW_INIT_DONE		HW_BIT(4)	/* HW-published */
#define   CONN_WFSYS_SW_RST_BIT5		HW_BIT(5)	/* cleared by ROM start-up */

/* ROM connsys low-power setting (writes in this order, each a fresh RMW):
 *   0x7c001300 = (r & 0xf803ffff) | 0x001c0000
 *   0x7c001300 = (r & 0xfffc01ff) | 0x00000c00
 *   0x7c001300 = (r & 0xfffffe00) | 5
 *   0x7c001334 = (r & 0xfffffe00) | 7
 *   0x7c001304 &= ~bit 16;  0x7c001334 &= ~bit 16
 *   0x7c000050 = 0;  0x7c000054 = 7                                [static] */
#define MT7996_CONN_LP_CTL0			0x7c001300u
#define   CONN_LP_CTL0_F26_18			HW_GENMASK(26, 18)
#define   CONN_LP_CTL0_F17_9			HW_GENMASK(17, 9)
#define   CONN_LP_CTL0_F8_0			HW_GENMASK(8, 0)
#define MT7996_CONN_LP_CTL1			0x7c001304u
#define MT7996_CONN_LP_CTL2			0x7c001334u

/* ROM "switch mode" enable: 0x7c048320 |= 0x00802008, 0x7c048400 = 0xc. [static] */
#define MT7996_CONN_SWITCH_MODE0		0x7c048320u
#define MT7996_CONN_SWITCH_MODE1		0x7c048400u

/* CMDBT: ROM writes three pointers (0x18050080, 0x18050280, 0x18050400) and
 * copies 4 KiB of ROM data to 0x7c050000. Engine semantics undecoded. [static] */
#define MT7996_CONN_CMDBT_PTR0			0x7c011050u
#define MT7996_CONN_CMDBT_PTR1			0x7c011080u
#define MT7996_CONN_CMDBT_PTR2			0x7c011084u
#define MT7996_CONN_CMDBT_DATA_BASE		0x7c050000u
#define MT7996_CONN_CMDBT_DATA_SIZE		0x00001000u

/* ======================================================================== */
/* CONN_INFRA clock control (0x7c012000 page)                               */
/* ======================================================================== */

/* Bus divider setup (ROM, in order): +0x008 |= 1; +0x050 |= bit 29;
 * +0x050 |= bit 31; +0x038 |= 4; caller then clears +0x050 bits 0 and 16.
 * Start-up also writes +0x064 = 0xffffffff and +0x074 = 0x00ffffff.
 * Live: +0x008 = 0x00000009, +0x038 = 0x05050004, +0x050 = 0xb1c04180.
 * Field meanings undecoded.                                 [measured, static] */
#define MT7996_CONN_CLK_BASE			0x7c012000u
#define MT7996_CONN_CLK_DIV_008			0x7c012008u
#define MT7996_CONN_CLK_DIV_038			0x7c012038u
#define MT7996_CONN_CLK_DIV_050			0x7c012050u
#define MT7996_CONN_CLK_064			0x7c012064u
#define MT7996_CONN_CLK_074			0x7c012074u

/* CONN frequency counter. RW. Sequence: clear START; SRC = source; set EN;
 * WIN = 6; set START; poll READY; read COUNT; clear START, clear EN.
 * f = floor(COUNT * 32768 / 6) Hz. ROM READY poll is UNBOUNDED; bound it.
 * Measured: OSC 40.004 MHz (count 7325), HCLK 160.033 MHz, WPLL 160.033 MHz,
 * BPLL never ready.                                         [measured, static] */
#define MT7996_CONN_FREQ_METER			0x7c012090u
#define   CONN_FREQ_METER_SRC			HW_GENMASK(1, 0)
#define     CONN_FREQ_METER_SRC_OSC		0u
#define     CONN_FREQ_METER_SRC_HCLK		1u
#define     CONN_FREQ_METER_SRC_BPLL		2u
#define     CONN_FREQ_METER_SRC_WPLL		3u
#define   CONN_FREQ_METER_EN			HW_BIT(2)
#define   CONN_FREQ_METER_WIN			HW_GENMASK(6, 4)	/* written 6 */
#define   CONN_FREQ_METER_START			HW_BIT(7)
#define   CONN_FREQ_METER_COUNT			HW_GENMASK(23, 8)	/* RO */
#define   CONN_FREQ_METER_READY			HW_BIT(24)	/* RO */

/* Clock presence. RW START, RO status valid only while START is held.
 * Measured 0 -> 0x5 -> 0: WPLL present, BPLL absent.        [measured, static] */
#define MT7996_CONN_CLK_DETECT			0x7c012094u
#define   CONN_CLK_DETECT_START			HW_BIT(0)
#define   CONN_CLK_DETECT_BPLL			HW_BIT(1)
#define   CONN_CLK_DETECT_WPLL			HW_BIT(2)

/* ======================================================================== */
/* CONN AFE, per band (0x7c043000 + band * 0x1000)                          */
/* ======================================================================== */
/* Listed for its hazard; the analog fields belong to the PHY/capture view. */
#define MT7996_CONN_AFE_BASE(band)		(0x7c043000u + (band) * 0x1000u)
/* The patch init's AFE calibration pulse (above, "The ROM patch's radio
 * init", step 10) on CTL00 and CTL08, and the clock-mode fields of CTL04 /
 * CTL0C (ROM top_pos sets 2 / 2 / 2 for a 40 MHz crystal; the patch clears
 * them on bands 0 and 1). Names ours [guess]. The pulse leaves status bits
 * the host sees (+0x20 high bits, +0x80 / +0x9c / +0xb8 / +0xd4 [31]) that
 * no write reaches. [static][measured] */
#define MT7996_CONN_AFE_CTL00(band)		(0x7c043000u + (band) * 0x1000u)
#define   CONN_AFE_CTL00_PULSE			HW_BIT(0)
#define   CONN_AFE_CTL00_CAL20			HW_BIT(20)
#define   CONN_AFE_CTL00_CAL19			HW_BIT(19)
#define   CONN_AFE_CTL00_CAL18			HW_BIT(18)
#define   CONN_AFE_CTL00_CAL17			HW_BIT(17)
#define   CONN_AFE_PULSE_TICKS			2u	/* cos_delay_time */
#define   CONN_AFE_CAL_TICKS			0x1bu
#define MT7996_CONN_AFE_CTL04(band)		(0x7c043004u + (band) * 0x1000u)
#define   CONN_AFE_CTL04_LO			HW_GENMASK(1, 0)
#define   CONN_AFE_CTL04_HI			HW_GENMASK(17, 16)
#define   CONN_AFE_CTL04_E1_VAL			2u	/* both fields, E1 path */
#define MT7996_CONN_AFE_CTL0C(band)		(0x7c04300cu + (band) * 0x1000u)
#define   CONN_AFE_CTL0C_MODE			HW_GENMASK(18, 17)
#define   CONN_AFE_CTL0C_E1_VAL			2u
#define MT7996_CONN_AFE_CTL08(band)		(0x7c043008u + (band) * 0x1000u)
#define   CONN_AFE_CTL08_CAL			HW_BIT(20)
#define   CONN_AFE_CTL08_ADC_STOP		HW_BIT(24)
#define   CONN_AFE_CTL08_NORMAL			0x00006666u
/* HAZARD: setting CTL08 bit 24 stops that band's ADC, and it SURVIVES a radio
 *         reload (the A-dies and CONN AFE are not reset by the WF subsystem
 *         reset); no stream starts until the word is rewritten to 0x00006666
 *         and the radio reloaded.                                  [measured] */

/* ======================================================================== */
/* CONN hardware semaphores (0x7c070000)                                    */
/* ======================================================================== */
/*
 * A READ of ACQUIRE(i) is an acquire attempt: granted iff
 * (value & 0xfd) == 1. Writing 1 to RELEASE(i) releases. ROM acquire loop:
 * 1000 tries x ms with 1 us delays. Index 12 serialises A-die SPI (ROM and
 * firmware take it around every RF register transaction, IRQs saved);
 * index 0x11 is used by the WA handler.                             [static]
 *
 * HAZARD: NEVER READ ACQUIRE(12) (0x7c070030) from the host or through the
 *         register proxy: the read takes the A-die SPI semaphore and every
 *         later firmware RF access fails (status 3). Exclude the whole
 *         0x7c070000 page from sweeps; RELEASE words are uncharacterised.
 *                                                                    [static]
 */
#define MT7996_CONN_SEMA_BASE			0x7c070000u
#define MT7996_CONN_SEMA_ACQUIRE(i)		(0x7c070000u + (i) * 4u)
#define MT7996_CONN_SEMA_RELEASE(i)		(0x7c070200u + (i) * 4u)
#define   CONN_SEMA_GRANTED_MASK		0xfdu
#define   CONN_SEMA_GRANTED			0x01u
#define MT7996_CONN_SEMA_ADIE_SPI		12u
#define MT7996_CONN_SEMA_WA			0x11u

/* ======================================================================== */
/* A-die clock block (0x7c091000, 0x7c092000)                               */
/* ======================================================================== */

/* A-die block words. RW. ROM A-die power-on does &= 0x0002ffff on both.
 * Read 0x0180ee01; after that mask 0x0000ee01.
 * HAZARD: an edit PERSISTS across the WF subsystem reset and a full vendor
 *         bring-up; only a power cycle restores it.               [measured] */
#define MT7996_CONN_ADIE_BLK0_CTL		0x7c091004u
#define MT7996_CONN_ADIE_BLK1_CTL		0x7c092004u
#define   CONN_ADIE_BLK_POWERON_KEEP		0x0002ffffu

/* A-die clock gate. b0 RW enable, b1 RO BUSY (not an acknowledge). The ROM
 * writes b0 then polls up to 100 x 5 us WHILE b1 is set. 0x00000001 =
 * enabled and idle, observed in every state.               [measured, static] */
#define MT7996_CONN_ADIE_CLK_GATE		0x7c091124u
#define   CONN_ADIE_CLK_GATE_EN			HW_BIT(0)
#define   CONN_ADIE_CLK_GATE_BUSY		HW_BIT(1)
/* Change across firmware bring-up: 0x7c0910a8 0x1 -> 0x203, 0x7c091118
 * 0 -> 0x30000000; undecoded.                                     [measured] */

/* ======================================================================== */
/* A-die SPI masters (connsys side), one per A-die                          */
/* ======================================================================== */
/*
 * Master for die d (0..2; die 0 = MT7976 2.4 GHz, dies 1/2 = MT7977) at
 * 0x7c098000 + d * 0x1000 (host L1 0x18098000 + d * 0x1000). Writing WDATA
 * launches the transaction (no start bit); a read writes WDATA = 0 after
 * setting CMD. Poll BUSY (ROM: 100000 reads, no delay). Take CONN semaphore
 * 12 around every transaction. Chip-select by chain k: k0 CS1, k1 CS0,
 * k2 CS6, k3 CS7, k4 CS3, TOP CS5 (TOP channel registers).  [static, measured]
 * Chip IDs read through this path: die 0 0x79768a20, dies 1/2 0x79778a10.
 *                                                                 [measured]
 * HAZARD: MT7977 CS6/CS7 (k2/k3) do not answer: reads return a garbage
 *         pattern. TOP index 0x222 on die 1 has wedged that die's SPI read
 *         path past a WF subsystem reset (power cycle needed): never sweep
 *         TOP 0x200..0x2ff. The A-dies are not reset by the WF subsystem
 *         reset.                                                  [measured]
 */
#define MT7996_CONN_SPI_BASE(d)			(0x7c098000u + (d) * 0x1000u)
#define MT7996_CONN_SPI_STA(d)			(0x7c098000u + (d) * 0x1000u)
#define   CONN_SPI_STA_CH0_BUSY			HW_BIT(1)
#define   CONN_SPI_STA_CH1_BUSY			HW_BIT(2)
#define   CONN_SPI_STA_TOP_BUSY			HW_BIT(5)
/* Master config; reads 0x20109008 on all three. Retail ROM sets bit 15 on
 * master 0 only.                                            [measured, static] */
#define MT7996_CONN_SPI_CFG(d)			(0x7c098004u + (d) * 0x1000u)
#define   CONN_SPI_CFG_EN			HW_BIT(15)
/* Channel command: RW, holds the last command. */
#define MT7996_CONN_SPI_CH0_CMD(d)		(0x7c098010u + (d) * 0x1000u)
#define MT7996_CONN_SPI_CH0_WDATA(d)		(0x7c098014u + (d) * 0x1000u)
#define MT7996_CONN_SPI_CH0_RDATA(d)		(0x7c098018u + (d) * 0x1000u)	/* RO */
#define MT7996_CONN_SPI_CH1_CMD(d)		(0x7c098020u + (d) * 0x1000u)
#define MT7996_CONN_SPI_CH1_WDATA(d)		(0x7c098024u + (d) * 0x1000u)
#define MT7996_CONN_SPI_CH1_RDATA(d)		(0x7c098028u + (d) * 0x1000u)
#define MT7996_CONN_SPI_TOP_CMD(d)		(0x7c098050u + (d) * 0x1000u)
#define MT7996_CONN_SPI_TOP_WDATA(d)		(0x7c098054u + (d) * 0x1000u)
#define MT7996_CONN_SPI_TOP_RDATA(d)		(0x7c098058u + (d) * 0x1000u)
#define   CONN_SPI_CMD_ADDR			HW_GENMASK(11, 0)	/* register index */
#define   CONN_SPI_CMD_READ			HW_BIT(12)
#define   CONN_SPI_CMD_CS			HW_GENMASK(15, 13)
#define   CONN_SPI_CMD_KEEP			HW_GENMASK(17, 16)	/* preserved, undecoded */

/* ======================================================================== */
/* CBTOP (0x70000000)                                                       */
/* ======================================================================== */

/* Pad strap. RO. [16:15] A-die combination: 0 = MT7976 + 2 x MT7977 (the
 * W1700K population by chip id), 1/2/3 other combinations. Bit 15 also gates
 * the A-die variant of the patch eFuse recovery (fw_format.h).  [static] */
#define MT7996_CBTOP_PAD_GPIO			0x700056f0u
#define   CBTOP_PAD_GPIO_ADIE_COMB		HW_GENMASK(16, 15)
#define     CBTOP_PAD_GPIO_ADIE_COMB_677	0u
#define     CBTOP_PAD_GPIO_ADIE_COMB_577	1u
#define     CBTOP_PAD_GPIO_ADIE_COMB_607	2u
#define     CBTOP_PAD_GPIO_ADIE_COMB_507	3u
#define   CBTOP_PAD_GPIO_2ADIE_TBTC		HW_BIT(19)
/* MediaTek's patch init does die 2 only when [17] (SKU bit 2) is set. [static] */
#define   CBTOP_PAD_GPIO_DIE2_ONLY		HW_BIT(17)
/* MediaTek's a_die_sku = [19:15] (the ROM patch stores strap >> 15 & 0x1f at
 * boot; the WM reads it through the ROM's pwrCtrlGetGetChipSku, slot
 * 0x0081c324). Bits of the SKU value: [1:0] = CBTOP_PAD_GPIO_ADIE_COMB, [2] =
 * CBTOP_PAD_GPIO_DIE2_ONLY, [3] = CBTOP_PAD_GPIO_B18, [4] =
 * CBTOP_PAD_GPIO_2ADIE_TBTC. The WM's PHY init tests (phy_init.h): [1:0] the
 * A-die combination (picks the AdieSel row and the FEM pin set), [2:0] == 4
 * (AdieSel row 6), [0] (the FEM words take another path in V:e00cd44e, not
 * decoded), [3] (the crystal trim goes to die 0's TOP with other fields,
 * adie.h; not decoded), [4] (FEM type 4 on band 0, no unused-path test).
 * [static] */
#define   CBTOP_PAD_GPIO_SKU			HW_GENMASK(19, 15)
#define     CBTOP_PAD_GPIO_SKU_COMB		HW_GENMASK(1, 0)
#define     CBTOP_PAD_GPIO_SKU_ROW6_MASK	HW_GENMASK(2, 0)
#define       CBTOP_PAD_GPIO_SKU_ROW6		4u
#define     CBTOP_PAD_GPIO_SKU_FEM_ALT		HW_BIT(0)
#define     CBTOP_PAD_GPIO_SKU_XTAL_DIE0	HW_BIT(3)
#define     CBTOP_PAD_GPIO_SKU_TBTC		HW_BIT(4)

/* XO ready: patch init polls [5:2] == 7 (21 x 1 ms).                 [static] */
#define MT7996_CBTOP_XO_STA			0x7000c030u
#define   CBTOP_XO_STA_READY			HW_GENMASK(5, 2)

/* The XO request word polled by XO_STA (MediaTek's patch init, step 6 below):
 * [7:0] = 1. [static] */
#define MT7996_CBTOP_XO_CTL			0x7000c000u
#define   CBTOP_XO_CTL_REQ			HW_GENMASK(7, 0)
#define     CBTOP_XO_CTL_REQ_ON			1u
#define   CBTOP_XO_STA_READY_VAL		7u
#define   CBTOP_XO_READY_TICKS			20u	/* x cos_delay_time(1) */

/* Hardware revision; firmware and host expect 0x8a00.                [static]
 * The WM's DPD calibration (phy_cal.h T2{0..3}) picks its "E1" tables when
 * this reads 0x8a00 and "E2" otherwise (stored as "Current_Ddie_Version"
 * 0x8a00 / 0x8b00 in WM RAM 0xe0081270). MediaTek's stock cold load on this
 * unit runs the E2 tables (step-0 PA code 0x28; E1 gives 0x30), so the WM
 * does not read 0x8a00 here, although firmware and host expect it. The
 * register itself was not read in that trace.                      [inferred] */
#define MT7996_CBTOP_HW_REV			0x70010204u
#define   CBTOP_HW_REV_E1			0x8a00u
/* Unit A reads 0x8b00 (2026-10-11), so neither the patch's E1 path nor the WM's
 * E1 tables run on it. [measured] */
#define   CBTOP_HW_REV_E2			0x8b00u

/* ------------------------------------------------------------------------ */
/* The ROM patch's radio init (MediaTek MCU_Patch_init, vendor patch         */
/* 0x009036c6), which our WM runs at bring-up (firmware/radio/patch_init.c)  */
/* ------------------------------------------------------------------------ */
/*
 * MediaTek runs it from its ROM patch at patch download, before any WM; our
 * patch carries none of it, so our WM does it. The steps, in order (step
 * numbers of the decode, bench/.../spitrace-20261009T2225Z/reports/
 * cold-bringup-report.md section 3), all [static] from the patch listing:
 *  1 CBTOP_27040 = 0xfff; CBTOP_27014 = 0x20; CBTOP_01028 = 1
 *  5 MCU_CFG_210 = 0xe; CBTOP_0015C |= 1
 *  6 XO_CTL [7:0] = 1; poll XO_STA [5:2] == 7, 20 x cos_delay_time(1)
 *  7 E1 only (HW_REV == 0x8a00): CBTOP_00000 [18], [19], HW_BND four
 *    values, HW_BND final, CBTOP_00000 [17], [16], then the AFE CTL04/0C
 *    cycle (below)
 *  9 PINMUX(0..4) = 0x11111111
 * 10 the AFE calibration pulse of each die on its band (adie.h "ROM patch
 *    TOP init"): CTL00 [0] pulse, CTL08 [20] on, CTL00 [20], [19], [18],
 *    [17] on, cos_delay_time(0x1b), off in the same order, CTL08 [20] off,
 *    with the die's analog-interface nibbles raised around it
 * 11 die 0 also on band 1 iff eFuse 0x471 == 0xa, read from die 0's A-die
 *    eFuse (ROM eFuse section 1; secure.h)
 * 12 the A-die TOP init per chip ID (adie.h)
 * 13 CBTOP_25000 [24] = 1, 30 us, [5:0] = 0, [8] = 1, [17:16] = 1
 * 14 AFE CTL04 [1:0] = 0, [17:16] = 0, CTL0C [18:17] = 0 on bands 0 and 1
 *    when the strap's die-2-only bit (CBTOP_PAD_GPIO_DIE2_ONLY) is set, on
 *    band 1 for combinations 2 / 3 (eFuse permitting); on the W1700K's
 *    combination nothing (V:00903816 bbc #2 -> V:00903a22, sku & 3 = 0):
 *    every band keeps the ROM's 2 / 2 / 2 (measured after MediaTek's patch
 *    init, 2026-10-11; the decode report had the condition inverted)
 * 15 PINMUX_320 = 0x11111100
 * 16 the CONN pad words (CONN_PAD_*) in MediaTek's order
 * 17 CONN switch: per k 0..3 SWITCH_SEL byte k = 0xfd, then SWITCH_EN [k]
 * 22 the eFuse trims (adie.h ADIE_TRIM_EFUSE_*, ADIE_TOP_034_TRIM, ADIE_TOP_220_TRIM)
 * Not ours: steps 2 (the patch's own .data/.bss), 3 and 4 (eFuse FT-mapping
 * recovery: HAZARD, burns eFuse), 8 (two-A-die variants), 18 (0x10002000:
 * in our flow the WA image is already downloaded there), 19 (software
 * hooks), 20 (Connsys trace / kmon), 21 (log callbacks, the LVTS thermal
 * interrupt), 23 (timer, console print). Names below are ours [guess] except
 * HW_BND (MediaTek's MT_CBTOP_HW_BND); meanings are not established.
 */
#define MT7996_CBTOP_00000			0x70000000u	/* E1 path only */
#define   CBTOP_00000_E1_B16			HW_BIT(16)
#define   CBTOP_00000_E1_B17			HW_BIT(17)
#define   CBTOP_00000_E1_B18			HW_BIT(18)
#define   CBTOP_00000_E1_B19			HW_BIT(19)
#define MT7996_CBTOP_01028			0x70001028u
#define   CBTOP_01028_INIT			1u
#define MT7996_CBTOP_0015C			0x7000015cu
#define   CBTOP_0015C_EN			HW_BIT(0)
#define MT7996_CBTOP_27040			0x70027040u
#define   CBTOP_27040_INIT			0x00000fffu
#define MT7996_CBTOP_27014			0x70027014u
#define   CBTOP_27014_INIT			0x00000020u
#define MT7996_CBTOP_PINMUX(n)			(0x70005340u + 0x10u * (n))	/* n 0..4 */
#define   CBTOP_PINMUX_INIT			0x11111111u
#define   CBTOP_PINMUX_N			5u
#define MT7996_CBTOP_PINMUX_320			0x70005320u
#define   CBTOP_PINMUX_320_INIT			0x11111100u
/* Observed 0x03010100 after MediaTek's patch init. */
#define MT7996_CBTOP_25000			0x70025000u
#define   CBTOP_25000_B24			HW_BIT(24)
#define   CBTOP_25000_LOW			HW_GENMASK(5, 0)
#define   CBTOP_25000_B8			HW_BIT(8)
#define   CBTOP_25000_MODE			HW_GENMASK(17, 16)	/* = 1 */
#define   CBTOP_25000_SETTLE_US			30u
/* MediaTek: MT_CBTOP_HW_BND. E1 path: four values then the final one, by
 * strap bit 18 (CBTOP_PAD_GPIO). Reads 0 on unit A. */
#define MT7996_CBTOP_HW_BND			0x70010020u
#define   CBTOP_HW_BND_E1_SEQ(strap18)		{ (strap18) ? 0x00101104u : 0x00101175u, \
						  (strap18) ? 0x00001104u : 0x00001175u, \
						  (strap18) ? 0x80001104u : 0x80001175u, \
						  (strap18) ? 0x80101104u : 0x80101175u }
#define   CBTOP_HW_BND_E1_FINAL(strap18)	((strap18) ? 0x80111104u : 0x80111175u)
#define   CBTOP_PAD_GPIO_B18			HW_BIT(18)
#define   CBTOP_E1_STEP_TICKS			0x139u	/* cos_delay_time */
#define   CBTOP_E1_AFE_TICKS			0x7d0u

/* WF MCU configuration word written by the patch init: = 0xe. [static] */
#define MT7996_MCU_CFG_210			0x88000210u
#define   MCU_CFG_210_INIT			0x0000000eu

/* CONN pads (0x7c00e000 page), the patch init's step 16. [static] */
#define MT7996_CONN_PAD_E000			0x7c00e000u
#define MT7996_CONN_PAD_E008			0x7c00e008u
#define MT7996_CONN_PAD_E00C			0x7c00e00cu
#define MT7996_CONN_PAD_E014			0x7c00e014u
#define MT7996_CONN_PAD_E018			0x7c00e018u
#define MT7996_CONN_PAD_E104			0x7c00e104u
#define MT7996_CONN_PAD_E108			0x7c00e108u
#define MT7996_CONN_PAD_E10C			0x7c00e10cu
#define MT7996_CONN_PAD_E118			0x7c00e118u
#define MT7996_CONN_PAD_E11C			0x7c00e11cu
#define   CONN_PAD_B1				HW_BIT(1)
#define   CONN_PAD_B3				HW_BIT(3)
#define   CONN_PAD_B0				HW_BIT(0)
#define   CONN_PAD_LO5				HW_GENMASK(4, 0)	/* E104: 0xa */
#define   CONN_PAD_BYTE0			HW_GENMASK(7, 0)	/* E10C 0xee, E11C 0x8e */
#define   CONN_PAD_BYTE1			HW_GENMASK(15, 8)	/* E10C 0xf4, E11C 0x8e */
#define   CONN_PAD_E104_VAL			0x0au
#define   CONN_PAD_E10C_B0_VAL			0xeeu
#define   CONN_PAD_E10C_B1_VAL			0xf4u
#define   CONN_PAD_E11C_VAL			0x8eu

/* CONN switch (0x7c048000 page; the ROM's switch mode words are above),
 * the patch init's steps 16 and 17: SWITCH_010 [4:0] = 0xa; per k 0..3,
 * SWITCH_SEL byte k = 0xfd, then SWITCH_EN [k]. Observed after it:
 * 0xfdfdfdfd and 0xf. [static][measured] */
#define MT7996_CONN_SWITCH_010			0x7c048010u
#define   CONN_SWITCH_010_LO5			HW_GENMASK(4, 0)
#define   CONN_SWITCH_010_VAL			0x0au
#define MT7996_CONN_SWITCH_EN			0x7c048100u
#define   CONN_SWITCH_EN_K(k)			HW_BIT(k)
#define MT7996_CONN_SWITCH_SEL			0x7c048108u
#define   CONN_SWITCH_SEL_K(k)			(0xffu << (8u * (k)))
#define   CONN_SWITCH_SEL_VAL			0xfdu
#define   CONN_SWITCH_KS			4u

/* WF L0.5 reset. Reads 0.                                  [measured, static] */
#define MT7996_CBTOP_WF_L05_RST			0x70028550u
#define   CBTOP_WF_L05_RST_WF_RST		HW_GENMASK(4, 0)

/* WF subsystem reset. RW. Reads 0x00010340 in every state (b16 = 1,
 * REVERT_CYCLE = 3, b6 = 1). Whole-path reset: set b0, wait 20 ms, clear b0,
 * wait 20 ms. Effects: MCU back to ROM download state (FW_STATE 0), band-0
 * host ownership returns to firmware, oscillator grants revoked (OSC_MASK
 * 0x81300000 -> 0x81000000), host-visible MCU DMA destroyed (so exposed host
 * buffers may be freed after it plus bus-master disable). Does NOT reset the
 * A-dies, CONN AFE, ICAP configuration, 0x7c091004/0x7c092004 or the PCIe
 * link, and does NOT clear 0xdead2117 window poison. Works regardless of MCU
 * state; firmware never writes it.                         [measured, static] */
#define MT7996_CBTOP_WF_SUBSYS_RST		0x70028600u
#define   CBTOP_WF_SUBSYS_RST_WHOLE_PATH	HW_BIT(0)
#define   CBTOP_WF_SUBSYS_RST_REVERT		HW_BIT(5)
#define   CBTOP_WF_SUBSYS_RST_BYPASS_WFDMA_SLP	HW_BIT(6)
#define   CBTOP_WF_SUBSYS_RST_REVERT_CYCLE	HW_GENMASK(15, 8)
#define   CBTOP_WF_SUBSYS_RST_BYPASS_WFDMA2_SLP	HW_BIT(16)
#define   CBTOP_WF_SUBSYS_RST_IDLE		0x00010340u
#define MT7996_CBTOP_WF_SUBSYS_RST_HOLD_MS	20u

/* eFuse controller at 0x70021000: see secure.h (HAZARD). */

/* ======================================================================== */
/* WF_TOP_MISC_ON (0x81020000, BAR 0xc0000)                                 */
/* ======================================================================== */

#define MT7996_WF_TOP_ON_BASE			0x81020000u
#define MT7996_WF_TOP_ON_SIZE			0x00010000u

/* ROM power-on step 4 (0x81020004 |= 0xc00; 0x81020008 |= 0x100, |= 0x2000)
 * and step 5 (0x81020050 |= 0x10, |= 0x1, |= 0x40, then 100 us).
 * DSP core enable/reset words at 0x81020010/14: see dsp.h.          [static] */
#define MT7996_WF_TOP_ON_004			0x81020004u
#define MT7996_WF_TOP_ON_008			0x81020008u
#define MT7996_WF_TOP_ON_050			0x81020050u

/* Per-band bit b (bands 0..2): pulsed 0 then 1 by every calibration tail
 * (sequence at MT7996_CONN_PWR_800). Observed 0x0000000f before and after.
 * Band reset, active low [inferred].                       [static, measured] */
#define MT7996_WF_TOP_ON_BAND_RST		0x8102000cu
#define   WF_TOP_ON_BAND_RST_BAND(b)		HW_BIT(b)

/* CONNCFG sleep control. Reads 0x82000000; page partly live.      [measured] */
#define MT7996_CONN_SLP_CTL			0x81021000u

/* Oscillator grant status (CONNCFG OSC_MASK). Effectively RO from the host:
 * writes of bits 20/22/23/27 read back unchanged. Grants follow firmware
 * state: 0x81000000 pre-firmware / after WF subsystem reset; 0x81300000
 * firmware up, idle; 0x81700000 while receiving (PSE granted, lags the
 * command). Bits 16/18 flicker: read more than once. AP2CONN_WFSYS (b27) and
 * PLE (b23) never observed granted.                         [measured, static]
 * A host read of a PLE/PSE/BN register is safe only while its grant bit is
 * set; otherwise it can hard-lock the host (host.h HAZARD).       [measured] */
#define MT7996_CONN_OSC_MASK			0x81021104u
#define   CONN_OSC_MASK_EN_MASK			HW_GENMASK(15, 0)
#define   CONN_OSC_MASK_BN0_WF_OSC_EN		HW_BIT(20)
#define   CONN_OSC_MASK_BN1_WF_OSC_EN		HW_BIT(21)
#define   CONN_OSC_MASK_PSE_OSC_EN		HW_BIT(22)
#define   CONN_OSC_MASK_PLE_OSC_EN		HW_BIT(23)
#define   CONN_OSC_MASK_MCU_OSC_EN		HW_BIT(24)
#define   CONN_OSC_MASK_AP2CONN_WFSYS_OSC_EN	HW_BIT(27)
#define   CONN_OSC_MASK_OSC_EN_ALL		HW_BIT(31)
#define   CONN_OSC_MASK_PRE_FW			0x81000000u
#define   CONN_OSC_MASK_FW_IDLE			0x81300000u
#define   CONN_OSC_MASK_FW_RX			0x81700000u
/* +0x100 reads 0x3f; +0x110 goes 0 -> 0x00100000 on firmware bring-up. */
#define MT7996_CONN_CFG_ON_100			0x81021100u
#define MT7996_CONN_CFG_ON_110			0x81021110u

/* ROM power-on entry gate: bit 4 set skips the WiFi power-on block.
 * Reads 0x00000001.                                        [measured, static] */
#define MT7996_CONN_PWR_GATE			0x81021600u
#define   CONN_PWR_GATE_SKIP			HW_BIT(4)
/* Power handshake status: ROM polls bit 17, UNBOUNDED. Live 0x00ff0104. */
#define MT7996_CONN_PWR_HS_STA			0x8102161cu
#define   CONN_PWR_HS_STA_ACK			HW_BIT(17)

/*
 * Bus sleep-protection requests and acknowledges [inferred: set, then poll
 * the ack; released on wake]. V:e013100e(enable) walks a 6-entry table
 * {request word, bits, ack word, ack mask} (WM data 0x022243f8..):
 *   0x57000050 |= 0x1     ack 0x57000050 [1]  (WFDMA MCU wrap, mcu_map.h)
 *   0x81023f00 |= 0x4000  ack 0x8102300c [7]
 *   0x81023f00 |= 0x2000  ack 0x8102300c [6]
 *   0x81023f1c |= 0x10    ack 0x81023018 [0]
 *   0x81023f1c |= 0x20    ack 0x81023018 [1]
 *   0x81023f08 |= 0x1000  ack 0x81023f08 [13]
 * enable: set each request and poll ack == mask; disable: clear each and
 * poll ack == 0 (1 us steps, at most 1001 polls in all); around the
 * sequence 0x81023f00 [6:5] and 0x81023f1c [1:0] are forced on, then
 * released. MediaTek's WM runs disable (release) on every wake (mac.h).
 * Stock 0x81023f00 = 0xc0000100, 0x81023f1c = 0, 0x81023f08 = 0x00140008,
 * acks 0x8102300c = 0x3a07, 0x81023018 = 0. Names [guess]. Needed only by
 * a firmware that sleeps.                                  [static, measured]
 */
#define MT7996_WF_TOP_ON_SLPPROT_REQ0		0x81023f00u
#define   WF_TOP_ON_SLPPROT_REQ0_FORCE		HW_GENMASK(6, 5)
#define   WF_TOP_ON_SLPPROT_REQ0_A		HW_BIT(13)
#define   WF_TOP_ON_SLPPROT_REQ0_B		HW_BIT(14)
#define MT7996_WF_TOP_ON_SLPPROT_REQ2		0x81023f08u
#define   WF_TOP_ON_SLPPROT_REQ2_REQ		HW_BIT(12)
#define   WF_TOP_ON_SLPPROT_REQ2_ACK		HW_BIT(13)
/* MediaTek's radar detector start (rdmRddStart, the channel set's BW320 background receiver,
 * phy_chan.h) writes this whole word, no read, before anything else (V:e00ad54a..V:e00ad554):
 * the word's stock value (REQ [12] clear). [static][measured] */
#define   WF_TOP_ON_SLPPROT_REQ2_RDD_START	0x00140008u
#define MT7996_WF_TOP_ON_SLPPROT_REQ1		0x81023f1cu
#define   WF_TOP_ON_SLPPROT_REQ1_FORCE		HW_GENMASK(1, 0)
#define   WF_TOP_ON_SLPPROT_REQ1_A		HW_BIT(4)
#define   WF_TOP_ON_SLPPROT_REQ1_B		HW_BIT(5)
#define MT7996_WF_TOP_ON_SLPPROT_ACK0		0x8102300cu
#define   WF_TOP_ON_SLPPROT_ACK0_A		HW_BIT(6)	/* REQ0_A */
#define   WF_TOP_ON_SLPPROT_ACK0_B		HW_BIT(7)	/* REQ0_B */
#define MT7996_WF_TOP_ON_SLPPROT_ACK1		0x81023018u
#define   WF_TOP_ON_SLPPROT_ACK1_A		HW_BIT(0)	/* REQ1_A */
#define   WF_TOP_ON_SLPPROT_ACK1_B		HW_BIT(1)	/* REQ1_B */

/* ConnCfg power requester. ROM: 0x81026004 &= ~0x100; +0x000 |= 1; |= 2;
 * poll 0x8102161c b17; poll +0x000 b3 -- both polls UNBOUNDED.
 * Live +0x000 = 0x0000002b.                                [measured, static] */
#define MT7996_CONN_PWR_REQ			0x81026000u
#define   CONN_PWR_REQ_B0			HW_BIT(0)
#define   CONN_PWR_REQ_B1			HW_BIT(1)
#define   CONN_PWR_REQ_DONE			HW_BIT(3)
/* Bus clock divider / power-on step 2 word (|= 0x1000). Live 0x0b201c11. */
#define MT7996_CONN_PWR_004			0x81026004u
/* PLE/PSE oscillator REQUEST (the ROM's last power-on write, |= 0x00c00000).
 * 0x00002002 pre-firmware -> 0x00c10002 with firmware up: requested, not
 * granted (see OSC_MASK).                                  [measured, static] */
#define MT7996_CONN_OSC_REQ			0x81026104u
#define   CONN_OSC_REQ_PSE			HW_BIT(22)
#define   CONN_OSC_REQ_PLE			HW_BIT(23)
/* Power-on step 3 group: each &= ~0xfc, |= 1, then |= 0x100 (0x108) or
 * |= 0x708100 (0x10c, 0x110). Firmware-up values 0x88138501 (0x108),
 * 0x00708101 (0x10c/0x110/0x11c).                          [measured, static] */
#define MT7996_CONN_PWR_108			0x81026108u
#define MT7996_CONN_PWR_10C			0x8102610cu
#define MT7996_CONN_PWR_110			0x81026110u
#define MT7996_CONN_PWR_11C			0x8102611cu
#define   CONN_PWR_1XX_CLR			HW_GENMASK(7, 2)
#define   CONN_PWR_1XX_GATE			HW_BIT(8)	/* 0x10c band 0, 0x110 band 1 */
/* Step 3 also does |= 0x2a then |= 0xa80. Per-band 6-bit fields and a shared
 * [13:12]; observed 0x07f50aaa (each band field 0x2a) before and after a
 * calibration.                                              [static, measured] */
#define MT7996_CONN_PWR_800			0x81026800u
#define   CONN_PWR_800_BAND0			HW_GENMASK(5, 0)
#define   CONN_PWR_800_BAND1			HW_GENMASK(11, 6)
#define   CONN_PWR_800_BAND2			HW_GENMASK(20, 15)
#define   CONN_PWR_800_SHARED			HW_GENMASK(13, 12)
/* The per-band gate sequence (below) as lists: the bits of MT7996_CONN_PWR_800 the
 * closing helper clears, one read-modify-write each in this order, before it clears
 * CONN_PWR_800_SHARED; the bits the opening helper sets. Band 0: V:e00487ea /
 * V:e00488dc; band 1: V:e004882e / V:e0048926; band 2: V:e004887e / V:e0048974 (the
 * calibration tail); the channel-set RX setup's helpers write the same (phy_cal.h).
 * The gate word around them: band 0 / 1 MT7996_CONN_PWR_10C / _110 [8]
 * (CONN_PWR_1XX_GATE, CONN_PWR_GATE_REG) cleared, then set; band 2 its WF PLL
 * clock instead, WFSYS_ON_LPCR_DIS_PLL_CKEN to MT7996_WFSYS_ON_LPCR_DIS(2), then
 * WFSYS_ON_LPCR_EN_PLL_CKEN to MT7996_WFSYS_ON_LPCR_EN(2). Between closing and
 * opening 10 us, then 1 us (the opening helper), then the band reset pulse
 * MT7996_WF_TOP_ON_BAND_RST. [static][measured] */
#define CONN_PWR_800_GATE_CLR_B0		{ HW_BIT(3), HW_BIT(5), HW_BIT(0), HW_BIT(2), HW_BIT(4) }
#define CONN_PWR_800_GATE_SET_B0		{ HW_BIT(3), HW_BIT(5) }
#define CONN_PWR_800_GATE_CLR_B1		{ HW_BIT(9), HW_BIT(11), HW_BIT(6), HW_BIT(8), HW_BIT(10) }
#define CONN_PWR_800_GATE_SET_B1		{ HW_BIT(9), HW_BIT(11) }
#define CONN_PWR_800_GATE_CLR_B2		{ HW_BIT(16), HW_BIT(18), HW_BIT(20), HW_BIT(19), HW_BIT(17), \
					  HW_BIT(15) }
#define CONN_PWR_800_GATE_SET_B2		{ HW_BIT(16), HW_BIT(18), HW_BIT(20) }
#define CONN_PWR_GATE_BAND_LPCR		2u
#define CONN_PWR_GATE_REG(b)			((b) == 0u ? MT7996_CONN_PWR_10C : MT7996_CONN_PWR_110)
#define CONN_PWR_GATE_HOLD_US			10u
#define CONN_PWR_GATE_RST_US			1u

/* Per-band WFSYS gate sequence of every calibration tail (V:e00489c2, after
 * the 430 D-die words; phy_cal.h): [static, measured]
 *  band 0: 0x8102610c [8] = 0; 0x81026800 [3], [5], [0], [2], [4], [13:12] =
 *          0; 10 us; 1 us; 0x8102000c [0] = 0 then 1; 0x81026800 [3], [5] =
 *          1; 0x8102610c [8] = 1.
 *  band 1: 0x81026110 [8] = 0; 0x81026800 [9], [11], [6], [8], [10], [13:12]
 *          = 0; 10 us; 1 us; 0x8102000c [1] pulse; 0x81026800 [9], [11] = 1;
 *          0x81026110 [8] = 1.
 *  band 2: 0x81033008 = 0x10; 0x81026800 [16], [18], [20], [19], [17], [15],
 *          [13:12] = 0; 10 us; 1 us; 0x8102000c [2] pulse; 0x81026800 [16],
 *          [18], [20] = 1; 0x81033004 = 0x10.
 * Registers: MT7996_CONN_PWR_10C / _110 (CONN_PWR_1XX_GATE), _PWR_800,
 * MT7996_WF_TOP_ON_BAND_RST, and for band 2 the WFSYS_ON band-2 clear / set
 * pair MT7996_WFSYS_ON_LPCR_DIS(2) / _EN(2) with WFSYS_ON_LPCR_EN_PLL_CKEN
 * (0x10). These are WM writes; the host-write HAZARD on WFSYS_ON stands.
 * Observed 0x8102610c / 0x81026110 = 0x00708101, 0x8102000c = 0xf, before
 * and after. The channel-set D-die RX calibration (phy_cal.h; V:e00ddad2..,
 * V:e00ddb2e.., V:e00ddb8e.. on bands 0 / 1 / 2) runs the same field
 * sequence on every channel set, with its PHY words between closing the
 * gate and the band reset; the same values before and after (0x07f50aaa,
 * 0x00708101, 0xf). [static][measured] */

/* EEPROM/eFuse control word; the stock host reads it (bit 30 = eFuse in
 * use). Host reaches it through L2.                                 [static] */
#define MT7996_WF_EE_CTRL			0x81070000u
#define   WF_EE_CTRL_USE_EFUSE			HW_BIT(30)

/* ======================================================================== */
/* WF_TOP_MISC_OFF (0x80020000, BAR 0xb0000)                                */
/* ======================================================================== */
/* ICAP instances at 0x80023000+: see icap.h. */

#define MT7996_WF_TOP_OFF_BASE			0x80020000u
#define MT7996_WF_TOP_OFF_SIZE			0x00010000u

/* WF frequency meter. RW. Start: SEL, WIN = 1, START, EN; poll READY (ROM
 * poll UNBOUNDED); COUNT. ROM: f = COUNT * 32000; scale against a crystal
 * (40 MHz) count from the same meter instead. Rest value 0.  [measured, static] */
#define MT7996_WF_FREQ_METER			0x80020100u
#define   WF_FREQ_METER_COUNT			HW_GENMASK(15, 0)	/* RO */
#define   WF_FREQ_METER_READY			HW_BIT(16)	/* RO */
#define   WF_FREQ_METER_WIN			HW_GENMASK(22, 20)
#define   WF_FREQ_METER_EN			HW_BIT(23)
#define   WF_FREQ_METER_START			HW_BIT(27)
#define   WF_FREQ_METER_SEL			HW_GENMASK(29, 28)
#define     WF_FREQ_METER_SEL_XTAL		0u
#define     WF_FREQ_METER_SEL_CAL		1u	/* source in WF_FREQ_CAL_SRC */
#define     WF_FREQ_METER_SEL_BUS		2u

/* WM CPU clock divider: rate = 1440 / (D + 2) MHz, D in [7:2]; D = 0
 * (720 MHz) is the firmware cap. WA divider at +4. The ROM leaves the WM at
 * 0x0003001d (D = 7, 160 MHz) and the WA word at 0; the vendor firmware sets
 * both to 0x00030001, and so does ours.                    [measured, static] */
#define MT7996_WF_WM_CLK_DIV			0x8002e014u
#define MT7996_WF_WA_CLK_DIV			0x8002e018u
#define   WF_CPU_CLK_DIV_D			HW_GENMASK(7, 2)
#define     WF_CPU_CLK_DIV_D_720MHZ		0u
/* The other bits of 0x00030001; set in every value seen (ROM and vendor). */
#define   WF_CPU_CLK_DIV_EN			HW_BIT(0)	/* [guess] name */
#define   WF_CPU_CLK_DIV_SRC			HW_GENMASK(17, 16)	/* [guess] name */
#define     WF_CPU_CLK_DIV_SRC_PLL		3u	/* [guess] name */

/* Frequency-meter calibration source. Rest value 0x02101000. */
#define MT7996_WF_FREQ_CAL_SRC			0x8002e01cu
#define   WF_FREQ_CAL_SRC_GATE			HW_BIT(6)
#define   WF_FREQ_CAL_SRC_SEL			HW_GENMASK(19, 17)
#define     WF_FREQ_CAL_SRC_WM			0u
#define     WF_FREQ_CAL_SRC_WA			1u
#define     WF_FREQ_CAL_SRC_EXT_PLL		2u
#define     WF_FREQ_CAL_SRC_WFDMA_CLK		3u
#define     WF_FREQ_CAL_SRC_WM_CPU		4u
#define     WF_FREQ_CAL_SRC_WA_CPU		5u
#define     WF_FREQ_CAL_SRC_WF_LMAC		6u
#define     WF_FREQ_CAL_SRC_WF_PHY		7u

/* Power-on step 1 (|= 0x2000 then |= 0x200, separate RMWs) and later
 * |= 0x04000000. Firmware-up value 0x040c2300.            [measured, static] */
#define MT7996_WF_TOP_OFF_E108			0x8002e108u

/* ======================================================================== */
/* WFSYS_ON low-power block (0x81030000), L2 only                           */
/* ======================================================================== */
/*
 * HAZARD: NO HOST WRITES ANYWHERE IN 0x81030000-0x8103FFFF. A write can wedge
 *         the part until power is removed; the MCU executing the ROM's
 *         WFSYS_ON power-on sequence left every WiFi-die host window poisoned
 *         (0xdead2117) until a power cycle. Reads of +0x48, 0x81031000/04/
 *         08/14 and 0x81032000 have been clean.                     [measured]
 * HAZARD: the APB2 alias 0x820b0000 (BAR 0xae000) is quarantined for reads
 *         (recorded behind an existing wedge, not cleared).          [measured]
 */
#define MT7996_WFSYS_ON_BASE			0x81030000u
#define MT7996_WFSYS_ON_SMCTL1			0x81030018u	/* WPLL stable / PON time */
#define MT7996_WFSYS_ON_SER_RST			0x81030044u
/* ROM |= 0xf000. 0 after reset -> 0x0000ffcf after a vendor bring-up. */
#define MT7996_WFSYS_ON_PREON_CTL1		0x81030048u
/* Per band block: 0x81031000 (BN0), 0x81032000 (BN1), 0x81033000 (IP1_BN0). */
#define MT7996_WFSYS_ON_BAND(n)			(0x81031000u + (n) * 0x1000u)
/* LPCR: 0x88600001 after reset -> 0x8c600013 after vendor bring-up (stock
 * 0x8c600013 on all three). RO status; field names from the ROM's GCI
 * module 0x26 (+0x340 per band block).                    [measured, static] */
#define MT7996_WFSYS_ON_LPCR(n)			(0x81031000u + (n) * 0x1000u)
#define   WFSYS_ON_LPCR_STS_CFG_B0_VALID	HW_BIT(31)
#define   WFSYS_ON_LPCR_LP_OWN_BUSY		HW_BIT(30)	/* [guess] name: the leave waits for 0 */
#define   WFSYS_ON_LPCR_LP_OWN_STAT		HW_BIT(29)
#define   WFSYS_ON_LPCR_STS_LP0_SLEEP		HW_BIT(28)
#define   WFSYS_ON_LPCR_STS_LP0_VALID		HW_BIT(27)
#define   WFSYS_ON_LPCR_WPLL_RDY		HW_BIT(26)	/* RO */
/* Set register (reads 0): b1 WF digital clock, b4 WF PLL clock; ROM also
 * writes b31, b22, b0. Clear register: b1, ROM |= 0x10.     [measured, static]
 * GCI names of both words (same layout): [31] B0_VALID, [30] LP0_OWN, [21]
 * SX_ZPS_MODE, [0] DSLEEP_REQ_B.                                    [static]
 * Every band-2 calibration tail writes 0x10 to DIS(2), then to EN(2) (the
 * WFSYS gate sequence at MT7996_CONN_PWR_800).                      [static]
 * The per-band PHY init (V:e00de5f6, phy.h "Per-band PHY init"), on every
 * band's pass, writes 0x10 to DIS(0), DIS(1), EN(0), EN(1) in that order:
 * the BN0 and BN1 WF PLL clocks gated off and on (fixed addresses; IP1_BN0
 * not touched). Both are WM writes; the host-write HAZARD above stands.
 *                                                         [static, measured] */
#define MT7996_WFSYS_ON_LPCR_EN(n)		(0x81031004u + (n) * 0x1000u)
#define   WFSYS_ON_LPCR_EN_B0_VALID		HW_BIT(31)
#define   WFSYS_ON_LPCR_EN_LP0_OWN		HW_BIT(30)
#define   WFSYS_ON_LPCR_EN_SX_ZPS_MODE		HW_BIT(21)
#define   WFSYS_ON_LPCR_EN_DIGCK		HW_BIT(1)
#define   WFSYS_ON_LPCR_EN_PLL_CKEN		HW_BIT(4)
#define   WFSYS_ON_LPCR_EN_DSLEEP_REQ_B	HW_BIT(0)
#define MT7996_WFSYS_ON_LPCR_DIS(n)		(0x81031008u + (n) * 0x1000u)
#define   WFSYS_ON_LPCR_DIS_LP0_OWN		HW_BIT(30)
#define   WFSYS_ON_LPCR_DIS_DIGCK		HW_BIT(1)
#define   WFSYS_ON_LPCR_DIS_PLL_CKEN		HW_BIT(4)
/* BN0 / BN1 TICKCR0 (ROM CR-map names WFSYS_ON BN0_TICKCR0 / BN1_TICKCR0):
 * two fields, LMAC_1X_MODE [4:2] and WLAN_CLK_MODE [1:0] (CR map
 * ENUM_WFSYS_ON_CONFIG_BN0_TICKCR0 0x2600e0 / e1, BN1 0x260420 / 421). Every
 * channel set's MAC-word routine (V:e00e5c9a -> V:e00e549a, mac.h "Channel
 * set") writes BN0 then BN1, WLAN_CLK_MODE then LMAC_1X_MODE, one
 * read-modify-write each (the ROM's non-transaction setter FIT +0x20, ROM
 * V:0081c682): both 2 for MediaTek's 5 MHz width, 1 for 10 MHz, else 0
 * (phy_chan.h MT7996_CHSET_TICKCR0_*). Stock 0, unchanged. [static][measured] */
#define MT7996_WFSYS_ON_TICKCR0(n)		(0x81031024u + (n) * 0x1000u)	/* n 0..1 */
#define   WFSYS_ON_TICKCR0_LMAC_1X_MODE		HW_GENMASK(4, 2)
#define   WFSYS_ON_TICKCR0_WLAN_CLK_MODE	HW_GENMASK(1, 0)
/*
 * Leaving low power (ROM halPmLeaveLowPower(band); MediaTek's WM runs it for
 * bands 0..2 at the start of every main-task message, V:e00ecd64): if LPCR
 * STS_LP0_VALID, LPCR_DIS = LP0_OWN (0x40000000), then wait for
 * LP_OWN_STAT == 0 and LPCR [30] == 0 (timeout (t + 5000) * 32768 / 1e6
 * ticks); then MT7996_MLO_SCR0 LINK_ACTIVE(band) = 1 (mac.h), and, with WM
 * flag 0xe01ec258 set, a read of SMCTL0 LP0_PWR_OFF_EVENT (0 here). WM
 * writes; the host-write HAZARD above stands. Needed only by a firmware
 * that sleeps.                                              [static, measured]
 */
#define MT7996_WFSYS_ON_LPCR2(n)		(0x81031010u + (n) * 0x1000u)	/* A-die clock/wake status */
#define MT7996_WFSYS_ON_014(n)			(0x81031014u + (n) * 0x1000u)	/* ROM |= 8 (BN0) */
#define MT7996_WFSYS_ON_LPCMD1(n)		(0x81031060u + (n) * 0x1000u)
/* SMCTL0 (GCI BN0_SMCTL0): [0] LP0_WIFI_PWR_OFF_EVENT (RO, 0), [1] its
 * clear, [31:16] LP0_DUMMY.                               [static, measured] */
#define MT7996_WFSYS_ON_SMCTL0(n)		(0x81031090u + (n) * 0x1000u)
#define   WFSYS_ON_SMCTL0_LP0_PWR_OFF_EVENT	HW_BIT(0)	/* RO */
#define   WFSYS_ON_SMCTL0_LP0_PWR_OFF_EVENT_CLR HW_BIT(1)
#define   WFSYS_ON_SMCTL0_LP0_DUMMY		HW_GENMASK(31, 16)
#define MT7996_WFSYS_ON_SMCTL6(n)		(0x810310a8u + (n) * 0x1000u)	/* A-die LDO */

/*
 * ROM WiFi power-on sequence (MCU-side; listed for reference, the host must
 * NOT replay it -- see WFSYS_ON HAZARD). Entry skipped if CONN_PWR_GATE b4.
 *  1. 0x8002e108 |= 0x2000; |= 0x200
 *  2. (flag) 0x81026004 |= 0x1000; 0x81026108 |= 0x00100000, 0x00020000,
 *     0x00018000
 *  3. 0x81026108 &= ~0xfc, |= 1, |= 0x100; 0x8102610c &= ~0xfc, |= 1,
 *     |= 0x708100; 0x81026800 |= 0x2a; 0x81026110 same as 0x10c;
 *     0x81026800 |= 0xa80
 *  4. (flag) 0x81020004 |= 0xc00; 0x81020008 |= 0x100, |= 0x2000
 *  5. 0x81020050 |= 0x10, |= 0x1, |= 0x40; delay 100 us
 *  6. UMAC (PLE/PSE/PP) enable pulses, polls up to 2001 tries
 *  7. per band X in {0x81031000, 0x81032000}: X+4 |= 0x80000000; 100 us;
 *     X+4 |= 0x00400000; 80 us; X+8 |= 0x10; X+4 |= 1, |= 2; poll X b26
 *     (2001 tries)
 *  8. 0x81026104 |= 0x00c00000; 0x8002e108 |= 0x04000000;
 *     0x81030048 |= 0xf000; X+4 |= 0x10
 *  9. 0x7c091004 &= 0x0002ffff; 0x7c092004 &= 0x0002ffff;
 *     0x81031014 |= 0x08
 * Firmware's own download/start performs the needed power-on: BN0/BN1 grants
 * appear in OSC_MASK after FW_START, with no host power-on writes.  [measured]
 */

/* ======================================================================== */
/* GCI: the ROM's register-field layer                                      */
/* ======================================================================== */
/*
 * MediaTek's "HAL generic control interface": the ROM and WM reach register
 * fields through selectors, module << 16 | group << 5 | field. The ROM's
 * FIT_TABLES (mcu_map.h MT7996_WM_ROM_FIT_TABLES) hold the interface: +0x00
 * GetConfigParamByte, +0x08 GetDWord, +0x0c SetConfigParam (one
 * read-modify-write), +0x28 TxnSetConfigParamVaArgs (n, sel, v, ...: one
 * read, all fields merged, one write to the FIRST selector's address, so a
 * transaction-path write stores a stale whole-word snapshot: no
 * address-equality check), +0x84 GetChipModuleIfc(module). Each module's
 * pfGetCRControlParam indexes a ROM rodata table: group entry {u32 field
 * table, u16 register offset, u8 field count}, field {u8 lsb, u8 msb};
 * address = the module's base + offset (the PHY modules: phy_map.h
 * MT7996_PHY_ROMSEL). Modules of the WM's host / idle path: 0x00..0x02 ARB
 * (0x820e3000 / 0x820f3000 / 0x830e3000, table 0x0083c18c), 0x1a / 0x1b
 * UMAC PLE / PSE (0x820c0000 / 0x820c8000, table 0x0083cc6c), 0x26 WFSYS_ON
 * (0x81031000, table 0x0083c6d4, +0x340 per band block), 0x3d MLO
 * (0x820d1000, table 0x0083f52c). The ROM power manager addresses the
 * WFSYS_ON band words above, e.g. BN0 WPLL ready 0x260004 -> 0x81031000
 * b26, enable 0x260024 -> 0x81031004 b1, disable 0x260044 -> 0x81031008 b1;
 * +0x340 per band.                                                  [static]
 */
#define MT7996_GCI_SEL(module, group, field)	(((module) << 16) | ((group) << 5) | (field))
#define MT7996_GCI_MODULE_ARB(b)		(b)	/* b = 0..2 */
#define MT7996_GCI_MODULE_PLE			0x1au
#define MT7996_GCI_MODULE_PSE			0x1bu
#define MT7996_GCI_MODULE_WFSYS_ON_BAND		0x26u
#define MT7996_GCI_MODULE_MLO			0x3du
#define MT7996_GCI_BAND_STRIDE			0x340u

#endif /* HW_MT7996_CONN_H */
