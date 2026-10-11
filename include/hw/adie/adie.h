/* SPDX-License-Identifier: MIT */
/*
 * RF A-dies of the W1700K: access path, die/bank map, chip identity, TOP
 * registers and the per-chain RX analog registers common to both die types.
 * Synthesizer registers are in sx.h, chain states and test modes in
 * rfmodes.h, die specifics in mt7976.h and mt7977.h.
 *
 * DIES. Three A-dies hang off the MT7996, one per band; band b is die b.
 *   die 0  MT7976 (MT7976GN)  band 0, 2.4 GHz, receive chains k 0..3, plus the
 *                             DFS/SM monitor receiver at k 4
 *   die 1  MT7977             band 1, 5 GHz,  chains k 0..3, synthesizer at k 4
 *   die 2  MT7977             band 2, 6 GHz,  chains k 0..4, synthesizer at k 4
 * [measured: chip IDs; static: band -> die records in the WM]
 * The A-dies are not reset by the MT7996's MCU reset (RESET_MT): their
 * registers keep whatever was written until the firmware rewrites them or the
 * board is power cycled. [measured]
 * Board: antenna connector 1 is shared by band 0 chain 0 and band 1 chain 0.
 * [measured]
 *
 * ADDRESS SPACE. An A-die register is a 32-bit word named by a 12-bit
 * register INDEX (0x000..0xfff) inside a bank k. Indices are not byte
 * addresses: 0xa4a and 0xa4b are neighbouring 32-bit registers, and a dump
 * with a stride of 4 misses most of the synthesizer. Register macros in the
 * adie/ headers are such indices, except names ending in _MCU (MT7996 MCU bus
 * addresses) and the ADIE_SPI_* registers (offsets from ADIE_SPI_MCU_BASE).
 * Banks:
 *   k 0..4  per-chain banks WF0..WF4 (on the MT7977, k 4 also holds the
 *           synthesizer; on the MT7976, k 0 holds the 2.4 GHz synthesizer)
 *   k 15    the die's TOP bank (chip ID, crystal trim, FEM word, cal setup)
 * The same index means the same register in every chain bank of a die.
 *
 * ACCESS. Each access is one SPI transaction issued by an SPI master inside
 * the MT7996 connectivity block (one master per die, chip-select = bank),
 * serialised by CONN hardware semaphore 12. Addresses of the masters and the
 * semaphore are MCU (WM) bus addresses (0x7c0xxxxx, CONN_INFRA). The
 * firmware's own path is the ROM's halPhyRfRegReadAnt/WriteAnt, reached from
 * the host with UNI REG_ACCESS (RF tag 1, ADIE_ANT below). [static][measured]
 *
 * Confidence tags: [measured] verified on hardware, [static] read from
 * firmware/ROM code, [inferred] reasoned. "HAZARD:" marks accesses that wedge
 * a bus, need a reload or power cycle, or transmit.
 */
#ifndef HW_ADIE_ADIE_H
#define HW_ADIE_ADIE_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------
 * Dies, banks, firmware "ant" numbering
 * ------------------------------------------------------------------------ */
#define ADIE_NUM_DIES                   3u
#define ADIE_DIE_OF_BAND(b)             (b)		/* [measured] */
#define ADIE_DIE_MT7976                 0u
#define ADIE_DIE_MT7977_5G              1u
#define ADIE_DIE_MT7977_6G              2u

#define ADIE_INDEX_MASK                 HW_GENMASK(11, 0)
#define ADIE_K_WF(n)                    (n)		/* chain bank WFn, n 0..4 */
#define ADIE_K_TOP                      15u
#define ADIE_NUM_K_WF                   5u

/* Firmware bank number ("ant", u1AntIdx): REG_ACCESS RF tag 1 carries it in
 * its idx field and the register index in ofs[11:0]. [static][measured]
 * An ant with k 5..14, or ant >= 0x30, silently addresses die 0 k 0. [static] */
#define ADIE_ANT(die, k)                (((die) << 4) | (k))
#define ADIE_ANT_DIE                    HW_GENMASK(7, 4)
#define ADIE_ANT_K                      HW_GENMASK(3, 0)

/* UNI REG_ACCESS (cid 0x0d) RF TLV, tag 1: {le16 tag, le16 len = 0x10,
 * le16 idx = ant, u16 rsv, le32 ofs = index, le32 data}. Query reply slot
 * {0x00100001, le16 idx, u16, le32 index, le32 value}; up to 93 RF queries
 * per command; a failed SPI read returns 0, indistinguishable from a register
 * that reads 0. [static][measured] */
#define ADIE_FW_REGACC_TAG_RF           1u
#define ADIE_FW_REGACC_RF_LEN           0x10u
#define ADIE_FW_REGACC_RF_MAX           93u

/* ------------------------------------------------------------------------
 * SPI chip-selects. Names are the firmware's (sys_spi_subsystem). [static]
 * CS 2 ("BT") answers 0xff at index 1 and 0 elsewhere on every die; CS 4
 * ("GPS") returns the garbage pattern (and see HAZARDS). Both are vestigial
 * on these dies. [measured]
 * ------------------------------------------------------------------------ */
#define ADIE_CS_WF1                     0u
#define ADIE_CS_WF0                     1u
#define ADIE_CS_BT                      2u
#define ADIE_CS_WF4                     3u
#define ADIE_CS_GPS                     4u
#define ADIE_CS_TOP                     5u
#define ADIE_CS_WF2                     6u
#define ADIE_CS_WF3                     7u
/* Bank k -> chip-select (the ROM's 47-byte ant table). [static][measured] */
#define ADIE_K_TO_CS(k)                 ((k) == ADIE_K_TOP ? ADIE_CS_TOP : \
					 (k) == 0u ? ADIE_CS_WF0 :          \
					 (k) == 1u ? ADIE_CS_WF1 :          \
					 (k) == 2u ? ADIE_CS_WF2 :          \
					 (k) == 3u ? ADIE_CS_WF3 : ADIE_CS_WF4)

/* ------------------------------------------------------------------------
 * A-die SPI master, one per die, in the MT7996 (MCU bus address).
 * Three channels with identical layout: channel 0 serves every CS except 2
 * and 5, channel 1 serves CS 2, the TOP channel serves CS 5. 32-bit data,
 * no address translation. [static][measured]
 * ------------------------------------------------------------------------ */
#define ADIE_SPI_MCU_BASE(die)          (0x7c098000u + 0x1000u * (die))

/* +0x000 STATUS, RO. Busy flag per channel; set while a transfer runs. */
#define ADIE_SPI_STATUS                 0x000u
#define ADIE_SPI_STATUS_BUSY_CH0        HW_BIT(1)
#define ADIE_SPI_STATUS_BUSY_CH1        HW_BIT(2)
#define ADIE_SPI_STATUS_BUSY_TOP        HW_BIT(5)

/* +0x004 bit 15: set by the ROM's SPI init on master 0 only; meaning
 * unknown. Masters 1 and 2 work without it. [static][measured] */
#define ADIE_SPI_CFG                    0x004u
#define ADIE_SPI_CFG_B15                HW_BIT(15)

/* Per-channel command / write-data / read-data registers. */
#define ADIE_SPI_CH0_CMD                0x010u
#define ADIE_SPI_CH0_WDATA              0x014u
#define ADIE_SPI_CH0_RDATA              0x018u
#define ADIE_SPI_CH1_CMD                0x020u
#define ADIE_SPI_CH1_WDATA              0x024u
#define ADIE_SPI_CH1_RDATA              0x028u
#define ADIE_SPI_TOP_CMD                0x050u
#define ADIE_SPI_TOP_WDATA              0x054u
#define ADIE_SPI_TOP_RDATA              0x058u
/* Channel of a chip-select: CMD offset; WDATA = CMD + 4, RDATA = CMD + 8. */
#define ADIE_SPI_CMD_OF_CS(cs)          ((cs) == ADIE_CS_TOP ? ADIE_SPI_TOP_CMD : \
					 (cs) == ADIE_CS_BT ? ADIE_SPI_CH1_CMD : \
					 ADIE_SPI_CH0_CMD)
#define ADIE_SPI_BUSY_OF_CS(cs)         ((cs) == ADIE_CS_TOP ? ADIE_SPI_STATUS_BUSY_TOP : \
					 (cs) == ADIE_CS_BT ? ADIE_SPI_STATUS_BUSY_CH1 : \
					 ADIE_SPI_STATUS_BUSY_CH0)

/* CMD word. [17:16] are preserved by the ROM (read-modify-write); meaning
 * unknown. [static] */
#define ADIE_SPI_CMD_KEEP               HW_GENMASK(17, 16)
#define ADIE_SPI_CMD_CS                 HW_GENMASK(15, 13)
#define ADIE_SPI_CMD_READ               HW_BIT(12)
#define ADIE_SPI_CMD_INDEX              HW_GENMASK(11, 0)

/* +0x10c, +0x110 bit 0: set on all three masters by the ROM patch's power
 * routine; meaning unknown. [static] */
#define ADIE_SPI_PWR0                   0x10cu
#define ADIE_SPI_PWR1                   0x110u
#define ADIE_SPI_PWR_B0                 HW_BIT(0)

/* CONN hardware semaphore 12 guards the A-die SPI. [static][measured]
 * Acquire = a READ of ACQ; granted iff (value & 0xfd) == 1. The ROM tries up
 * to 1000 times, 1 us apart. Release = write 1 to REL. Only release a
 * semaphore that was granted.
 * HAZARD: a read of ADIE_SEMA_ACQ is an acquire. A stray read (host dump,
 * REG_ACCESS probe, sweep) leaves the semaphore owned and every later
 * firmware A-die access then fails after 1 ms. */
#define ADIE_SEMA_ID                    12u
#define ADIE_SEMA_ACQ_MCU               0x7c070030u	/* 0x7c070000 + 4 * id */
#define ADIE_SEMA_REL_MCU               0x7c070230u	/* 0x7c070200 + 4 * id */
#define ADIE_SEMA_REL_VAL               1u
#define ADIE_SEMA_GRANTED(v)            (((v) & 0xfdu) == 1u)
#define ADIE_SEMA_TRIES                 1000u

/*
 * One transaction, as the ROM does it; works from any WM context. [static][measured]
 *   1. acquire ADIE_SEMA (interrupts off for the whole transaction);
 *   2. if STATUS & busy(cs): release, fail (the ROM does not wait);
 *   3. CMD = (CMD & KEEP) | cs << 13 | read << 12 | index;
 *   4. dummy read of WDATA (the ROM always does it);
 *   5. WDATA = value, or 0 for a read: this starts the transfer;
 *   6. poll STATUS & busy(cs) == 0 (the ROM allows 100000 reads);
 *   7. read: value = RDATA;
 *   8. write 1 to ADIE_SEMA_REL.
 * Cost [measured]: 9 register reads in 46 us of WM time, semaphore included.
 */
#define ADIE_SPI_BUSY_POLLS             100000u

/* A hardware SPI command injector in the MT7996 (MCU bus address): when the
 * selected trigger sources fire, it writes VAL_ON / VAL_OFF to the register
 * named by CMD. The DFS monitor start uses it to toggle die-0 WF4 0x001[9:8]
 * (CMD 0x6001 = CS 3, index 0x001) while the AP's own bands transmit.
 * [static][inferred] */
#define ADIE_SPI_INJ_CTL_MCU            0x7c091214u
#define ADIE_SPI_INJ_CTL_EN             HW_BIT(0)
#define ADIE_SPI_INJ_CTL_SRC            HW_GENMASK(3, 1)
#define ADIE_SPI_INJ_CMD_MCU            0x7c091218u	/* [15:0] SPI CMD word */
#define ADIE_SPI_INJ_VAL_ON_MCU         0x7c09121cu
#define ADIE_SPI_INJ_VAL_OFF_MCU        0x7c091220u

/* ------------------------------------------------------------------------
 * HAZARDS of the A-die read path.
 * ------------------------------------------------------------------------
 * Garbage pattern: a broken read path returns 0 at even indices and
 * 0xfffff800 / 0xff800000 at odd ones. [measured]
 *
 * HAZARD: reading TOP index 0x222 can wedge that die's SPI reads into the
 * garbage pattern for good: it survived RESET_MT reloads (the firmware patch
 * download then timed out) and only a power cycle cleared it. Not
 * deterministic: other reads of it recovered by themselves. Never read TOP
 * 0x200..0x2ff. The receive path kept working while reads were wedged.
 * [measured]
 *
 * HAZARD: zeroing MT7977 TOP 0x060 or TOP 0x664 (one of the two) makes that
 * die keep no later SPI write for the rest of the run, until a power cycle;
 * reads still work. MediaTek's calibration end restores ~430 A-die registers
 * from a backup that only its backup item (phy_cal.h calibration mask bit 3)
 * fills: run without it, the restore writes zeros and triggers this. In the
 * TOP list that zero restore differed from the stock one in 0x060
 * (0x06002010 -> 0), 0x664 (0xffa428af -> 0), then 0x668, 0x000, 0x04c,
 * 0x7e9, 0x4a4; 0x668 is the first write shown not retained, so the trigger
 * is one of the two before it. Not TOP 0x600: the stock restore writes TOP
 * 0x600 = 0 and 0x070 = 0 (their backed-up values) on every calibration and
 * the die keeps every later write (11,260 read-after-write checks in the
 * stock calls, none differing). The MT7976 survives TOP 0x060 = 0 and 0x070
 * = 0 in the same restore. [measured effect, inferred trigger]
 * HAZARD: reading chain k 1 index 0x5aa (all three dies), or any CS 4
 * ("GPS") index, blanks that die's reads for about 0.3 s on a die MediaTek's
 * firmware initialised; it recovers by itself. On a cold die (power cycle,
 * only our firmware) the same read of die 0 left every later read of that
 * die, TOP included, returning 0 until the next power cycle. Never read it.
 * [measured]
 *
 * MT7977 (dies 1, 2) banks k 2 and k 3 (CS 6, CS 7): the 2026-10-09 census
 * (the stream producer's raw chip-select reads) saw only the garbage pattern
 * there; our WM's A-die path (the transaction below, ADIE_K_TO_CS) reads them
 * correctly on this unit: the RC calibration reads bank 2 0xece and lands
 * the codes MediaTek's way (2026-10-11). What made the census reads fail is
 * not established. The MT7976 answers on k 0..4. [measured]
 *
 * Verify a new region with a sentinel read after each block:
 * ADIE_TOP_SENTINEL reads ADIE_TOP_SENTINEL_VAL on every die. [measured]
 */
#define ADIE_TOP_WEDGE_INDEX            0x222u
#define ADIE_TOP_NOREAD_FIRST           0x200u
#define ADIE_TOP_NOREAD_LAST            0x2ffu
#define ADIE_K1_BLANK_INDEX             0x5aau
#define ADIE_GARBAGE_EVEN               0x00000000u
#define ADIE_GARBAGE_ODD_A              0xfffff800u
#define ADIE_GARBAGE_ODD_B              0xff800000u

/* ------------------------------------------------------------------------
 * TOP bank (k 15)
 * ------------------------------------------------------------------------ */

/* 0x000  TOP control. RW. Observed 0x0000ffef (die 0, monitor receiver
 * started) and 0x0000ffff (die 2). The monitor start writes 0x0000ffef.
 * [measured][static] Bit 4 was read as "cleared while the TTG PLL runs"
 * [inferred] from an MT7977 TTG write; the TTG RX calibrations run the TTG
 * with it set: the MT7976 path sets it after every TTG programming, the
 * MT7977 path in TTG mode 4 (the DC phase of every step, both phases of
 * T1[23]) clears it and sets it again at once (mt7977.h). Its role is not
 * established. Other bits unknown. [static][measured] */
#define ADIE_TOP_CTRL0                  0x000u
#define ADIE_TOP_CTRL0_TTG_N            HW_BIT(4)	/* [inferred] */
#define ADIE_TOP_CTRL0_MON              0x0000ffefu

/* 0x00c  Constant. RO. 0xa6aaaa55 on every die: a read-path sentinel. [measured] */
#define ADIE_TOP_SENTINEL               0x00cu
#define ADIE_TOP_SENTINEL_VAL           0xa6aaaa55u

/* 0x02c  Chip ID. RO. [measured] The firmware reads die 2's [7:0] (the ECO
 * byte, WM 0xe01ec180, at calibration init, phy_cal.h) to pick its MT7977
 * synthesizer preset and RF_INIT variant for both MT7977 dies (> 9: SX80_E3,
 * see mt7977.h and "RF_INIT" below); item 0 adds MediaTek's "dap" tables
 * when die 0's [11:8] == 0xc (MT7976C; 0xa here). [static][measured] */
#define ADIE_TOP_CHIP_ID                0x02cu
#define ADIE_TOP_CHIP_ID_PART           HW_GENMASK(31, 16)
#define ADIE_TOP_CHIP_ID_PART_MT7976    0x7976u
#define ADIE_TOP_CHIP_ID_PART_MT7977    0x7977u
#define ADIE_TOP_CHIP_ID_REV            HW_GENMASK(15, 8)	/* 0x8a on both */
#define ADIE_TOP_CHIP_ID_VER            HW_GENMASK(7, 0)
#define ADIE_TOP_CHIP_ID_MT7976         0x79768a20u	/* die 0 */
#define ADIE_TOP_CHIP_ID_MT7977         0x79778a10u	/* dies 1 and 2 */
/* Item 0's 7976C test (V:e0040edc..V:e0040ef0): die 0's [11:8] == 0xc adds MediaTek's
 * "dap" tables (V:e0050cbe; rfinit.h MT7976C_*) after the MT7976 tables. 0x79768a20 here:
 * [11:8] = 0xa, not taken. The test is item 0's own: the ROM patch's chip-ID lists
 * (ADIE_CHIP_ID_MT7976C_*, below) call 0x79768b00 and 0x79768a10 "7976C" too, which this test
 * does not select, and 0x79768c00 an MT7976, which it does. [static][measured] */
#define ADIE_TOP_CHIP_ID_11_8            HW_GENMASK(11, 8)
#define   ADIE_TOP_CHIP_ID_11_8_MT7976C  0xcu

/* 0x060  Analog interface mode. RW. Observed 0x06002010 (dies 0, 2; all
 * three after the TOP init).
 * [26:24] = 6 normally; calibrations write 0, 1, 3 (the TTG RX
 * calibrations: 3 for the DC phase, 1 for the IQ phase of every gain step;
 * the TX IQ calibration T1{8,12,16}: 0).
 * Writing 0/1/3 while streaming changed nothing observable.
 * [15:0] = 0x2010 from the TOP init (below), a read-modify-write that keeps
 * [26:24]. [static][measured]
 * HAZARD: the whole word = 0 on an MT7977 is one of the two candidate
 * write-lock triggers (HAZARDS above): write it only by read-modify-write,
 * keeping [26:24] = 6. */
#define ADIE_TOP_IFMODE                 0x060u
#define ADIE_TOP_IFMODE_SEL             HW_GENMASK(26, 24)
#define ADIE_TOP_IFMODE_SEL_NORMAL      6u
#define ADIE_TOP_IFMODE_LO              HW_GENMASK(15, 0)
#define ADIE_TOP_IFMODE_LO_INIT         0x2010u

/* 0x068  TOP init word (below): [23:20] = 3, [30] = 1 (both die types),
 * [3] = 1 (MT7977, after a clear). Observed 0x00300000 -> 0x40300000 (die
 * 0) / 0x40300008 (dies 1, 2). Not decoded. [static][measured] */
#define ADIE_TOP_068                    0x068u
#define ADIE_TOP_068_B30                HW_BIT(30)
#define ADIE_TOP_068_F23_20             HW_GENMASK(23, 20)
#define   ADIE_TOP_068_F23_20_INIT      3u	/* the TOP init (e00dc9a4 / e00dcc1e) */
#define ADIE_TOP_068_B3                 HW_BIT(3)	/* MT7977 */

/* 0x070  Per-chain analog-interface nibbles. RW. Observed 0x00000005 (die 0),
 * 0x00000000 (die 2). Calibrations write 0x88888005 ([31:12] five nibbles,
 * 8 = cal interface on; 9, a, d = other modes; the MT7976 TX test mode of
 * T2{4} sets the transmitting WF's nibble [15 + 4 WF : 12 + 4 WF] to d,
 * rfmodes.h; the MT7976 TTG RX path, V:e006366a, writes 0xaaaaa005 in
 * A-die modes 4 (T1[23]) and 5 alike; the MT7977 path does not write it).
 * The stock ROM patch writes 0x88880005 or 0x5 at bring-up. No effect seen
 * when written mid-stream. [static][measured] */
#define ADIE_TOP_CHAIN_IF               0x070u
#define ADIE_TOP_CHAIN_IF_NIBBLES       HW_GENMASK(31, 12)
#define ADIE_TOP_CHAIN_IF_CAL           0x88888005u
#define ADIE_TOP_CHAIN_IF_TTG_RX        0xaaaaa005u

/* 0x3a4  RC-calibration setup (MT7977). RW. Power-on 0x011fe01a (dies 1 and
 * 2, [14] already set); after the RC calibration 0x011fa01a. The calibration
 * sets [14] and [22:21] = 1 (measurement 1) / 2 (measurement 2), and ends
 * with [22:21] = 0, then [14] = 0. [static][measured] */
#define ADIE_TOP_RCCAL                  0x3a4u
#define ADIE_TOP_RCCAL_EN               HW_BIT(14)
#define ADIE_TOP_RCCAL_PHASE            HW_GENMASK(22, 21)
/* Phase values of the MT7977 RC calibration (V:e004d3d8..V:e004d3ee,
 * V:e004d48c..V:e004d4a4, V:e004d84e..V:e004d862): 1 for measurement 1, 2 for measurement 2, 0
 * at the end (then ADIE_TOP_RCCAL_EN = 0). [static][measured] */
#define   ADIE_TOP_RCCAL_PHASE_MEAS1     1u
#define   ADIE_TOP_RCCAL_PHASE_MEAS2     2u
#define   ADIE_TOP_RCCAL_PHASE_OFF       0u

/* 0x490..0x4a6  FEM / antenna-switch pins (adie_init.h: MediaTek's pin mux
 * V:e00cea48 of the common PHY init writes them on every die; names ours
 * [guess], roles: the pins drive the external FEM / antenna switches
 * [inferred]). TOP 0x4a4 [31:16] holds pin j's mode bit at [16 + j]; the pin
 * fields (ADIE_FEM_PIN_FIELDS) and the per-set read-modify-writes
 * (ADIE_FEM_TOP_TBL*) are MediaTek's tables. TOP 0x744 is MT7976_TOP_FEM.
 * The channel set's gain-step programmer (phy_cal.h "Group loop") also writes
 * TOP 0x4a4..0x4a6: group 0 the five flags ADIE_TOP_FEM_4A4_GSTEP, else six
 * flags of the WF from TOP 0x4a4 bit 0 (WF 0), 0x4a5 bit 0 / 16 (WF 1 / 2),
 * 0x4a6 bit 0 / 16 (WF 3 / 4) (phy_chan.h MT7996_CHSET_GSTEP_TOP / _BIT).
 * No other writer in the stock load. [static][measured] */
#define ADIE_TOP_FEM_490                  0x490u
#define ADIE_TOP_FEM_PINSEL(n)            (0x49cu - 4u * (n))	/* n 0..2: 0x49c, 0x498, 0x494 */
#define ADIE_TOP_FEM_49D                  0x49du
#define   ADIE_TOP_FEM_49D_SET            0x00000005u
#define   ADIE_TOP_FEM_49D_CLR            0x00000006u
#define ADIE_TOP_FEM_49E                  0x49eu
#define ADIE_TOP_FEM_4A0                  0x4a0u
#define ADIE_TOP_FEM_4A4                  0x4a4u
#define   ADIE_TOP_FEM_4A4_PINMODE(j)     HW_BIT(16u + (j))	/* j 0..15 */
#define   ADIE_TOP_FEM_4A4_GSTEP        HW_GENMASK(4, 0)	/* group 0: the five step flags */
#define ADIE_TOP_FEM_4A5                  0x4a5u
#define ADIE_TOP_FEM_4A6                  0x4a6u

/* 0x4ac  TOP init word (below): [17] = 1, [16] = 1 (both die types; the
 * MT7977 also clears [18]); observed 0 -> 0x30000. T2{6} sets [17] for its
 * call (phy_cal.h). Not decoded. [static][measured] */
#define ADIE_TOP_4AC                    0x4acu
#define ADIE_TOP_4AC_B18                HW_BIT(18)	/* MT7977: cleared */
#define ADIE_TOP_4AC_B17                HW_BIT(17)
#define ADIE_TOP_4AC_B16                HW_BIT(16)

/* 0x4b0 / 0x4b4  Written at every channel set (3 / 0x1c, or 0x1f / 0x1f).
 * Observed die 0: 0x1f / 0x1f; die 2: 0x07070707 / 0x7. In MediaTek's
 * stock channel sets only the MT7976 path of the channel-set D-die RX
 * calibration writes them (0x1f / 0x1f, V:e00dd2f6 / V:e00dd312; phy_cal.h);
 * the MT7977 dies are not written there. Meaning unknown.
 * [static][measured] */
#define ADIE_TOP_CHSET0                 0x4b0u
#define ADIE_TOP_CHSET1                 0x4b4u

/* 0x57c  MT7977 only: the TOP init writes 0x11111, 0, then 0x11111 (writes
 * only). Not decoded. [static] */
#define ADIE_TOP_57C                    0x57cu
#define ADIE_TOP_57C_INIT               0x00011111u

/* 0x654 / 0x658  Crystal trim. RW. [30:24] = 7-bit trim, written identically
 * to both words; it moves every LO and the sample clock in proportion. The
 * firmware's frequency-offset command writes it on die 2's TOP; with strap
 * 0x700056f0[18] set it uses die 0's TOP and alternative fields (0x654/0x658
 * [22:16], 0xa94 [26:20]/[18:12], 0xaa4 [17:16]). Observed 0x40401010 (die 0)
 * and 0x68402a2a (die 2). [static][measured]
 * Source: the PHY-init routine's tail (V:e00b096e -> V:e012f064, on every
 * band's pass; phy_cal.h "Calibration init") reads 0x654, inserts trim [6:0]
 * into [30:24] and stores the word to 0x654 and 0x658 of die 2, with trim =
 * EEPROM 0x1269 + the signed offset EEPROM 0x126f (sign-magnitude: [6] sign,
 * 1 = +, [5:0] magnitude; V:e00e1228 with sel 1). Stock 0xd4 + 0x14 = 0xe8
 * -> 0x68, the observed die-2 value. [static][measured] */
#define ADIE_TOP_XTAL_TRIM0             0x654u
#define ADIE_TOP_XTAL_TRIM1             0x658u
#define ADIE_TOP_XTAL_TRIM_VAL          HW_GENMASK(30, 24)
#define ADIE_TOP_XTAL_TRIM_DIE          2u
#define ADIE_EE_XTAL_TRIM               0x1269u	/* host EEPROM image */
#define ADIE_EE_XTAL_TRIM_OFS           0x126fu
#define   ADIE_EE_XTAL_TRIM_OFS_MAG     HW_GENMASK(5, 0)
#define   ADIE_EE_XTAL_TRIM_OFS_POS     HW_BIT(6)	/* 1 = + */

/* 0x71c  Written 0 by MediaTek's XPA helpers after their pre-rows (MT7976
 * V:e0050dc0..V:e0050dce, 7976C V:e0050aaa..V:e0050ac0; ant = die << 4 | 0xf), once per WF. Not
 * the chain-bank ADIE_BWLUT_71C. Role unknown. [static][measured] */
#define ADIE_TOP_71C                    0x71cu
#define   ADIE_TOP_71C_XPA              0x0u

/* 0xdc4  [7:3] cleared, then set by the TOP init (AdieSel 6 and 7, both die
 * types here; observed 0xfa -> 0x02 -> 0xfa). Not decoded.
 * [static][measured] */
#define ADIE_TOP_DC4                    0xdc4u
#define ADIE_TOP_DC4_F7_3               HW_GENMASK(7, 3)

/*
 * TOP init V:e00dbfe6, per die, in the per-band PHY init before item 0
 * (phy.h "Per-band PHY init"; ant = die << 4 | 0xf), branched by the
 * AdieSel byte (phy_map.h MT7996_PHY_FCB_CFG_INIT). [static][measured]
 *   D-die first: WF_COMM +0xd480 [19] pulsed (phy.h MT7996_PHY_WFCOMM_D480);
 *   TOP 0xdc4 [7:3] cleared, then set;
 *   MT7976 (AdieSel 6): 0x4ac [17] = 1; WF 0..4 0x479 [29] = 0 (0x38000000
 *     -> 0x18000000, rfmodes.h ADIE_TSSI_479); IFMODE [15:0] = 0x2010; 0x068
 *     [23:20] = 3, then [30] = 1; 0x4ac [16] = 1; WF_COMM +0xd49c [30] = 1,
 *     [31] = 0 and +0xd4a0 [22] = 1 (phy.h);
 *   MT7977 (other): WF_COMM +0xd4a0 [20] = 1, [21] = 0, [22] = 1 and +0xd49c
 *     [30] = 1, [31] = 0; 0x4ac [17] = 1, [18] = 0, [16] = 1; 0x068 [30] =
 *     1, [3] = 0; IFMODE [15:0] = 0x2010; 0x068 [23:20] = 3, [30] = 1, [3] =
 *     1; 0x4ac [16] = 1, [17] = 1; +0xd49c [30] = 1, [31] = 1; +0xd4a0 [22],
 *     [20] = 1; 0x57c = 0x11111, 0, 0x11111.
 * Observed: 0x4ac 0 -> 0x30000, 0x068 0x00300000 -> 0x40300000 (die 0) /
 * 0x40300008 (dies 1, 2), IFMODE 0x06002010 unchanged. Every IFMODE access
 * is a read-modify-write that keeps [26:24] = 6 (the IFMODE write-lock
 * HAZARD above). Part of a receiver's cold init [inferred].
 */

/* ------------------------------------------------------------------------
 * Per-chain RX analog (k 0..4)
 * ------------------------------------------------------------------------
 * Gain: the A-die LNA and LPF stages are set by index from the MT7996 PHY
 * (per-chain AGC force fields, see ../mt7996/phy.h); no direct A-die gain
 * register is decoded. Effect of the forced index on the captured path
 * [measured]: LNA 4 -> 0 steps -5.6, -5.8, -6.1, -6.8 dB (about 24 dB); LPF
 * 9 -> 1 about 2 dB per step (about 16 dB); LPF index 0 behaves differently.
 * The A-die holds gain tables at 0xe70..0xe7f (variant 0xe80..0xe8f), loaded
 * by the firmware's RF tables per band. [static]
 */

/* 0x100  RX analog bandwidth class. RW, whole word = code. Observed 0 at
 * rest. Writing it takes effect at once, mid-stream, and the die then rewrites
 * its own filter registers (ADIE_BWLUT_* below). [measured]
 * Two-sided bandwidth by code: 20/40/80/160/320 MHz. Measured noise-floor
 * corners: about +-10, +-16, +-30, +-100 MHz; code 4 flat to +-158 MHz at
 * 320 Msps. [measured] Codes 5 and 6 produce self-oscillation-like lines at
 * +-115 MHz. [measured] The firmware maps code 7 to 4; on the MT7976 it only
 * writes codes 0..3. [static]
 * MT7976: only codes 0..3 select a filter. The word keeps [2:0] (a write of
 * 0x103 reads back 3), and code 4 reads back 4 but filters as code 0 (within
 * 1 dB from 5 to 150 MHz off the LO at 320 Msps): the die decodes [1:0]
 * (its LUT mirror ADIE_BWLUT_1E1 [17:16] = code & 3). There is no 320 MHz
 * filter on the MT7976 (mt7976.h "RX analog filter"). [measured] */
#define ADIE_RX_BW                      0x100u
#define ADIE_RX_BW_CODE_20MHZ           0u
#define ADIE_RX_BW_CODE_40MHZ           1u
#define ADIE_RX_BW_CODE_80MHZ           2u
#define ADIE_RX_BW_CODE_160MHZ          3u
#define ADIE_RX_BW_CODE_320MHZ          4u

/* 0x101  Written 1 with the BW class (MT7977, flag 1); in the DC calibrations
 * [0] = (calibration group != 0), with 0x103 (DCOC below); 1 at rest on dies
 * 1/2 and die-0 k 4, 0 on die-0 k 0..3. Bit 0 cleared on a receiving MT7977
 * chain kills reception. [static][measured] */
#define ADIE_BW_AUX0                    0x101u
#define ADIE_BW_AUX0_EN                 HW_BIT(0)
/* 0x102  Second BW-class word, written with the same code as 0x100 by the
 * firmware's BW setter (TX path, inferred). [static][inferred] */
#define ADIE_TX_BW                      0x102u
/* 0x103  Companion of 0x101 for 0x102. Observed 1. [static][measured] */
#define ADIE_BW_AUX1                    0x103u
/* [0]: with 0x101 [0] = (calibration group != 0)
 * in the MT7977 fine body (read-modify-write); the coarse body and both
 * MT7976 bodies write both words whole (1 or 0). [static][measured] */
#define ADIE_BW_AUX1_EN                 HW_BIT(0)

/* 0x1e8  Reads back the current BW class code. RO. [measured] */
#define ADIE_RX_BW_RB                   0x1e8u

/*
 * Filter registers the die reloads itself when 0x100 is written. No firmware
 * code writes them. Values by code 4/3/2/1/0 [measured]:
 *   0x1e1 [17:16]      0/3/2/1/0
 *   0x71b [15:12]      4/3/2/1/0
 *   0x71c [11:8]       7/3/2/1/0
 *   0x771 [7:0]        cc/ab/aa/ab/aa, [24] 0/1/1/0/0
 *   0x773 [7:0]        0c/18/18/31/31 (halves as the bandwidth doubles),
 *         [23:16]      c0/c0/30/30/30, [15:8] 22/22/32/22/22
 *   0x450 [16], [8]    0/0/1/1/1
 *   0x451 [29:28], [21:20]  3/3/0/0/0
 *   0x082, 0x084 [3:0] 2/2/8/8/8;  0x086 [26] 0/0/1/1/0;  0x085 [15] varies
 *   0x770, 0x772, 0x986, 0x987 also change.
 * Writing 0x770[6:0], 0x772[23:8], 0x71b[19:16], 0x71c[26:24], 0x71c[19:16]
 * (or the reloaded registers generally) does not move the corner. [measured]
 * MT7976 (die 0, chain bank k 0, every index read after each code): the die
 * changes only 0x1e1 [17:16] = code & 3, 0x982 = 0x001e9800 / 0x001aba00 /
 * 0x001a0600 / 0x001a0600 and 0x983 = 0xe0001111 / 0x80001111 (codes 0 / 1..3),
 * besides the status words 0x085 [15] (set at code 3, also moves by itself),
 * 0x986 and 0x987. 0x1e1, 0x982, 0x983 and 0x086 ignore writes (read-only
 * mirrors of the die's internal decode). 0x450, 0x451, 0x71b, 0x71c and
 * 0x770..0x773 read 0 on the MT7976. No MediaTek table or calibration writes
 * any of them (notes/20 section 5): the per-code settings are fixed logic in
 * the die. [measured][static]
 */
#define ADIE_BWLUT_1E1                  0x1e1u
#define ADIE_BWLUT_450                  0x450u
#define ADIE_BWLUT_451                  0x451u
#define ADIE_BWLUT_71B                  0x71bu
#define ADIE_BWLUT_71C                  0x71cu
#define ADIE_BWLUT_770                  0x770u
#define ADIE_BWLUT_771                  0x771u
#define ADIE_BWLUT_772                  0x772u
#define ADIE_BWLUT_773                  0x773u
#define ADIE_BWLUT_982                  0x982u	/* MT7976, RO */
#define ADIE_BWLUT_982_MT7976(code)     ((code) == 0u ? 0x001e9800u : (code) == 1u ? 0x001aba00u : 0x001a0600u)
#define ADIE_BWLUT_983                  0x983u	/* MT7976, RO */
#define ADIE_BWLUT_983_MT7976(code)     ((code) == 0u ? 0xe0001111u : 0x80001111u)
#define ADIE_BWLUT_1E1_CODE             HW_GENMASK(17, 16)	/* = ADIE_RX_BW code & 3 */
#define ADIE_BWLUT_986                  0x986u
#define ADIE_BWLUT_987                  0x987u

/* 0x171  Strobe word. RW. Observed 0xfcffffff. Bit 9 is pulsed low then high
 * before an MT7977 retune (synthesizer bank); bit 10 is pulsed before the RC
 * calibration (measurement chain); bit 11 is pulsed in calibration flows.
 * Bits 0 and 8 cleared on a receiving MT7977 chain kill reception.
 * [static][measured] */
#define ADIE_STROBE                     0x171u
#define ADIE_STROBE_SX_PREP             HW_BIT(9)
#define ADIE_STROBE_RCCAL_PREP          HW_BIT(10)
#define ADIE_STROBE_CAL                 HW_BIT(11)

/* 0x172  Channel-set word. RW. Observed 0xaa000003 (chains), 0x2a000003
 * (die-0 k 4). [31:28] = 0xa is written at every channel set; [26] is set for
 * the 5 GHz monitor receiver. In MediaTek's stock channel sets the MT7976
 * path writes it (die 0 k 0..3: 0x2a000003 -> 0xaa000003); the MT7977 path
 * does not. [static][measured] */
#define ADIE_CHSET                      0x172u
#define ADIE_CHSET_HI                   HW_GENMASK(31, 28)
#define ADIE_CHSET_HI_VAL               0xau
#define ADIE_CHSET_MON5G                HW_BIT(26)

/* 0x933 / 0x934  Written 1..3 at every channel set next to the BW class.
 * Observed 3 / 0. The channel-set D-die RX calibration (V:e00dcd88,
 * phy_cal.h) writes 0x933 after ADIE_RX_BW on every chain: MT7976 always
 * 2; MT7977 by the BW code: 1 for codes 0, 1, 2 for codes 2, 3, 3 for code
 * 4 and above (the code itself then written as 4). Stock: 2 on die 0
 * (BW20, k 0..3), 2 on die 1 (BW160) and 3 on die 2 (BW320), k 0..4 on
 * both MT7977 dies. 0x934 is the companion the same routine writes next to
 * ADIE_TX_BW (0x102) instead [inferred: not run in the stock channel sets].
 * Meaning unknown. [static][measured] */
#define ADIE_CHSET_933                  0x933u
#define ADIE_CHSET_933_MT7976           2u
#define ADIE_CHSET_933_MT7977(bw)       ((bw) <= 1u ? 1u : (bw) <= 3u ? 2u : 3u)	/* bw: ADIE_RX_BW code */
#define ADIE_CHSET_934                  0x934u

/* 0xe90  Chain word (k 0..3), both die types; on the MT7976 also the DCOC /
 * TX-detector mode word (mt7976.h MT7976_DCOC_E90). [30] = !(EEPROM 0x199 +
 * die [3]), set by the PHY-init routine's tail on all four chains of every
 * die (V:e00e2314 -> V:e0130752 mode 2; phy_cal.h "Calibration init"):
 * 0x01630210 -> 0x41630210 on every die. Name [guess]. [static][measured] */
#define ADIE_CHAIN_E90                  0xe90u
#define ADIE_CHAIN_E90_B30              HW_BIT(30)

/* 0x631 (group 0, rfmodes.h ADIE_LOOP_631) / 0x636 (other groups, mt7977.h
 * MT7977_TTG_636)  Gain-step words of the channel set's gain-step programmer
 * (V:e00d1dc4, phy_cal.h "Group loop"; one read-modify-write on k = WF):
 * [3:0], [7:4], [11:8], [15:12], [19:16] the step of slots 0..4, [23:20]
 * slot 5 (group != 0), [31:24] kept. [static][measured] */
#define ADIE_GSTEP_CHAIN_KEEP           HW_GENMASK(31, 24)
#define ADIE_GSTEP_CHAIN_FIELD(n)       HW_GENMASK(4u * (n) + 3u, 4u * (n))	/* n 0..5 */

/* --- The ROM patch's TOP init and trims (MediaTek MCU_Patch_init) ---------
 * MediaTek runs these from its ROM patch at patch download (mt7996/conn.h
 * "The ROM patch's radio init"); our WM runs them at bring-up
 * (firmware/radio/patch_init.c). Per die, selected by ADIE_TOP_CHIP_ID
 * (patch V:00901732); all values [static], immediates of the patch:
 *  MT7976 (0x79768a20, 0x79768c00; V:009014ec), after 0x4ac [4:0] = 0x1c
 *   iff eFuse 0x471 == 0xa (die 0's A-die eFuse, the ROM's eFuse section
 *   1; secure.h): 0xa00 = 0xffffffff, 100 us;
 *   0xa20 [7:6] = 1; 0x038 = 0x4a563b00; 0x65c = 0x1959f80f;
 *   0x664 = 0x34d00fe0.
 *  MT7976C (0x79768c10, 0x79768b00, 0x79768a10; V:0090146c): 0x4ac [4:0] =
 *   0x1c, then the same with 0x65c = 0x1d59080f, 0x664 = 0x34c00fe0.
 *  MT7977 (V:0090156c; chip ID 0x79778a00 / a01 variant 1 / 2, 0x79778a10
 *   / a11 variant 3): 0xa00 = 0xffffffff, 100 us; 0xa20 [7:6] = 1;
 *   0x66c = 0x5a00ab00; 0x65c = 0xfc103142; 0x660 = 0x4c460960;
 *   0x664 = 0xffa428af; 0x668 = 0xffd42d00 (variants 1, 2: 0xffd40000);
 *   0x7e2 = 0x155; 0x7e4 = 0; 0x7e5 = 0x01110111; 0x7e6..0x7e8 = 0;
 *   0x7e9 = 0xea000000; 0x220 = 0x08844080 (variants 1, 2: 0x08a4c080);
 *   0x034 = 0xc01011f0 (variants 1, 2: 0xc01091f0); 0x030 [18] = 0, then 1.
 * The AFE calibration pulse of each die's band (conn.h step 10) runs with
 * the die's analog-interface nibbles raised: MT7976 ADIE_TOP_CHAIN_IF =
 * 0x88880005 during, 0x00000005 after; MT7977 TOP 0x043 / 0x044 / 0x048 /
 * 0x049 / 0x04a (MT7977_TOP_DCOC_WF(0..4)) = 0x88880000 during, 0 after.
 * Trims (the patch's step 22): an eFuse byte whose [7] is set puts its
 * [6:3] into MT7976 TOP 0x034 [15:12] (eFuse 0xba6) or MT7977 TOP 0x220
 * [23:20] (eFuse 0x1e7a for die 1, 0x127a for die 2; secure.h); unit A
 * reads 0xc4 / 0xd4 / 0xcc, so 8 / 0xa / 9. Every value MediaTek's init
 * leaves equals the immediate here on a power-cycled unit (2026-10-09).
 * Names ours [guess], roles unknown except the trims (bias trim
 * [inferred]). [static][measured]
 */
#define ADIE_TOP_A00                    0xa00u
#define   ADIE_TOP_A00_INIT             0xffffffffu
#define   ADIE_TOP_A00_SETTLE_US        100u
#define ADIE_TOP_A20                    0xa20u
#define   ADIE_TOP_A20_B76              HW_GENMASK(7, 6)	/* = 1 */
#define ADIE_TOP_038                    0x038u
#define   ADIE_TOP_038_MT7976           0x4a563b00u
#define ADIE_TOP_65C                    0x65cu
#define   ADIE_TOP_65C_MT7976           0x1959f80fu
#define   ADIE_TOP_65C_MT7976C          0x1d59080fu
#define   ADIE_TOP_65C_MT7977           0xfc103142u
#define ADIE_TOP_660                    0x660u
#define   ADIE_TOP_660_MT7977           0x4c460960u
#define ADIE_TOP_664                    0x664u
#define   ADIE_TOP_664_MT7976           0x34d00fe0u
#define   ADIE_TOP_664_MT7976C          0x34c00fe0u
#define   ADIE_TOP_664_MT7977           0xffa428afu
#define ADIE_TOP_668                    0x668u
#define   ADIE_TOP_668_MT7977           0xffd42d00u
#define   ADIE_TOP_668_MT7977_V12       0xffd40000u
#define ADIE_TOP_66C                    0x66cu
#define   ADIE_TOP_66C_MT7977           0x5a00ab00u
#define ADIE_TOP_7E2                    0x7e2u
#define   ADIE_TOP_7E2_MT7977           0x00000155u
#define ADIE_TOP_7E4                    0x7e4u	/* = 0 */
#define ADIE_TOP_7E5                    0x7e5u
#define   ADIE_TOP_7E5_MT7977           0x01110111u
#define ADIE_TOP_7E6                    0x7e6u	/* 0x7e6..0x7e8 = 0 */
#define ADIE_TOP_7E8                    0x7e8u
#define ADIE_TOP_220                    0x220u	/* the only TOP 0x2xx index ever read: see HAZARD below */
#define   ADIE_TOP_220_TRIM             HW_GENMASK(23, 20)
#define   ADIE_TOP_220_MT7977           0x08844080u
#define   ADIE_TOP_220_MT7977_V12       0x08a4c080u
#define ADIE_TOP_034                    0x034u
#define   ADIE_TOP_034_TRIM             HW_GENMASK(15, 12)	/* MT7976 */
#define   ADIE_TOP_034_MT7977           0xc01011f0u
#define   ADIE_TOP_034_MT7977_V12       0xc01091f0u
#define ADIE_TOP_030                    0x030u
#define   ADIE_TOP_030_B18              HW_BIT(18)	/* pulsed 0 -> 1 */
#define   ADIE_TOP_4AC_LO5              HW_GENMASK(4, 0)
#define   ADIE_TOP_4AC_LO5_VAL          0x1cu
#define ADIE_TOP_CHAIN_IF_AFE_PULSE     0x88880005u	/* MT7976 */
#define ADIE_TOP_CHAIN_IF_IDLE          0x00000005u
#define MT7977_TOP_AFE_PULSE            0x88880000u	/* MT7977_TOP_DCOC_WF(0..4) */
#define ADIE_TRIM_EFUSE_VALID           HW_BIT(7)
#define ADIE_TRIM_EFUSE_CODE            HW_GENMASK(6, 3)
#define ADIE_CHIP_ID_MT7976C_A          0x79768c10u
#define ADIE_CHIP_ID_MT7976C_B          0x79768b00u
#define ADIE_CHIP_ID_MT7976C_C          0x79768a10u
#define ADIE_CHIP_ID_MT7976_B           0x79768c00u
#define ADIE_CHIP_ID_MT7977_V1          0x79778a00u
#define ADIE_CHIP_ID_MT7977_V2          0x79778a01u
#define ADIE_CHIP_ID_MT7977_V3B         0x79778a11u

/* --- RF_INIT and the WF tables (calibration item 0) ----------------------
 * Item 0 (phy_cal.h MT7996_PHY_CALMASK_RF_INIT), run outside phyCalDo from
 * the calibration init's band loop (phy_cal.h "Calibration init"), D-die
 * words first: [static][measured]
 *  MT7976 (die 0, V:e00512e8): WF_COMM +0xd414 (phy.h MT7996_PHY_RFIF_D414)
 *   = 0xe3220503 and the five MT7996_PHY_RF_TBL_KEY words = 0; per WF 0..4
 *   the RF_INIT pre-rows (descriptor 0xe0075640, 0x33 rows) and table walker
 *   (descriptor 0xe0075540); then per WF 0..3 RF_COMMON (the +0xd414 test,
 *   the key record, pre-rows 0xe0075508 (9 rows), walker 0xe0074998 (0x1f
 *   groups)), RF_EPA_COMMON (walker 0xe0074514), XPA (pre-rows 0xe007488c
 *   (5 rows), TOP 0x71c = 0) and XLNA (pre-rows 0xe008137c (1 row)); last
 *   the MT7976C test (TOP 0x02c [11:8] == 0xc; 0x79768a20 here: no "dap"
 *   tables).
 *  MT7977 (dies 1, 2, V:e0051c04): +0xd414 = 0x77e40312 (ECO byte > 9;
 *   0x77e20312 else), the key records = 0, then per WF 0..4 the pre-rows
 *   (descriptor 0xe007fdb4, 0x35 rows) and walker (descriptor 0xe007f964, 6
 *   groups); die 2's ECO byte selects the variant for die 1 too (mt7977.h).
 * 674 / 501 / 498 A-die registers of dies 0 / 1 / 2 keep their RF_INIT value
 * through the stock load (4 per MT7977 die are rewritten by the channel
 * set): it is cold-init state for any receiver, with the synthesizer-bank
 * defaults the SX needs. The R-cal (T0[0], gated) and the RC calibration
 * (T0[1]) follow (below).
 */

/* --- RC calibration and RC trim (the filter corner) ---------------------
 * The firmware measures one (MT7976) or two (MT7977) RC time constants
 * against the reference clock once at power-on and copies the resulting
 * 5-bit codes into every chain of the die; nothing rewrites them on a
 * channel or BW change. [static]
 * Sequence (calibration mask bit 5, phy_cal.h; once per die after RF_INIT
 * and the R-cal), on the measurement bank M, with fixed waits and no
 * completion flag: [static][measured]
 *  MT7977 (M = k 2): M 0x171[10] = 0 then 1; TOP 0x3a4 |= [14], [22:21] = 1;
 *   M 0xe50 [31:16] = 0xc810, [9:0] = 0x0e4; M 0xe51[0] = 1, 40 us, 0;
 *   TOP 0x3a4 [22:21] = 2; M 0xe50 [31:16] = 0x4810, then 0x8810;
 *   M 0xe59 |= [8]; M 0xe51[0] = 1, 40 us, 0; M 0xe59 &= ~[8];
 *   code 1 = M 0xece[20:16] -> 0xe51[12:8] on k 0..4, then [16] = 1;
 *   code 2 = M 0xece[14:10] -> 0xe59[5:1] on k 0..4, then [0] = 1;
 *   M 0xe50 [31:16] = 0x4810; TOP 0x3a4 [22:21] = 0, then [14] = 0.
 *  MT7976 (M = k 0): M 0x171[10] = 0 then 1; M 0xe50[9:0] = the target for
 *   the reference clock; M 0xe51[0] = 1, 40 us, 0; code = M 0xece[4:0] ->
 *   0xe51[12:8] on k 0..4, then [16] = 1.
 * The codes are measurements and vary between cold loads (die 2 code 1: 10
 * in one load, 9 in another). [measured]
 * Measured effect of 0xe51[12:8] at BW code 0 (die 0, 2350 MHz): codes
 * 0 -> 28 move the -3 dB corner 11.7 -> 10.0 MHz, monotonic, about 0.6 % per
 * step; larger code = lower corner. Write the code with bit 16 set, on every
 * chain. [measured]
 * At BW code 3 the trim acts about 2.5 times harder (die 0, band 0 at 320
 * Msps, 2026-10-11): codes 0 / 17 (the calibrated code) / 31 put the -3 dB
 * corner at about +-85 / +-68 / +-55 MHz, about 1.4 % per step
 * (mt7976.h "RX analog filter"). [measured]
 */

/* 0xe50  RC-cal target and mode (measurement chain). RW. [static][measured]
 * [9:0] target count in reference cycles; larger = longer RC = lower corner.
 * MT7977: firmware writes 0x0e4; observed 0x08e on die 2. MT7976: chosen by
 * the reference clock (24/25/26/40/52/80 MHz -> 0x07c/0x081/0x086/0x0ce/
 * 0x113/0x19c, about f_ref x 5.15 us); observed 0x0ce (40 MHz).
 * [31:16] mode word (MT7977): 0x6810 at power-on, 0xc810 for measurement 1,
 * 0x4810 then 0x8810 for measurement 2, 0x4810 at the end; [31] is set while
 * a measurement runs, [30] for measurement 1 and at rest, [29] only at
 * power-on [inferred]. Observed MT7977 power-on 0x6810008e, after the
 * calibration 0x481000e4; MT7976 0x000000ce before and after. [measured] */
#define ADIE_RCCAL_BANK_MT7977          2u	/* measurement bank (k) */
#define ADIE_RCCAL_BANK_MT7976          0u
#define ADIE_RCCAL_CFG                  0xe50u
#define ADIE_RCCAL_CFG_TARGET           HW_GENMASK(9, 0)
#define ADIE_RCCAL_CFG_MODE             HW_GENMASK(31, 16)
#define ADIE_RCCAL_MODE_POR             0x6810u
#define ADIE_RCCAL_MODE_MEAS1           0xc810u
#define ADIE_RCCAL_MODE_IDLE            0x4810u
#define ADIE_RCCAL_MODE_MEAS2           0x8810u
#define ADIE_RCCAL_TARGET_MT7977        0x0e4u
/* MT7976 RC-cal target (ADIE_RCCAL_CFG_TARGET) by the reference clock in Hz, P+0x10 of the
 * calibration block (V:e006030c..V:e006034e, V:e0060520..V:e006053e); any other clock writes 0.
 * The W1700K's reference is 40 MHz [inferred: P+0x10's writer is not traced; 40 MHz gives the
 * stock load's 0xe50 = 0x0ce on die 0, and 40 / 80 MHz reproduce every TTG word, phy_cal.h].
 * The MT7977 path writes ADIE_RCCAL_TARGET_MT7977 whatever the clock. [static][measured] */
#define ADIE_RCCAL_REF_HZ                40000000u
#define ADIE_RCCAL_TARGET_MT7976(hz) \
	((hz) == 24000000u ? 0x07cu : (hz) == 25000000u ? 0x081u : (hz) == 26000000u ? 0x086u : \
	 (hz) == 40000000u ? 0x0ceu : (hz) == 52000000u ? 0x113u : (hz) == 80000000u ? 0x19cu : 0u)

/* 0xe51  RC trim code 1 (every chain). RW. Observed MT7976 power-on
 * 0x00001000 (code 16); MT7977 after RF_INIT 0x00c00c00 (code 12, [23:22] =
 * 3, kept by the calibration's read-modify-write); after the calibration die
 * 0 0x00011100 (code 17), die 1 0x00c10b00 (11), die 2 0x00c10a00 (10).
 * [static][measured]
 * [12:8] code 0..31 (the filter corner trim); [16] set after the code is
 * written (software code valid, inferred); [0] RC-cal start: set, wait 40 us,
 * clear (measurement chain). [23:22] seen set on die 2, meaning unknown. */
#define ADIE_RCTRIM                     0xe51u
#define ADIE_RCTRIM_START               HW_BIT(0)
#define ADIE_RCTRIM_CODE                HW_GENMASK(12, 8)
#define ADIE_RCTRIM_SW                  HW_BIT(16)
#define ADIE_RCTRIM_CODE_MAX            31u
#define ADIE_RCTRIM_START_US            40u

/* 0xe59  RC trim code 2, MT7977 only. RW. Power-on 0x4c0; after the
 * calibration 0x4d9 on die 1 (code 12), 0x4dd on die 2 (code 14).
 * [5:1] code from the second measurement; [0] set after the code (valid);
 * [8] selects the second measurement while the start bit pulses. Which block
 * it trims is not established. [static][measured] */
#define ADIE_RCTRIM2                    0xe59u
#define ADIE_RCTRIM2_VALID              HW_BIT(0)
#define ADIE_RCTRIM2_CODE               HW_GENMASK(5, 1)
#define ADIE_RCTRIM2_PHASE2             HW_BIT(8)

/* 0xece  RC-cal result. RO. MT7977: [20:16] -> 0xe51 code, [14:10] -> 0xe59
 * code. MT7976: [4:0] -> 0xe51 code. Observed die 0 0x00114211, die 1
 * 0x000b320b, die 2 0x000a3a0a and 0x00093a09 (two cold loads): [4:0]
 * equals [20:16] and [9:5] = 0x10 in all four. [static][measured] */
#define ADIE_RCCAL_RESULT               0xeceu
#define ADIE_RCCAL_RESULT_MT7977_C1     HW_GENMASK(20, 16)
#define ADIE_RCCAL_RESULT_MT7977_C2     HW_GENMASK(14, 10)
#define ADIE_RCCAL_RESULT_MT7976_C1     HW_GENMASK(4, 0)

/* 0xe40  R-cal (resistor trim) code. RW. [static][measured]
 * Applied (calibration mask bit 4, phy_cal.h) from the die's R-cal byte in
 * the host EEPROM image (MT7996_PHY_EE_RCAL) by two read-modify-writes per
 * bank: the code into its field, then [9] set. No measurement, no read-back,
 * and no later writer (it is in no calibration backup list).
 * MT7977: [8:4] = byte[4:0] on k 0..4 (k 4 = the synthesizer bank); nothing
 * is written if the byte is not valid. Power-on 0x108.
 * MT7976: [7:4] = byte[3:0] on k 0..3 only (k 4, the monitor receiver, keeps
 * RF_INIT's 0x368); not valid -> [9:4] = 0x36. RF_INIT writes 0x368 on
 * k 0..4 first, so [8] stays 1.
 * Observed after the calibration: die 0 0x378, die 1 0x328, die 2 0x368. Its
 * effect is not characterised (it moves gain and bias too). On die 0 at BW
 * code 3 (RC trim 0, 320 Msps) [7:4] = 0, 7 or 15 leaves the RX filter
 * response within 0.5 dB to +-150 MHz: it does not set the filter corner.
 * [measured] */
#define ADIE_RCAL                       0xe40u
#define ADIE_RCAL_CODE_MT7977           HW_GENMASK(8, 4)
#define ADIE_RCAL_CODE_MT7976           HW_GENMASK(7, 4)
#define ADIE_RCAL_EN                    HW_BIT(9)
#define ADIE_RCAL_DEFAULT_MT7976        0x36u		/* in [9:4] */
#define ADIE_RCAL_POR_MT7977            0x108u
#define ADIE_RCAL_RFINIT_MT7976         0x368u
#define ADIE_RCAL_BANKS_MT7976          4u		/* k 0..3 */
#define ADIE_RCAL_BANKS_MT7977          5u		/* k 0..4 */
/* MT7976 R-cal with an invalid EEPROM byte (MT7996_PHY_EE_RCAL_VALID clear): one
 * read-modify-write per bank k 0..3 puts ADIE_RCAL_DEFAULT_MT7976 into [9:4]
 * (V:e0041ad0..V:e0041af2, andi -0x3f1); [9] (ADIE_RCAL_EN) is not set separately. [static] */
#define ADIE_RCAL_DEFAULT_FIELD_MT7976   HW_GENMASK(9, 4)
/* 0xecc  On-chip R-cal measurement, [31:27]. RO. The firmware's measurement
 * routine has no caller. [static] */
#define ADIE_RCAL_RESULT                0xeccu
#define ADIE_RCAL_RESULT_CODE           HW_GENMASK(31, 27)

/* --- RX DC-offset calibration (DCOC), chain bank k = WF -----------------
 * Two firmware items (phy_cal.h MT7996_PHY_CALMASK_RX_DCOC / _RX_DCOC2), each
 * run per chain at every calibration group's LO and RX bandwidth class (the
 * MT7977 bodies write the class to 0x100 and 0x102 first):
 *   fine (mask bit 8): chain in ADIE_CHAIN_EN_RX_DCOC (rfmodes.h; MT7977: SX
 *     bank 0x000 = 0x82000000); one coarse conversion (0x923[23], done when
 *     0x987[15:13] == 0); then the fine engine (0x922[31], done when
 *     0x98e[15] == 1) sweeps the gain steps itself, 6 on the MT7977 and 5 on
 *     the MT7976, and leaves 7-bit codes in ADIE_DCOC_FI0..2 / ADIE_DCOC_FQ0..2;
 *   coarse per step (mask bit 9): chain in ADIE_CHAIN_EN_RX, mode 0x88; the
 *     firmware writes the step index ADIE_DCOC_STEP_IDX (5..0 MT7977, 4..0
 *     MT7976) and fires one coarse conversion per step; signed bytes land in
 *     ADIE_DCOC_SI0..2 / ADIE_DCOC_SQ0..2.
 * The firmware computes no code: it reads the six result words, range-checks
 * them, stores their fields per calibration group in WM RAM and writes the
 * fields back (phy_cal.h); every other register it touched is restored from
 * the backup taken before the calibration. The measurement is entirely
 * inside the A-die: no D-die capture, no RX-gain forcing on the D-die.
 * Rest values below are the backup image (= the A-die state before the
 * calibration, identical in every pass and in an independent cold load).
 * Die specifics: mt7977.h (SX bank, TOP) and mt7976.h. [static][measured]
 */

/* 0x922  Fine DC engine control. RW. Rest 0. [static][measured]
 * Fine cal: [27:26] = 2, [19:18] = 3 (MT7977 only), [17] = 0, then [31] = 1;
 * poll 0x98e[15] == 1 (3 matching reads, 1 ms silent deadline); [31] = 0.
 * TX loop-DC mode (mask bit 10, below; both die types): whole 0x09028000
 * ([27:26] = 2, [25:24] = 1, [17] = 1, [15] = 1), then [31] pulsed once per
 * PA step with the same done poll. [static][measured] */
#define ADIE_DCOC_FINE                  0x922u
#define ADIE_DCOC_FINE_START            HW_BIT(31)
#define ADIE_DCOC_FINE_MODE             HW_GENMASK(27, 26)	/* 2 */
#define ADIE_DCOC_FINE_MODE_DC          2u
#define ADIE_DCOC_FINE_2524             HW_GENMASK(25, 24)	/* 1 in loop-DC mode */
#define ADIE_DCOC_FINE_1918             HW_GENMASK(19, 18)	/* 3 on the MT7977 */
#define ADIE_DCOC_FINE_B17              HW_BIT(17)		/* 0; 1 in loop-DC mode */
#define ADIE_DCOC_FINE_B15              HW_BIT(15)		/* 1 in loop-DC mode */
#define ADIE_DCOC_FINE_LOOPDC           0x09028000u

/* 0x923  Coarse DC conversion and coarse correction codes. RW. Rest
 * 0x00030000. [static][measured]
 * [23] start one conversion (firmware sets, polls 0x987[15:13] == 0, clears);
 * [21] apply the codes in [15:0] [inferred]; [15:8] code B (copied from
 * 0x987[23:16]), [7:0] code A (copied from 0x987[31:24]).
 * Fine cal: MT7976 writes [21] = 1 and the two measured codes, then runs the
 * fine engine with them; MT7977 writes the codes, then the whole word
 * 0x00230000 ([21] = 1, [17:16] = 3, codes 0): its measured coarse codes are
 * discarded before the fine step. Coarse per step: [21], [19:18], [17:16]
 * cleared first. Both end with the restore to the rest value, so no coarse
 * code survives a calibration. */
#define ADIE_DCOC_COARSE                0x923u
#define ADIE_DCOC_COARSE_START          HW_BIT(23)
#define ADIE_DCOC_COARSE_APPLY          HW_BIT(21)
#define ADIE_DCOC_COARSE_1918           HW_GENMASK(19, 18)
#define ADIE_DCOC_COARSE_1716           HW_GENMASK(17, 16)	/* 3 at rest */
#define ADIE_DCOC_COARSE_CODE_B         HW_GENMASK(15, 8)
#define ADIE_DCOC_COARSE_CODE_A         HW_GENMASK(7, 0)
#define ADIE_DCOC_COARSE_MT7977_FINE    0x00230000u

/* 0x924  DCOC setup. RW. Rest 0x00000802. Both items: [15] = 1, then
 * [13:12] = 0 (first accesses of each body). The TX loop DC (below) does the
 * same, then [3:0] = 9 (MT7977, P[6] = 1); with P[6] = 0 it writes the whole
 * word 0x8809 [static]. [static][measured] */
#define ADIE_DCOC_SETUP                 0x924u
#define ADIE_DCOC_SETUP_EN              HW_BIT(15)
#define ADIE_DCOC_SETUP_1312            HW_GENMASK(13, 12)
#define ADIE_DCOC_SETUP_LO              HW_GENMASK(3, 0)
#define ADIE_DCOC_SETUP_LO_LOOPDC       9u

/* 0x925  MT7977 fine-cal setup. RW. Rest 0x40520050. Fine cal: [7:4] = 5 for
 * calibration group 0, else [23:20] = 5 (both already 5 at rest).
 * [static][measured] */
#define ADIE_DCOC_SETUP2                0x925u
#define ADIE_DCOC_SETUP2_G0             HW_GENMASK(7, 4)
#define ADIE_DCOC_SETUP2_GN             HW_GENMASK(23, 20)
#define   ADIE_DCOC_SETUP2_VAL          5u	/* [7:4] (group 0) / [23:20] (other groups) */

/* 0x927 / 0x928 / 0x929  Fine-cal start (DAC) codes: four bytes (0x929:
 * one). The fine cal writes 0x18 to every byte (0x18181818, 0x929 = 0x18);
 * rest 0x09 per byte on the MT7976, 0x12 on the MT7977 (restored).
 * [static][measured] */
#define ADIE_DCOC_927                   0x927u
#define ADIE_DCOC_928                   0x928u
#define ADIE_DCOC_929                   0x929u
#define ADIE_DCOC_START_CODE            0x18u
#define ADIE_DCOC_START_CODES           0x18181818u	/* 0x927 / 0x928: ADIE_DCOC_START_CODE in every byte */

/* 0x987  Coarse conversion readback. RO. [31:24] code A, [23:16] code B,
 * [15:13] busy (0 = done; the conversion is done by the first poll read).
 * Observed 0xff011000 (die 0 WF0), 0x94871000 (die 1 WF0) after a
 * conversion; outside a calibration it changes between reads (observed
 * 0xfcff0100). The register also changes with the BW class (BW LUT above).
 * [static][measured] */
#define ADIE_DCOC_DC_RB                 0x987u
#define ADIE_DCOC_DC_RB_CODE_A          HW_GENMASK(31, 24)
#define ADIE_DCOC_DC_RB_CODE_B          HW_GENMASK(23, 16)
#define ADIE_DCOC_DC_RB_BUSY            HW_GENMASK(15, 13)

/* 0x98e  Fine engine status. RO. [15] done; [8] busy [inferred: set in the
 * first poll read, clear in the last]; [31:28] gain-step counter while it runs
 * (0..5 MT7977, 1..4 MT7976). Done values 0x018088c0 (MT7977), 0x01808840
 * (MT7976); the fine cal takes 38..47 poll reads on the MT7977, 26 on the
 * MT7976. [static][measured]
 * [6:0] the engine's current code. In the TX loop DC (below): 0x40 at the
 * start read (0x...29c0), the step's result at the done read (0x...a8xx;
 * [7] = 1 in both), equal to the code then found in the result row in 140 /
 * 140 calls; [31:28] stays 5 (0 on WF 1 in the T1[14] and band-1 calls)
 * through the steps: no step counter in this mode. [measured] */
#define ADIE_DCOC_DONE                  0x98eu
#define ADIE_DCOC_DONE_FLAG             HW_BIT(15)
#define ADIE_DCOC_DONE_BUSY             HW_BIT(8)
#define ADIE_DCOC_DONE_STEP             HW_GENMASK(31, 28)
#define ADIE_DCOC_DONE_CODE             HW_GENMASK(6, 0)

/* The DCOC poll (MediaTek's RfRegPoll V:e00444e0 with timeout 1 ms): 10 us, then
 * reads until three reads (not necessarily consecutive) satisfy the condition, 1 us
 * after each satisfying read; one deadline of 1 ms over the whole poll, after which a
 * satisfying last read still counts and one more read is tried. The DCOC bodies poll
 * ADIE_DCOC_DC_RB [15:13] == 0 (coarse conversion) and ADIE_DCOC_DONE [15] == 1 (fine
 * engine). Stock: every one of 2,728 polls ended by its condition; the fine engine
 * takes 26 (MT7976) to 47 (MT7977) reads. [static][measured] */
#define ADIE_DCOC_POLL_MATCHES          3u
#define ADIE_DCOC_POLL_PRE_US           10u
#define ADIE_DCOC_POLL_MATCH_US         1u
#define ADIE_DCOC_POLL_DEADLINE_US      1000u

/* EEPROM (host EEPROM image): byte 0x1ae [1] set clears mask bit 8 (fine),
 * [0] mask bit 9 (coarse per step), at calibration init (V:e00d6b16..,
 * V:e00e04d6 idx 7 / 6). 0 on this unit. [static][measured] */
#define ADIE_DCOC_EE_OFF                0x1aeu
#define ADIE_DCOC_EE_OFF_FINE           HW_BIT(1)
#define ADIE_DCOC_EE_OFF_STEP           HW_BIT(0)

/* 0x8e0..0x8e2, 0x8e6..0x8e8  Fine DC results, two identical triplets (I and
 * Q paths [inferred]), 7-bit codes, mid-scale 0x40. Written by the fine
 * engine; RW (the firmware writes the fields back). Six slots per triplet:
 *   slot 0  0x8e0/0x8e6 [6:0]     (MT7977 only; the MT7976 leaves 0x40)
 *   slot 1..4  0x8e1/0x8e7 [6:0], [14:8], [22:16], [30:24]
 *   slot 5  0x8e2/0x8e8 [30:24]
 * Bits outside the fields are plain storage: 0x8e0 [31:8] reads 0x404040 at
 * power-on and 0 after the first write-back. Which slot is which gain step is
 * not established (slot 0 is the step the MT7976 lacks). Accepted range
 * 0x18..0x68; observed this load 56..77 (die 0 56..70). DPD (phy_cal.h
 * T2{0..3}) reruns the fine engine on its MT7976 feedback chain at the DPD
 * gain and restores none of these words (phy_cal.h HAZARD (RX)).
 * [static][measured] */
#define ADIE_DCOC_FI0                   0x8e0u
#define ADIE_DCOC_FI1                   0x8e1u
#define ADIE_DCOC_FI2                   0x8e2u
#define ADIE_DCOC_FQ0                   0x8e6u
#define ADIE_DCOC_FQ1                   0x8e7u
#define ADIE_DCOC_FQ2                   0x8e8u
#define ADIE_DCOC_F_SLOT0               HW_GENMASK(6, 0)	/* FI0 / FQ0 */
#define ADIE_DCOC_F_SLOT(n)             HW_GENMASK(8u * ((n) - 1u) + 6u, 8u * ((n) - 1u))	/* FI1 / FQ1, n 1..4 */
#define ADIE_DCOC_F_SLOT5               HW_GENMASK(30, 24)	/* FI2 / FQ2 */
#define ADIE_DCOC_F_MID                 0x40u
#define ADIE_DCOC_F_MIN                 0x18u
#define ADIE_DCOC_F_MAX                 0x68u
#define ADIE_DCOC_F_DEFAULT             0x40404040u	/* written when no record is valid */
#define ADIE_DCOC_F_SLOTS1_4            (ADIE_DCOC_F_SLOT(1u) | ADIE_DCOC_F_SLOT(2u) | ADIE_DCOC_F_SLOT(3u) | \
					 ADIE_DCOC_F_SLOT(4u))

/* 0x8f0..0x8f2, 0x8f6..0x8f8  Coarse per-step DC results, same slot layout as
 * the fine results with signed 8-bit fields: slot 0 0x8f0/0x8f6 [7:0] (MT7977
 * only), slots 1..4 0x8f1/0x8f7 bytes 0..3, slot 5 0x8f2/0x8f8 [31:24].
 * Accepted range -0x40..0x40 (see the check bug in phy_cal.h); observed this
 * load -7..+5. Default (no valid record) 0. [static][measured] */
#define ADIE_DCOC_SI0                   0x8f0u
#define ADIE_DCOC_SI1                   0x8f1u
#define ADIE_DCOC_SI2                   0x8f2u
#define ADIE_DCOC_SQ0                   0x8f6u
#define ADIE_DCOC_SQ1                   0x8f7u
#define ADIE_DCOC_SQ2                   0x8f8u
#define ADIE_DCOC_S_SLOT0               HW_GENMASK(7, 0)
#define ADIE_DCOC_S_SLOT(n)             HW_GENMASK(8u * ((n) - 1u) + 7u, 8u * ((n) - 1u))	/* n 1..4 */
#define ADIE_DCOC_S_SLOT5               HW_GENMASK(31, 24)
#define ADIE_DCOC_S_LIMIT               0x40		/* signed, +- */
#define ADIE_DCOC_S_SLOTS1_4            (ADIE_DCOC_S_SLOT(1u) | ADIE_DCOC_S_SLOT(2u) | ADIE_DCOC_S_SLOT(3u) | \
					 ADIE_DCOC_S_SLOT(4u))
#define ADIE_DCOC_S_DEFAULT             0u	/* written when the coarse codes fail */
/* Fine codes are 7-bit, accepted in [ADIE_DCOC_F_MIN, ADIE_DCOC_F_MAX]; coarse
 * codes signed bytes, accepted within +-ADIE_DCOC_S_LIMIT. A failed check
 * makes MediaTek write the item's defaults to its six rows instead of the codes
 * (V:e0044c62 / V:e0044d24, flag word phy_cal.h MT7996_PHY_DCOC_FAIL_FLAGS).
 * MediaTek's check (V:e00454ee) masks one bit too many (7-bit fields as bytes)
 * and its shift wraps mod 32, so it checks nothing of 0x8f2 / 0x8f8 [31:24] and
 * only bit 0 of 0x8f1 / 0x8f7. None failed in the stock load. Every code field
 * sits in one byte lane n (0..3) of its row. [static][measured] */
#define ADIE_DCOC_LANES                 4u
#define ADIE_DCOC_LANE(n)               HW_GENMASK(8u * (n) + 7u, 8u * (n))

/* 0x600 (chain bank, not TOP 0x600)  RX step override; the same register is
 * MT7976_MON_CTL for the monitor receiver (mt7976.h). RW. Rest 0x00000806
 * (both die types). Coarse per step: [16] = 1, then [11:8] = step index 5..0
 * (MT7977) / 4..0 (MT7976) before each conversion; restored afterwards.
 * The WM's A-die RX gain setter (V:e0064a14 MT7976, V:e0068982 MT7977)
 * writes [16] = enable, [11:8] = LNA index, [4:1] = LPF index, so the step
 * index is an LNA gain index; T2{5} sets LNA 1 / LPF 3, the MT7976 TX test
 * mode of T2{4} [11:8] = 0 and [4:1] = 5, 9, then 8, and DPD on its MT7976
 * feedback chain [11:8] = 0, [4:1] = the AGC gain code 0..9 (rfmodes.h).
 * The TTG RX calibrations force every bank's gain per step: [16] = 1,
 * [11:8] = LNA (MT7977 5..0, and 7 in T1[23]'s steps 6..11; MT7976 4..0),
 * [4:1] = LPF (phy_cal.h MT7996_PHY_TTG_RX_*). Observed 0x00000806 at rest.
 * [static][measured] */
#define ADIE_DCOC_STEP                  0x600u
#define ADIE_DCOC_STEP_EN               HW_BIT(16)
#define ADIE_DCOC_STEP_IDX              HW_GENMASK(11, 8)
#define ADIE_DCOC_STEP_LPF              HW_GENMASK(4, 1)
#define ADIE_DCOC_STEPS_MT7977          { 5u, 4u, 3u, 2u, 1u, 0u }	/* WM 0xe00812d0 */
#define ADIE_DCOC_STEPS_MT7976          { 4u, 3u, 2u, 1u, 0u }		/* WM 0xe00812b0 */

/* 0x024  [31:30]: fine cal = 2, coarse per step |= 3, TX loop DC = 2 (both
 * die types; the MT7977 clears it after the steps). Rest 0x00000044.
 * [static][measured] */
#define ADIE_DCOC_024                   0x024u
#define ADIE_DCOC_024_MODE              HW_GENMASK(31, 30)
#define   ADIE_DCOC_024_MODE_FINE       2u	/* fine: = 2 */
#define   ADIE_DCOC_024_MODE_STEP       3u	/* coarse per step: |= 3 */

/* 0x031 / 0x032  Calibration-tail words (every T1/T2 body, V:e00486fc):
 * 0x031 [7:0] = 8, [23:16] = 0x10; 0x032 [23:16] = 0; then restored. Rest
 * 0x031 = 0x00105008 (MT7976), 0x00285008 (MT7977); 0x032 = 0. [static][measured] */
#define ADIE_CALTAIL_031                0x031u
#define ADIE_CALTAIL_031_LO             HW_GENMASK(7, 0)
#define ADIE_CALTAIL_031_HI             HW_GENMASK(23, 16)
#define ADIE_CALTAIL_032                0x032u
#define ADIE_CALTAIL_032_HI             HW_GENMASK(23, 16)
/* The tail V:e00486fc writes 0x031 [7:0] = 8, then [23:16] = 0x10, then
 * 0x032 [23:16] = 0 (dcoc.c). [static][measured] */
#define ADIE_CALTAIL_031_LO_VAL         8u
#define ADIE_CALTAIL_031_HI_VAL         0x10u
#define ADIE_CALTAIL_032_HI_VAL         0u

/* 0x080  Effective chain block enables. RO. Reads 0x8003fbcb while 0x010 =
 * ADIE_CHAIN_EN_RX_DCOC with 0x011[31] = 1 [measured]. The fine cal reads it
 * last: [31] set -> ADIE_STROBE_CAL (0x171[11]) pulsed low for 10 us. [static] */
#define ADIE_CHAIN_EN_RB                0x080u
#define ADIE_CHAIN_EN_RB_DCOC           HW_BIT(31)
/* [31] set at the end of the fine body: ADIE_STROBE_CAL held low this
 * long (V:e004c540 / V:e005f706). [static] */
#define ADIE_DCOC_STROBE_US             10u

/* MT7977 chain-bank words of the fine cal (rest values restored): [static][measured]
 * 0x11a [14] set for the fine step (rest 0x80013814);
 * 0x11b [0], [3], [4], [8] set and [2:1] cleared around the coarse conversion,
 *       all cleared before the fine step (rest 0x01000880);
 * 0x403 (mt7977.h MT7977_RCD_PRE) [5] set for the fine step (the SX retune
 *       writes 0x15 first; rest 0x0f000000);
 * 0x744 (mt7977.h MT7977_RCD_744) [8] set for the fine step (SX retune writes
 *       0x00200004; rest 0x00200000);
 * 0x101 / 0x103 [0] = (group != 0) (coarse per step and MT7976: whole word);
 * 0x601 / 0x602 [0] = 0 (rest 0). */
#define ADIE_DCOC_11A                   0x11au
#define ADIE_DCOC_11A_FINE              HW_BIT(14)
#define ADIE_DCOC_11B                   0x11bu
#define ADIE_DCOC_11B_COARSE_SET        0x00000119u	/* [0] [3] [4] [8] */
#define ADIE_DCOC_11B_COARSE_CLR        HW_GENMASK(2, 1)
/* 0x11b, MT7977 fine body: around the coarse
 * conversion [0] set, [3] set, [2:1] cleared, [4] set, [8] set, one
 * read-modify-write each in this order; after it [0], [3], [2:1], [4], [8]
 * cleared, in the same order. [static][measured] */
#define ADIE_DCOC_11B_B0                HW_BIT(0)
#define ADIE_DCOC_11B_B3                HW_BIT(3)
#define ADIE_DCOC_11B_B4                HW_BIT(4)
#define ADIE_DCOC_11B_B8                HW_BIT(8)
#define ADIE_DCOC_403_FINE              HW_BIT(5)	/* 0x403 */
#define ADIE_DCOC_744_FINE              HW_BIT(8)	/* chain 0x744 */
/* 0x601 / 0x602  Chain words: both MT7977 DCOC bodies clear [0] of each
 * (read-modify-write, after the FEM helper); rest 0 on both [measured: the
 * backup image]; restored. On the MT7976 the same indices are
 * MT7976_LO_DIST5G and its companion (5 GHz LO distribution; RF_INIT writes
 * 0x601 = 0, 0x602 = 1, rfinit.h); their role on the MT7977 is unknown.
 * [static][measured] */
#define ADIE_DCOC_601                   0x601u
#define ADIE_DCOC_602                   0x602u
#define ADIE_DCOC_60X_B0                HW_BIT(0)

/* --- TX loop-DC calibration, chain bank k = WF ---------------------------
 * Firmware item phy_cal.h MT7996_PHY_CALMASK_LOOP_DC (T1{6,10,14}): on the
 * MT7977 the chain in TX (0x010 = 0x01fa23c5, rfmodes.h), CALPATH 3, and the
 * fine DC engine 0x922 in loop-DC mode (ADIE_DCOC_FINE_LOOPDC) run once per
 * PA step 0x4a0 [27:24] = 0..7 (rfmodes.h ADIE_PA_4A0_STEP); each run leaves
 * one 7-bit code. The MT7976 (EPA board) sets up and saves without measuring.
 * Sequence and result handling: phy_cal.h. Rest values are the backup image;
 * every register below except the result rows is restored. [static][measured]
 */

/* 0x8ef, 0x8ee, 0x8ed, 0x8ec, 0x8ff, 0x8fe, 0x8fd, 0x8fc  Loop-DC result rows
 * (WM row order), four 7-bit fields each at [6:0], [14:8], [22:16], [30:24];
 * RW (the WM writes them back; phy_map.h indirect item 28). The engine in
 * loop-DC mode stores step s's code (0x4a0 [27:24] = s) in byte s of 0x8ef
 * (s 0..3) and byte s - 4 of 0x8ee (s 4..7): the code is 0x98e [6:0] at the
 * step's done read, 140 / 140 calls [measured]. 0x8ec, 0x8ed and
 * 0x8fc..0x8ff are never written by the engine in the stock load (saved and
 * written back only). Read at the first loop-DC call: 0x40404040 (MT7977 all
 * rows; MT7976 0x8ec..0x8ef), 0 (MT7976 0x8fc..0x8ff). Stock codes 0x08..0x32
 * (die 1), 0x0a..0x33 (die 2); accepted range 0x0a..0x60 (phy_cal.h
 * MT7996_PHY_LOOPDC_MIN / _MAX); default 0x40404040 (mid-scale), which is
 * what every row holds after each call and channel set in the stock load.
 * T2{5} reads [6:0] of 0x8ef / 0x8ee into 0x920 [26:20] (rfmodes.h
 * ADIE_TX_LVL_920). Hardware use: TX DC / LO-leakage correction per PA step
 * [inferred: measured with the chain in TX, read by T2{5} per PA step].
 * [static][measured] */
#define ADIE_LOOPDC_RES0                0x8efu	/* steps 0..3 */
#define ADIE_LOOPDC_RES1                0x8eeu	/* steps 4..7 */
#define ADIE_LOOPDC_RES2                0x8edu
#define ADIE_LOOPDC_RES3                0x8ecu
#define ADIE_LOOPDC_RES4                0x8ffu
#define ADIE_LOOPDC_RES5                0x8feu
#define ADIE_LOOPDC_RES6                0x8fdu
#define ADIE_LOOPDC_RES7                0x8fcu
#define ADIE_LOOPDC_FIELD(n)            HW_GENMASK(8u * (n) + 6u, 8u * (n))	/* n 0..3 */
#define ADIE_LOOPDC_MID                 0x40u
#define ADIE_LOOPDC_DEFAULT             0x40404040u

/* 0x935  Loop-DC word [guess: DC-DAC range]. RW. Rest 0x50000000 (MT7977).
 * MT7977, P[6] = 1: [9:0] = 0x270, then [21:12] = 0x270; P[6] = 0: whole
 * 0xf0000248, then 0xf0000270. MT7976, P[6] = 0: [9:0] = 0, [21:12] = 0x270,
 * then [21:12] = 0 when TOP 0x02c [11:8] == 0xb. Restored. [static][measured] */
#define ADIE_LOOPDC_935                 0x935u
#define ADIE_LOOPDC_935_A               HW_GENMASK(9, 0)
#define ADIE_LOOPDC_935_B               HW_GENMASK(21, 12)
#define ADIE_LOOPDC_935_VAL             0x270u

/* 0x026  MT7977: [31] cleared for the loop DC; written back 0x80880000 (=
 * rest) at its end, then restored. [static][measured] */
#define ADIE_LOOPDC_026                 0x026u
#define ADIE_LOOPDC_026_B31             HW_BIT(31)
#define ADIE_LOOPDC_026_REST            0x80880000u

/* 0x4a4  MT7977: [31:28] per-step value in the P[6] = 0 path (byte table
 * 0xe007192c: 0xf..0x8 twice); 0x70000000 at the end. [static] */
#define ADIE_LOOPDC_4A4                 0x4a4u
#define ADIE_LOOPDC_4A4_STEP            HW_GENMASK(31, 28)

/* 0xbc6 (MT7976) / 0xbc8 (MT7977), chain bank 0: the firmware's SX tune
 * stores the LO in kHz here (k 1 gets its WF parameter P[1]) after the
 * preset tables and before the synthesizer words, not after tuning
 * (sxchan.h; with phy.h MT7996_PHY_SX0_FREQ_KHZ). Nothing reads it back;
 * writing it does nothing. Observed 0x00254310 = 2442000 kHz
 * (die 0), 0x00634228 = 6505000 kHz (die 2). [static][measured] */
#define ADIE_FREQ_NOTE_MT7976           0xbc6u
#define ADIE_FREQ_NOTE_MT7977           0xbc8u

/* ------------------------------------------------------------------------
 * Chain-bank registers of unknown role (k 0..4)
 * ------------------------------------------------------------------------
 * Offset names of the chain-bank words MediaTek's RF tables write and no
 * other name covers, both die types: calibration item 0's RF_INIT,
 * RF_COMMON, RF_EPA_COMMON, XPA and XLNA tables (adie/rfinit.h; "7976C" the
 * MT7976C "dap" tables) and item 1's per-channel MT7977 tables (adie/rfchan.h
 * RFCHAN_*). Whole words; no code reads or tests them. The comment names the
 * tables that write each one. Role unknown. [static][measured]
 */
#define ADIE_CHAIN_013                  0x013u	/* MT7976 RF_INIT, 7976C RF_EPA_COMMON */
#define ADIE_CHAIN_025                  0x025u	/* MT7976 RF_INIT, MT7977 RF_INIT */
#define ADIE_CHAIN_033                  0x033u	/* MT7976 RF_INIT, MT7977 RF_INIT */
#define ADIE_CHAIN_035                  0x035u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_036                  0x036u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_03A                  0x03au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_104                  0x104u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_115                  0x115u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_116                  0x116u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_117                  0x117u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_118                  0x118u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_119                  0x119u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_11D                  0x11du	/* MT7976 RF_INIT, MT7977 RF_INIT */
#define ADIE_CHAIN_11E                  0x11eu	/* MT7976 RF_INIT */
#define ADIE_CHAIN_11F                  0x11fu	/* MT7976 RF_INIT */
#define ADIE_CHAIN_123                  0x123u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_124                  0x124u	/* MT7977 RF_INIT, MT7977 item 1 */
#define ADIE_CHAIN_126                  0x126u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_127                  0x127u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_12A                  0x12au	/* MT7976 RF_INIT */
#define ADIE_CHAIN_12E                  0x12eu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_130                  0x130u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_131                  0x131u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_135                  0x135u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_13A                  0x13au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_13C                  0x13cu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_13E                  0x13eu	/* MT7977 item 1 */
#define ADIE_CHAIN_13F                  0x13fu	/* MT7977 item 1 */
#define ADIE_CHAIN_143                  0x143u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_145                  0x145u	/* MT7977 RF_INIT, MT7977 item 1 */
#define ADIE_CHAIN_148                  0x148u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_14B                  0x14bu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_1D0                  0x1d0u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_242                  0x242u	/* XPA, 7976C XPA */
#define ADIE_CHAIN_243                  0x243u	/* XPA, 7976C XPA */
#define ADIE_CHAIN_245                  0x245u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_246                  0x246u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_247                  0x247u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_24A                  0x24au	/* MT7977 item 1 */
#define ADIE_CHAIN_24B                  0x24bu	/* MT7977 item 1 */
#define ADIE_CHAIN_24C                  0x24cu	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON, MT7977 item 1; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_24D                  0x24du	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON, MT7977 item 1; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_24E                  0x24eu	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON, MT7977 item 1; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_24F                  0x24fu	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON, MT7977 item 1; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_250                  0x250u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_251                  0x251u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_252                  0x252u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_253                  0x253u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_254                  0x254u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_255                  0x255u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_256                  0x256u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_257                  0x257u	/* RF_COMMON, RF_EPA_COMMON, 7976C RF_EPA_COMMON; RF_EPA_COMMON overwrites RF_COMMON */
#define ADIE_CHAIN_25C                  0x25cu	/* MT7977 item 1 */
#define ADIE_CHAIN_25D                  0x25du	/* MT7977 item 1 */
#define ADIE_CHAIN_25E                  0x25eu	/* MT7977 item 1 */
#define ADIE_CHAIN_25F                  0x25fu	/* MT7977 item 1 */
#define ADIE_CHAIN_260                  0x260u	/* MT7977 item 1 */
#define ADIE_CHAIN_261                  0x261u	/* MT7977 item 1 */
#define ADIE_CHAIN_26E                  0x26eu	/* MT7977 item 1 */
#define ADIE_CHAIN_26F                  0x26fu	/* MT7977 item 1 */
#define ADIE_CHAIN_270                  0x270u	/* MT7977 item 1 */
#define ADIE_CHAIN_271                  0x271u	/* MT7977 item 1 */
#define ADIE_CHAIN_272                  0x272u	/* MT7977 item 1 */
#define ADIE_CHAIN_273                  0x273u	/* MT7977 item 1 */
#define ADIE_CHAIN_274                  0x274u	/* MT7977 item 1 */
#define ADIE_CHAIN_275                  0x275u	/* MT7977 item 1 */
#define ADIE_CHAIN_287                  0x287u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_288                  0x288u	/* MT7977 item 1 */
#define ADIE_CHAIN_289                  0x289u	/* MT7977 item 1 */
#define ADIE_CHAIN_28A                  0x28au	/* MT7977 item 1 */
#define ADIE_CHAIN_28B                  0x28bu	/* MT7977 item 1 */
#define ADIE_CHAIN_28C                  0x28cu	/* MT7977 item 1 */
#define ADIE_CHAIN_28D                  0x28du	/* MT7977 item 1 */
#define ADIE_CHAIN_290                  0x290u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_291                  0x291u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_292                  0x292u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_293                  0x293u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_294                  0x294u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_295                  0x295u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_29E                  0x29eu	/* MT7977 item 1 */
#define ADIE_CHAIN_29F                  0x29fu	/* MT7977 item 1 */
#define ADIE_CHAIN_2A4                  0x2a4u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_2A5                  0x2a5u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_2A6                  0x2a6u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_2A7                  0x2a7u	/* MT7977 item 1 */
#define ADIE_CHAIN_2A8                  0x2a8u	/* MT7977 item 1 */
#define ADIE_CHAIN_2A9                  0x2a9u	/* MT7977 item 1 */
#define ADIE_CHAIN_2AA                  0x2aau	/* MT7977 item 1 */
#define ADIE_CHAIN_2AB                  0x2abu	/* MT7977 item 1 */
#define ADIE_CHAIN_2AC                  0x2acu	/* MT7977 item 1 */
#define ADIE_CHAIN_2AD                  0x2adu	/* MT7977 item 1 */
#define ADIE_CHAIN_2AE                  0x2aeu	/* MT7977 item 1 */
#define ADIE_CHAIN_2AF                  0x2afu	/* MT7977 item 1 */
#define ADIE_CHAIN_2B0                  0x2b0u	/* MT7977 item 1 */
#define ADIE_CHAIN_2B1                  0x2b1u	/* MT7977 item 1 */
#define ADIE_CHAIN_2B2                  0x2b2u	/* MT7977 item 1 */
#define ADIE_CHAIN_2B3                  0x2b3u	/* MT7977 item 1 */
#define ADIE_CHAIN_2B4                  0x2b4u	/* MT7977 item 1 */
#define ADIE_CHAIN_2BF                  0x2bfu	/* MT7977 item 1 */
#define ADIE_CHAIN_2C0                  0x2c0u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C1                  0x2c1u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C2                  0x2c2u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C3                  0x2c3u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C4                  0x2c4u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C5                  0x2c5u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C6                  0x2c6u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C7                  0x2c7u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C8                  0x2c8u	/* MT7977 item 1 */
#define ADIE_CHAIN_2C9                  0x2c9u	/* MT7977 item 1 */
#define ADIE_CHAIN_2CA                  0x2cau	/* MT7977 item 1 */
#define ADIE_CHAIN_2CB                  0x2cbu	/* MT7977 item 1 */
#define ADIE_CHAIN_2CC                  0x2ccu	/* MT7977 item 1 */
#define ADIE_CHAIN_2E9                  0x2e9u	/* MT7977 item 1 */
#define ADIE_CHAIN_2EA                  0x2eau	/* MT7977 item 1 */
#define ADIE_CHAIN_2EB                  0x2ebu	/* MT7977 item 1 */
#define ADIE_CHAIN_2EC                  0x2ecu	/* MT7977 item 1 */
#define ADIE_CHAIN_2ED                  0x2edu	/* MT7977 item 1 */
#define ADIE_CHAIN_2EE                  0x2eeu	/* MT7977 item 1 */
#define ADIE_CHAIN_2EF                  0x2efu	/* MT7977 item 1 */
#define ADIE_CHAIN_2F0                  0x2f0u	/* MT7977 item 1 */
#define ADIE_CHAIN_2F9                  0x2f9u	/* MT7977 item 1 */
#define ADIE_CHAIN_2FA                  0x2fau	/* MT7977 item 1 */
#define ADIE_CHAIN_2FB                  0x2fbu	/* MT7977 item 1 */
#define ADIE_CHAIN_2FC                  0x2fcu	/* MT7977 item 1 */
#define ADIE_CHAIN_2FD                  0x2fdu	/* MT7977 item 1 */
#define ADIE_CHAIN_2FE                  0x2feu	/* MT7977 item 1 */
#define ADIE_CHAIN_2FF                  0x2ffu	/* MT7977 item 1 */
#define ADIE_CHAIN_300                  0x300u	/* MT7977 item 1 */
#define ADIE_CHAIN_301                  0x301u	/* MT7977 item 1 */
#define ADIE_CHAIN_302                  0x302u	/* MT7977 item 1 */
#define ADIE_CHAIN_303                  0x303u	/* MT7977 item 1 */
#define ADIE_CHAIN_304                  0x304u	/* MT7977 item 1 */
#define ADIE_CHAIN_309                  0x309u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30A                  0x30au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30B                  0x30bu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30C                  0x30cu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30D                  0x30du	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30E                  0x30eu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_30F                  0x30fu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_310                  0x310u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_31D                  0x31du	/* MT7977 item 1 */
#define ADIE_CHAIN_31E                  0x31eu	/* MT7977 item 1 */
#define ADIE_CHAIN_31F                  0x31fu	/* MT7977 item 1 */
#define ADIE_CHAIN_320                  0x320u	/* MT7977 item 1 */
#define ADIE_CHAIN_321                  0x321u	/* MT7977 item 1 */
#define ADIE_CHAIN_322                  0x322u	/* MT7977 item 1 */
#define ADIE_CHAIN_323                  0x323u	/* MT7977 item 1 */
#define ADIE_CHAIN_324                  0x324u	/* MT7977 item 1 */
#define ADIE_CHAIN_325                  0x325u	/* MT7977 item 1 */
#define ADIE_CHAIN_326                  0x326u	/* MT7977 item 1 */
#define ADIE_CHAIN_327                  0x327u	/* MT7977 item 1 */
#define ADIE_CHAIN_328                  0x328u	/* MT7977 item 1 */
#define ADIE_CHAIN_329                  0x329u	/* MT7977 item 1 */
#define ADIE_CHAIN_32A                  0x32au	/* MT7977 item 1 */
#define ADIE_CHAIN_32B                  0x32bu	/* MT7977 item 1 */
#define ADIE_CHAIN_32C                  0x32cu	/* MT7977 item 1 */
#define ADIE_CHAIN_32D                  0x32du	/* MT7977 item 1 */
#define ADIE_CHAIN_32E                  0x32eu	/* MT7977 item 1 */
#define ADIE_CHAIN_32F                  0x32fu	/* MT7977 item 1 */
#define ADIE_CHAIN_330                  0x330u	/* MT7977 item 1 */
#define ADIE_CHAIN_331                  0x331u	/* MT7977 item 1 */
#define ADIE_CHAIN_332                  0x332u	/* MT7977 item 1 */
#define ADIE_CHAIN_333                  0x333u	/* MT7977 item 1 */
#define ADIE_CHAIN_334                  0x334u	/* MT7977 item 1 */
#define ADIE_CHAIN_335                  0x335u	/* MT7977 item 1 */
#define ADIE_CHAIN_336                  0x336u	/* MT7977 item 1 */
#define ADIE_CHAIN_337                  0x337u	/* MT7977 item 1 */
#define ADIE_CHAIN_338                  0x338u	/* MT7977 item 1 */
#define ADIE_CHAIN_339                  0x339u	/* MT7977 item 1 */
#define ADIE_CHAIN_33A                  0x33au	/* MT7977 item 1 */
#define ADIE_CHAIN_33B                  0x33bu	/* MT7977 item 1 */
#define ADIE_CHAIN_33C                  0x33cu	/* MT7977 item 1 */
#define ADIE_CHAIN_33D                  0x33du	/* MT7977 item 1 */
#define ADIE_CHAIN_33E                  0x33eu	/* MT7977 item 1 */
#define ADIE_CHAIN_33F                  0x33fu	/* MT7977 item 1 */
#define ADIE_CHAIN_340                  0x340u	/* MT7977 item 1 */
#define ADIE_CHAIN_341                  0x341u	/* MT7977 item 1 */
#define ADIE_CHAIN_342                  0x342u	/* MT7977 item 1 */
#define ADIE_CHAIN_343                  0x343u	/* MT7977 item 1 */
#define ADIE_CHAIN_344                  0x344u	/* MT7977 item 1 */
#define ADIE_CHAIN_345                  0x345u	/* MT7977 item 1 */
#define ADIE_CHAIN_346                  0x346u	/* MT7977 item 1 */
#define ADIE_CHAIN_353                  0x353u	/* MT7977 item 1 */
#define ADIE_CHAIN_354                  0x354u	/* MT7977 item 1 */
#define ADIE_CHAIN_355                  0x355u	/* MT7977 item 1 */
#define ADIE_CHAIN_356                  0x356u	/* MT7977 item 1 */
#define ADIE_CHAIN_357                  0x357u	/* MT7977 item 1 */
#define ADIE_CHAIN_358                  0x358u	/* MT7977 item 1 */
#define ADIE_CHAIN_359                  0x359u	/* MT7977 item 1 */
#define ADIE_CHAIN_35A                  0x35au	/* MT7977 item 1 */
#define ADIE_CHAIN_35B                  0x35bu	/* MT7977 item 1 */
#define ADIE_CHAIN_35C                  0x35cu	/* MT7977 item 1 */
#define ADIE_CHAIN_35D                  0x35du	/* MT7977 item 1 */
#define ADIE_CHAIN_35E                  0x35eu	/* MT7977 item 1 */
#define ADIE_CHAIN_35F                  0x35fu	/* MT7977 item 1 */
#define ADIE_CHAIN_360                  0x360u	/* MT7977 item 1 */
#define ADIE_CHAIN_361                  0x361u	/* MT7977 item 1 */
#define ADIE_CHAIN_364                  0x364u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_365                  0x365u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_366                  0x366u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_367                  0x367u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_36A                  0x36au	/* MT7977 item 1 */
#define ADIE_CHAIN_36B                  0x36bu	/* MT7977 item 1 */
#define ADIE_CHAIN_36C                  0x36cu	/* MT7977 item 1 */
#define ADIE_CHAIN_36D                  0x36du	/* MT7977 item 1 */
#define ADIE_CHAIN_36E                  0x36eu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_36F                  0x36fu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_370                  0x370u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_371                  0x371u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_372                  0x372u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_373                  0x373u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_374                  0x374u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_375                  0x375u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_376                  0x376u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_377                  0x377u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_378                  0x378u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_379                  0x379u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_37A                  0x37au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_37B                  0x37bu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_37C                  0x37cu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_485                  0x485u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_4A5                  0x4a5u	/* MT7977 item 1 */
#define ADIE_CHAIN_4A6                  0x4a6u	/* MT7977 item 1 */
#define ADIE_CHAIN_4A7                  0x4a7u	/* MT7977 item 1 */
#define ADIE_CHAIN_4A8                  0x4a8u	/* MT7977 item 1 */
#define ADIE_CHAIN_637                  0x637u	/* MT7977 RF_INIT; also MT7977 item 1 (its trailer, V:e00520f6) */
#define ADIE_CHAIN_638                  0x638u	/* MT7977 RF_INIT; also MT7977 item 1 (its trailer, V:e00520f6) */
#define ADIE_CHAIN_63A                  0x63au	/* MT7977 item 1 */
#define ADIE_CHAIN_63C                  0x63cu	/* MT7977 item 1 */
#define ADIE_CHAIN_63D                  0x63du	/* MT7977 item 1 */
#define ADIE_CHAIN_63F                  0x63fu	/* MT7977 item 1 */
#define ADIE_CHAIN_640                  0x640u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_641                  0x641u	/* MT7976 RF_INIT */
#define ADIE_CHAIN_642                  0x642u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_643                  0x643u	/* RF_COMMON */
#define ADIE_CHAIN_644                  0x644u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_645                  0x645u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_646                  0x646u	/* RF_COMMON */
#define ADIE_CHAIN_647                  0x647u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_648                  0x648u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_649                  0x649u	/* RF_COMMON */
#define ADIE_CHAIN_64A                  0x64au	/* RF_COMMON */
#define ADIE_CHAIN_64B                  0x64bu	/* RF_COMMON */
#define ADIE_CHAIN_64D                  0x64du	/* MT7977 item 1 */
#define ADIE_CHAIN_64F                  0x64fu	/* MT7977 item 1 */
#define ADIE_CHAIN_650                  0x650u	/* MT7977 item 1 */
#define ADIE_CHAIN_652                  0x652u	/* MT7977 item 1 */
#define ADIE_CHAIN_654                  0x654u	/* MT7977 item 1 */
#define ADIE_CHAIN_655                  0x655u	/* MT7977 item 1 */
#define ADIE_CHAIN_657                  0x657u	/* MT7977 item 1 */
#define ADIE_CHAIN_658                  0x658u	/* MT7977 item 1 */
#define ADIE_CHAIN_667                  0x667u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_668                  0x668u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_66D                  0x66du	/* MT7977 item 1 */
#define ADIE_CHAIN_66E                  0x66eu	/* MT7977 item 1 */
#define ADIE_CHAIN_66F                  0x66fu	/* MT7977 item 1 */
#define ADIE_CHAIN_670                  0x670u	/* MT7977 item 1 */
#define ADIE_CHAIN_671                  0x671u	/* MT7977 item 1 */
#define ADIE_CHAIN_672                  0x672u	/* MT7977 item 1 */
#define ADIE_CHAIN_673                  0x673u	/* MT7977 item 1 */
#define ADIE_CHAIN_674                  0x674u	/* MT7977 item 1 */
#define ADIE_CHAIN_675                  0x675u	/* MT7977 item 1 */
#define ADIE_CHAIN_676                  0x676u	/* MT7977 item 1 */
#define ADIE_CHAIN_677                  0x677u	/* MT7977 item 1 */
#define ADIE_CHAIN_678                  0x678u	/* MT7977 item 1 */
#define ADIE_CHAIN_679                  0x679u	/* MT7977 item 1 */
#define ADIE_CHAIN_67A                  0x67au	/* MT7977 item 1 */
#define ADIE_CHAIN_67B                  0x67bu	/* MT7977 item 1 */
#define ADIE_CHAIN_67C                  0x67cu	/* MT7977 item 1 */
#define ADIE_CHAIN_67D                  0x67du	/* MT7977 item 1 */
#define ADIE_CHAIN_67E                  0x67eu	/* MT7977 item 1 */
#define ADIE_CHAIN_67F                  0x67fu	/* MT7977 item 1 */
#define ADIE_CHAIN_680                  0x680u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_681                  0x681u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_682                  0x682u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_683                  0x683u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_684                  0x684u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_685                  0x685u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_686                  0x686u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_687                  0x687u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_688                  0x688u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_689                  0x689u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68A                  0x68au	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68B                  0x68bu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68C                  0x68cu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68D                  0x68du	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68E                  0x68eu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_68F                  0x68fu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_690                  0x690u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_691                  0x691u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_692                  0x692u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_693                  0x693u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_694                  0x694u	/* MT7976 RF_INIT, MT7977 item 1 */
#define ADIE_CHAIN_695                  0x695u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_696                  0x696u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_697                  0x697u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_698                  0x698u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_699                  0x699u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69A                  0x69au	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69B                  0x69bu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69C                  0x69cu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69D                  0x69du	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69E                  0x69eu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_69F                  0x69fu	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_6A0                  0x6a0u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_6A1                  0x6a1u	/* RF_COMMON, MT7977 item 1 */
#define ADIE_CHAIN_6A2                  0x6a2u	/* RF_COMMON */
#define ADIE_CHAIN_6A3                  0x6a3u	/* RF_COMMON */
#define ADIE_CHAIN_6A4                  0x6a4u	/* RF_COMMON */
#define ADIE_CHAIN_6BB                  0x6bbu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_6C1                  0x6c1u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_6C4                  0x6c4u	/* MT7977 item 1 */
#define ADIE_CHAIN_6C5                  0x6c5u	/* MT7977 item 1 */
#define ADIE_CHAIN_6C6                  0x6c6u	/* MT7977 item 1 */
#define ADIE_CHAIN_6C7                  0x6c7u	/* MT7977 item 1 */
#define ADIE_CHAIN_6C8                  0x6c8u	/* MT7977 item 1 */
#define ADIE_CHAIN_6C9                  0x6c9u	/* MT7977 item 1 */
#define ADIE_CHAIN_6CA                  0x6cau	/* MT7977 item 1 */
#define ADIE_CHAIN_6CB                  0x6cbu	/* MT7977 item 1 */
#define ADIE_CHAIN_6CC                  0x6ccu	/* MT7977 item 1 */
#define ADIE_CHAIN_6CD                  0x6cdu	/* MT7977 item 1 */
#define ADIE_CHAIN_6CE                  0x6ceu	/* MT7977 item 1 */
#define ADIE_CHAIN_6CF                  0x6cfu	/* MT7977 item 1 */
#define ADIE_CHAIN_6D0                  0x6d0u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D1                  0x6d1u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D2                  0x6d2u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D3                  0x6d3u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D4                  0x6d4u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D5                  0x6d5u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D6                  0x6d6u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D7                  0x6d7u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D8                  0x6d8u	/* MT7977 item 1 */
#define ADIE_CHAIN_6D9                  0x6d9u	/* MT7977 item 1 */
#define ADIE_CHAIN_6DA                  0x6dau	/* MT7977 item 1 */
#define ADIE_CHAIN_6DB                  0x6dbu	/* MT7977 item 1 */
#define ADIE_CHAIN_6DC                  0x6dcu	/* MT7977 item 1 */
#define ADIE_CHAIN_6DD                  0x6ddu	/* MT7977 item 1 */
#define ADIE_CHAIN_6DE                  0x6deu	/* MT7977 item 1 */
#define ADIE_CHAIN_6DF                  0x6dfu	/* MT7977 item 1 */
#define ADIE_CHAIN_6E0                  0x6e0u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E1                  0x6e1u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E2                  0x6e2u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E3                  0x6e3u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E4                  0x6e4u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E5                  0x6e5u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E6                  0x6e6u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E7                  0x6e7u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E8                  0x6e8u	/* MT7977 item 1 */
#define ADIE_CHAIN_6E9                  0x6e9u	/* MT7977 item 1 */
#define ADIE_CHAIN_6EA                  0x6eau	/* MT7977 item 1 */
#define ADIE_CHAIN_6EB                  0x6ebu	/* MT7977 item 1 */
#define ADIE_CHAIN_6EC                  0x6ecu	/* MT7977 item 1 */
#define ADIE_CHAIN_6ED                  0x6edu	/* MT7977 item 1 */
#define ADIE_CHAIN_6EE                  0x6eeu	/* MT7977 item 1 */
#define ADIE_CHAIN_6EF                  0x6efu	/* MT7977 item 1 */
#define ADIE_CHAIN_6F0                  0x6f0u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F1                  0x6f1u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F2                  0x6f2u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F3                  0x6f3u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F4                  0x6f4u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F5                  0x6f5u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F6                  0x6f6u	/* MT7977 item 1 */
#define ADIE_CHAIN_6F7                  0x6f7u	/* MT7977 item 1 */
#define ADIE_CHAIN_71E                  0x71eu	/* MT7977 item 1 */
#define ADIE_CHAIN_71F                  0x71fu	/* MT7977 item 1 */
#define ADIE_CHAIN_722                  0x722u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_724                  0x724u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_725                  0x725u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_727                  0x727u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_728                  0x728u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_72A                  0x72au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_72B                  0x72bu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_72D                  0x72du	/* MT7977 RF_INIT */
#define ADIE_CHAIN_72F                  0x72fu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_730                  0x730u	/* MT7977 item 1 */
#define ADIE_CHAIN_731                  0x731u	/* MT7977 item 1 */
#define ADIE_CHAIN_734                  0x734u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_735                  0x735u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_737                  0x737u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_738                  0x738u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_73A                  0x73au	/* MT7977 RF_INIT */
#define ADIE_CHAIN_73B                  0x73bu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_73D                  0x73du	/* MT7977 RF_INIT */
#define ADIE_CHAIN_73E                  0x73eu	/* MT7977 item 1 */
#define ADIE_CHAIN_73F                  0x73fu	/* MT7977 item 1 */
#define ADIE_CHAIN_748                  0x748u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_749                  0x749u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_74C                  0x74cu	/* MT7977 RF_INIT */
#define ADIE_CHAIN_750                  0x750u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_751                  0x751u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_950                  0x950u	/* MT7977 RF_INIT */
#define ADIE_CHAIN_E52                  0xe52u	/* MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E53                  0xe53u	/* MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E54                  0xe54u	/* MT7976 RF_INIT, RF_COMMON, MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E55                  0xe55u	/* MT7976 RF_INIT, RF_COMMON, MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E56                  0xe56u	/* MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E57                  0xe57u	/* MT7977 RF_INIT; beside ADIE_RCTRIM; the MT7977 RF_INIT writes 0x0c000c00 */
#define ADIE_CHAIN_E70                  0xe70u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E71                  0xe71u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E72                  0xe72u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E73                  0xe73u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E74                  0xe74u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E75                  0xe75u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E76                  0xe76u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E77                  0xe77u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E78                  0xe78u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E79                  0xe79u	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7A                  0xe7au	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7B                  0xe7bu	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7C                  0xe7cu	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7D                  0xe7du	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7E                  0xe7eu	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E7F                  0xe7fu	/* RF_COMMON; gain table words (RX analog, above) */
#define ADIE_CHAIN_E91                  0xe91u	/* MT7976 RF_INIT, MT7977 RF_INIT */
#define ADIE_CHAIN_E92                  0xe92u	/* MT7976 RF_INIT, MT7977 RF_INIT */
#define ADIE_CHAIN_E93                  0xe93u	/* MT7977 RF_INIT */

#endif /* HW_ADIE_ADIE_H */
