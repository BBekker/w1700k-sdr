/* SPDX-License-Identifier: MIT */
/*
 * MT7996 MAC side as MediaTek's WM touches it in its idle loop, host command
 * path, PHY-init routine and channel set: the UMAC PLE / PSE queue
 * managers, the RRO counters, the packet processor (PP), the MLO block and
 * per-band LMAC words. The receive path of this project uses none of them;
 * they are listed so the stock WM can be read and so an own WM knows what
 * it leaves out. The LMAC ARB_SCR word is in phy_cal.h
 * (MT7996_LMAC_ARB_SCR), the WFSYS_ON low-power block and the bus sleep
 * protection in conn.h, the ECC engine the UMAC ISR polls in secure.h.
 *
 * ADDRESS SPACE: WF bus as the WM sees it (0x820c0000..0x820cffff UMAC,
 * 0x820d1000 MLO, 0x820e0000 / 0x820f0000 / 0x830e0000 LMAC band 0 / 1 / 2).
 * The host's fixed BAR map covers most of them (host.h), but these are WM
 * accesses only: HAZARD: host reads in this range are quarantined (host.h
 * "HOST HARD LOCK"; a host read of PSE 0x820c8000 hard-locked the host and
 * wedged the device). Use REG_ACCESS (mcu_dic.h) or WM-side code.
 *
 * Name sources: MediaTek coda headers (vendor SDK
 * mt_wifi/include/chip/mt7990_coda; MT7990 is this part's firmware name),
 * MediaTek's GCI field enumerators (SDK ROM DWARF, decoded through the
 * retail ROM's own field tables, conn.h "GCI"), ROM function names. Names
 * marked [guess] are descriptive.
 *
 * WM main-task loop of MediaTek's stock WM, decoded against its cold load
 * (the common phase outside the calibration calls, regenerated record for
 * record; phy_cal.h "Calibration init"). Per OS message: [static][measured]
 *   wake V:00917fb8: the MT7996_PLE_LOG_0 check (below); per band 0..2 the
 *     WFSYS_ON low-power leave and MT7996_MLO_SCR0 LINK_ACTIVE (conn.h
 *     "Leaving low power"); the release of the bus sleep protection (conn.h
 *     MT7996_WF_TOP_ON_SLPPROT_*);
 *   at most one event: LMAC WF_INT (V:e009697c), the MAC hang check
 *     (V:e0098692), the RRO counters (V:e0123346) or the 500 ms monitor
 *     timer (V:e011c020, phy.h MT7996_PHY_WFCOMM_D750);
 *   three passes of the UMAC N9 ISR V:e0096696: PLE / PSE status, the ECC
 *     engine (secure.h), the write-1-to-clear of the PLE N9 status words;
 *   idle V:00917bfa: 17 GCI read-modify-writes clearing PSE
 *     QUEUE_EMPTY_MASK.
 * Host commands of the stock load before the calibration (mcu_cmd.h):
 * MCU_UNI_CMD_WSYS_CONFIG tag 3 FW_BASIC_CONFIG (PP checksum offload),
 * MCU_UNI_CMD_CHIP_CONFIG NIC capability (PLE / PSE / PP resources), then
 * MCU_UNI_CMD_EFUSE_CTRL in buffer mode, which runs the PHY-init routine
 * and the calibration (phy_cal.h). A receive-only firmware that never
 * sleeps needs none of this [inferred].
 *
 * Confidence tags: [measured] read in MediaTek's stock cold load, [static]
 * from WM / ROM code or tables, [inferred].
 */
#ifndef HW_MT7996_MAC_H
#define HW_MT7996_MAC_H

#include "../hw_bits.h"

/* ========================================================================
 * UMAC PLE (WF_PLE_TOP, 0x820c0000) and PSE (WF_PSE_TOP, 0x820c8000)
 * ======================================================================== */

#define MT7996_PLE_BASE                   0x820c0000u
#define MT7996_PSE_BASE                   0x820c8000u

/* PLE PG_HIF_GROUP: HIF page quota. The WM reports MIN_QUOTA - 0x10 to the
 * host as the PLE TX resource (UNI CHIP_CONFIG NIC capability, tag
 * CAP_TX_RESOURCE, V:e00b63f8). Stock 0x1da81da8. [static][measured] */
#define MT7996_PLE_PG_HIF_GROUP           0x820c000cu
#define   PLE_PG_HIF_GROUP_MIN_QUOTA      HW_GENMASK(12, 0)
#define   PLE_PG_HIF_GROUP_MAX_QUOTA      HW_GENMASK(28, 16)

/* PLE interrupt enables towards the N9 (the WM). Stock EN_MASK 0x0002000f
 * (CPU_Q0..3_NE and SPL_CMD_FIFO_FULL), ERR_MASK 0xfcfffdef, ERR_MASK_1
 * 0x0033087f, BSS_PS mask 0. [measured] */
#define MT7996_PLE_INT_N9_EN_MASK         0x820c0080u
#define   PLE_INT_N9_CPU_QN_NE(n)         HW_BIT(n)	/* n = 0..3 */
#define   PLE_INT_N9_DATA_TOGGLE          HW_BIT(16)
#define   PLE_INT_N9_SPL_CMD_FIFO_FULL    HW_BIT(17)
#define   PLE_INT_N9_AC_ENQ_LMAC          HW_BIT(22)
#define MT7996_PLE_INT_N9_ERR_MASK        0x820c0084u
#define MT7996_PLE_INT_N9_ERR_MASK_1      0x820c0088u
#define MT7996_PLE_N9_BSS_PS_INT_MASK     0x820c008cu
/* PLE N9 status words, the bit layout of the masks. W1C: the WM's UMAC ISR
 * body (V:e0096696) writes 0xffffffff to INT_N9_STS and N9_BSS_PS_INT_STS at
 * the end of every pass (three passes per main-task message, V:e009686c /
 * V:e0096870); they read 0 afterwards. [static][measured] */
#define MT7996_PLE_INT_N9_STS             0x820c0300u
#define MT7996_PLE_INT_N9_ERR_STS         0x820c0304u
#define MT7996_PLE_INT_N9_ERR_STS_1       0x820c0308u
#define MT7996_PLE_N9_BSS_PS_INT_STS      0x820c030cu
/* PLE QUEUE_EMPTY: per-queue empty flags, RO. Stock idle 0xf133ffff. The WM
 * checks CPU_Q2_EMPTY every pass (V:e0097150(2) -> V:e00d13f6) and moves
 * frames only when queue 2 is not empty. [static][measured] */
#define MT7996_PLE_QUEUE_EMPTY            0x820c0360u
#define   PLE_QUEUE_EMPTY_CPU_QN(n)       HW_BIT(n)	/* n = 0..3 */
#define   PLE_QUEUE_EMPTY_ALL_AC          HW_BIT(24)
/* PLE_LOG_0: a software word. [0] = the "UMAC LP reset bit" (ROM
 * halGetUmacLpResetBit returns ~[0] & 1, halSetUmacLpResetBit writes it).
 * The WM's wake path (V:00917fb8 -> V:e00ecdba) reads it on every message;
 * when [0] = 0 it calls V:e00ecdf8 (a stub returning 0 in the test-mode WM)
 * and sets [0] = 1 again. Stock 0x00000001. [static][measured] */
#define MT7996_PLE_LOG_0                  0x820c03b8u
#define   PLE_LOG_0_UMAC_LP_RESET_DONE    HW_BIT(0)	/* [guess] name */
#define MT7996_PLE_AMSDU_INT_N9_ERR_MASK  0x820c100cu	/* stock 0x00031011 [measured] */
#define MT7996_PLE_AMSDU_INT_N9_ERR_STS   0x820c1010u

/* PSE interrupt enables / status towards the N9. Stock EN_MASK 0xf
 * (CPU_Q0..3_NE), ERR_MASK 0x7f7f797f, ERR1_MASK 0x1f7f. INT_N9_STS is W1C
 * (the ISR writes 1 << n for CPU_Qn, n = 0..3, and 0xfff80f00 for the HIF /
 * LMAC group, V:e009674a..V:e00967b8). [static][measured] */
#define MT7996_PSE_INT_N9_EN_MASK         0x820c8008u
#define MT7996_PSE_INT_N9_ERR_MASK        0x820c8024u
#define MT7996_PSE_INT_N9_ERR1_MASK       0x820c8028u
#define MT7996_PSE_INT_N9_STS             0x820c8030u
#define   PSE_INT_N9_CPU_QN_NE(n)         HW_BIT(n)	/* n = 0..4 */
#define   PSE_INT_N9_LMAC_ENQ             HW_BIT(12)
#define   PSE_INT_N9_HIF_QN_NE(n)         HW_BIT(19 + (n))	/* n = 0..12 */
#define MT7996_PSE_INT_N9_ERR_STS         0x820c8034u
#define MT7996_PSE_INT_N9_ERR1_STS        0x820c8038u
/* PSE QUEUE_EMPTY_MASK: per-HIF-queue empty-interrupt masks. The retail
 * ROM's field table (0x0083ceec, GCI group 0xe) places HIF_0..7 at [15:8],
 * HIF_8..10 at [24:22] and maps HIF_11..16 onto [0]; the coda says HIF_n =
 * [8 + n]. The idle path (V:00917bfa -> V:e00ee988(0)) writes every field 0
 * with 17 separate read-modify-writes. Stock 0. The WM's low-power hook
 * between host commands (V:e00b5b4a -> V:e00b5ad0 -> V:e00ee988(0), about
 * 200 times per band phase of the stock load) runs the same 17 writes,
 * then stores cos_get_systime in WM 0xe01ec028. [static][measured] */
#define MT7996_PSE_QUEUE_EMPTY_MASK       0x820c80b4u
#define   PSE_QUEUE_EMPTY_MASK_HIF0_7     HW_GENMASK(15, 8)
#define   PSE_QUEUE_EMPTY_MASK_HIF8_10    HW_GENMASK(24, 22)
/* PSE PG_HIF0_GROUP: HIF0 page quota (stock 0x1fff00b0), reported as the PSE
 * TX resource. [static][measured] */
#define MT7996_PSE_PG_HIF0_GROUP          0x820c8110u
#define   PSE_PG_HIF0_MIN_QUOTA           HW_GENMASK(12, 0)
#define   PSE_PG_HIF0_MAX_QUOTA           HW_GENMASK(28, 16)

/* RRO (WF_RRO_TOP, 0x820c2000) +0x190 / +0x1a4 / +0x1a8 / +0x1ac: four
 * counters the WM's MURU timer 8 (V:e0123346) reads every period. Stock 0.
 * Names [guess]. [static][measured] */
#define MT7996_RRO_CNT0                   0x820c2190u
#define MT7996_RRO_CNT1                   0x820c21a4u
#define MT7996_RRO_CNT2                   0x820c21a8u
#define MT7996_RRO_CNT3                   0x820c21acu

/* ========================================================================
 * Packet processor (WF_PP_TOP / MT_MDP, 0x820cc000)
 * ======================================================================== */

/* +0x00: [15:8] TXP "add DDW" (ROM halPktProcessorGetTxpAddDDW(1)); the WM
 * reports it x 8 (bytes) in CAP_TX_RESOURCE. Stock 0x0003026c.
 * [static][measured] */
#define MT7996_PP_TXP_CFG                 0x820cc000u	/* [guess] name */
#define   PP_TXP_ADD_DDW1                 HW_GENMASK(15, 8)
#define   PP_TXP_ADD_DDW0                 HW_GENMASK(7, 0)
/* +0x40: checksum offload control, set by UNI WSYS_CONFIG tag 3
 * FW_BASIC_CONFIG (u2RxChecksum bit 0 IP, bit 1 UDP, bit 2 TCP;
 * u2TxChecksum): ROM halPktProcessorEnableCso and V:e00d14f2. Stock
 * 0x000f0003. [static][measured] */
#define MT7996_PP_CSO_CTRL                0x820cc040u	/* [guess] name */
#define   PP_CSO_TX_EN                    HW_BIT(0)	/* tx[2:0] != 0 */
#define   PP_CSO_TX_B4                    HW_BIT(1)	/* tx bit 4 */
#define   PP_CSO_TX_B5                    HW_BIT(2)	/* tx bit 5 */
#define   PP_CSO_RX_IP                    HW_GENMASK(17, 16)	/* rx bit 0 */
#define   PP_CSO_RX_TCP                   HW_BIT(18)	/* rx bit 2 */
#define   PP_CSO_RX_UDP                   HW_BIT(19)	/* rx bit 1 */

/* ========================================================================
 * MLO (GCI module 0x3d, 0x820d1000)
 * ======================================================================== */

/* SCR0. The WM's wake path sets LINK_ACTIVE(b) for every band whose WFSYS_ON
 * low power it leaves (V:e00e5454); the sleep path clears it. Stock
 * 0x07010740. [static][measured] */
#define MT7996_MLO_SCR0                   0x820d1000u
#define   MLO_SCR0_EMLSR_USR_RMV_EN       HW_BIT(0)
#define   MLO_SCR0_EMLMR_USR_RMV_EN       HW_BIT(1)
#define   MLO_SCR0_NSTR_USR_RMV_EN        HW_BIT(2)
#define   MLO_SCR0_EMLSR_VLD_CHK_EN       HW_BIT(4)
#define   MLO_SCR0_EMLMR_VLD_CHK_EN       HW_BIT(5)
#define   MLO_SCR0_NSTR_VLD_CHK_EN        HW_GENMASK(7, 6)
#define   MLO_SCR0_STR_BITMAP(n)          HW_BIT(8 + (n))
#define   MLO_SCR0_SYNC_CH_ACCESS_MECH    HW_BIT(12)
#define   MLO_SCR0_SYNC_CH_ACCESS_EN      HW_BIT(13)
#define   MLO_SCR0_BA_SESSION_CHK_EN      HW_BIT(16)
#define   MLO_SCR0_LINK_ACTIVE(b)         HW_BIT(24 + (b))
#define   MLO_SCR0_LINK_MLO_EN(b)         HW_BIT(28 + (b))

/* ========================================================================
 * LMAC, per band (BN0 0x820e0000, BN1 0x820f0000, BN2 0x830e0000). ARB_SCR
 * (+0x3000): phy_cal.h MT7996_LMAC_ARB_SCR.
 * ======================================================================== */

#define MT7996_LMAC_BASE(b)               ((b) == 2 ? 0x830e0000u : (b) == 1 ? 0x820f0000u : 0x820e0000u)
/* WF_CFG debug: the WM's MAC hang check (MURU timer 9, V:e0098692) saves
 * +0x98 / +0x9c / +0xa0, sets them to 0x1f / 0x05050505 / 0x78787777, reads
 * +0x104 [5:4] (1 = stuck), restores. Stock all 0. Names [guess].
 * [static][measured] */
#define MT7996_WF_CFG_DBG_SEL0(b)         (MT7996_LMAC_BASE(b) + 0x098u)
#define MT7996_WF_CFG_DBG_SEL1(b)         (MT7996_LMAC_BASE(b) + 0x09cu)
#define MT7996_WF_CFG_DBG_SEL2(b)         (MT7996_LMAC_BASE(b) + 0x0a0u)
#define MT7996_WF_CFG_DBG_FLAG(b)         (MT7996_LMAC_BASE(b) + 0x104u)
#define   WF_CFG_DBG_FLAG_STATE           HW_GENMASK(5, 4)
/* AGG state-machine debug (coda BN0_WF_AGG_TOP SMDCR / SMCR0..2), in the
 * same hang check: SMDCR = 0x03020100 selects the state machines; SMCR0 /
 * SMCR1 [3:0] == 4 in two consecutive reads = stuck. [static][measured] */
#define MT7996_AGG_SMDCR(b)               (MT7996_LMAC_BASE(b) + 0x22e8u)
#define MT7996_AGG_SMCR(b, n)             (MT7996_LMAC_BASE(b) + 0x22f0u + 4u * (n))	/* n = 0..2 */
/* TMAC frame power limits (coda BN0_WF_TMAC_TOP MAX_FPCR .. MIN_FPCR,
 * +0x42e4..+0x4344, 25 words: MAX_FPCR, the VHT / HE / EHT / SRU FP0CRn
 * words, OFST and MIN_FPCR), written by the PHY-init routine's per-band
 * table from ROM table 0x0083b6a0 ({address, value} pairs, band 2 + 0x01000000,
 * else + band << 16) by halTmacInitMaxTxPwr(band): 0x7f per rate byte
 * (maximum), OFST 0, MIN 0x80. TX only. [static][measured] */
#define MT7996_TMAC_MAX_FPCR(b)           (MT7996_LMAC_BASE(b) + 0x42e4u)
#define MT7996_TMAC_MIN_FPCR(b)           (MT7996_LMAC_BASE(b) + 0x4344u)
#define MT7996_TMAC_FPCR_WORDS            25u
/* WF_INT status / enable words idx 0..10 (ROM halIntGetInterruptSts /
 * halIntGetInterruptEnableSts; the WM's LMAC interrupt task V:e009697c):
 * BN WF_INT at LMAC + 0xc000 (0x820ec000 / 0x820fc000 / 0x830ec000), the
 * enables at +0x60. Stock band-0 enables: idx 6 = 0x800, idx 7 =
 * 0x00044101. [static][measured] */
#define MT7996_WF_INT_STS(b, i)           (MT7996_LMAC_BASE(b) + 0xc000u + 4u * (i))	/* i = 0..10 */
#define MT7996_WF_INT_EN(b, i)            (MT7996_WF_INT_STS(b, i) + 0x60u)
/* Channel set (UNI 0x34; phy_cal.h "Channel set"), the LMAC words with ROM
 * CR-map names: RMAC SMPS_CTRL (both tags; tag 0 V:e00b1006; 0xff0f0d32
 * unchanged); TMAC TFCR0 (V:e00f07e2; 0x1280ffe0 unchanged); V:e00e5c9a:
 * RATE BWCR = 0x80 / 0x8c / 0x90 at BW20 / BW160 / BW320 (from 0x80;
 * [4:2] = the bandwidth code [inferred: from the three values]), the
 * WFSYS_ON TICKCR0 words (conn.h), RMAC CHFREQ = the control channel
 * number as written: 7, 0x24, 0xb5 for 2.4 GHz ch 7, 5 GHz ch 36, 6 GHz
 * ch 1 (from 0); band 2 also TMAC TCR0 (0xc0000010 unchanged) and
 * BN_MODE (phy.h, unchanged). The receive path of this project uses none
 * of them. [static][measured] */
#define MT7996_TMAC_TCR0(b)               (MT7996_LMAC_BASE(b) + 0x4000u)
/* TCR0 [21] BW320M_LOC (CR map ENUM_WFTMAC_CONFIG_TCR0): at BW320 = 1 for the second
 * 320 MHz channelization (phy_chan.h MT7996_CHSET_320_2 centres), written twice per channel
 * set: V:e00e5c9a and V:e00f080a (both through V:e00d09ba). Stock band 2 0 (0xc0000010
 * unchanged). [static][measured] */
#define   TMAC_TCR0_BW320M_LOC            HW_BIT(21)
#define MT7996_TMAC_TFCR0(b)              (MT7996_LMAC_BASE(b) + 0x40e8u)
/* TFCR0 [19:16] PRIM20M_CH (CR map ENUM_WFTMAC_CONFIG_TFCR0): = the channel record's [0x10],
 * the index of the primary 20 MHz channel in the bandwidth (V:e00e55b0), written by
 * V:e00f07e2. Stock 0 (0x1280ffe0 unchanged). [static][measured] */
#define   TMAC_TFCR0_PRIM20M_CH           HW_GENMASK(19, 16)
#define MT7996_RMAC_CHFREQ(b)             (MT7996_LMAC_BASE(b) + 0x5090u)
/* CHFREQ [7:0] ChFREQ (CR map ENUM_RMAC_CONFIG_CHFREQ; [31:8] A3_COMP_PAT_B47_24 kept): the
 * control channel as the channel set numbers it (phy_chan.h MT7996_CHSET_CHFREQ_*).
 * [static][measured] */
#define   RMAC_CHFREQ_CH                  HW_GENMASK(7, 0)
#define MT7996_RMAC_SMPS_CTRL(b)          (MT7996_LMAC_BASE(b) + 0x5638u)
/* RMAC SMPS_CTRL fields (ROM CR map ENUM_RMAC_CONFIG_SMPS_CTRL, GCI module 0x17 + band, group
 * 108). The channel switch's first step (V:e00b1006, both UNI 0x34 tags) writes SMPS_MODE and
 * SMPS_BY_RTS_ONLY in one GCI transaction (V:e01240a2: one read, one write), then RX_STREAM =
 * the chain mask of the host's rx_path count, bits 0..n-1 (V:e00b1006's loop, V:e00edd0c ->
 * ROM V:0081d9fe); V:e0123c70 reads RX_STREAM back (ROM V:0081da36) and counts its chains.
 * The whole step is gated by the ROM data byte 0x0222b43a (ROM V:008231cc through slot
 * 0x0081b5f8; stock 1, the chanset decode's [FIT]). Stock 0xff0f0d32 unchanged on every band.
 * [static][measured] */
#define   RMAC_SMPS_CTRL_SMPS_RX_STREAM   HW_GENMASK(31, 24)
#define   RMAC_SMPS_CTRL_RX_STREAM        HW_GENMASK(23, 16)
#define   RMAC_SMPS_CTRL_BY_RTS_ONLY      HW_BIT(11)	/* SMPS_BY_RTS_ONLY */
#define   RMAC_SMPS_CTRL_MODE             HW_GENMASK(5, 4)	/* SMPS_MODE */
#define MT7996_RATE_BWCR(b)               (MT7996_LMAC_BASE(b) + 0xe008u)
#define   RATE_BWCR_BW                    HW_GENMASK(4, 2)	/* [inferred] */

#endif /* HW_MT7996_MAC_H */
