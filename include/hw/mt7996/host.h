/* SPDX-License-Identifier: MIT */
/*
 * MT7996 PCIe host interface: BAR0 layout, address translation windows, host
 * ownership (LPCR), firmware state and other host-visible status.
 *
 * ADDRESS SPACES used in this file:
 *   BAR      offset into PCIe BAR0 (2 MiB memory BAR) of function 14c3:7990.
 *   bus      a device bus address (CONN_INFRA 0x7cxxxxxx / 0x18xxxxxx, CBTOP
 *            0x70xxxxxx-0x77xxxxxx, WF 0x8xxxxxxx, MCU SYSRAM 0x004xxxxx ...),
 *            reached from the host through the fixed map or a remap window.
 * CONN_INFRA is 0x7c000000 in the MCU's view; the host-side L1 window needs
 * the same block expressed as 0x18000000 (subtract 0x7c000000, add
 * 0x18000000). The 0x7c -> 0x18 conversion is a host-side rule only. [measured]
 *
 * Host address resolution (first rule that matches):
 *   1. address < 0x100000            -> BAR offset, used directly.
 *   2. fixed map (table below)       -> BAR = map.bar + (address - map.bus);
 *      entries are searched IN ORDER and overlap (first match wins).
 *   3. 0x18000000..0x18bfffff        -> L1 window, unchanged
 *      0x7c000000..0x7c3fffff        -> L1 window, address - 0x7c000000 + 0x18000000
 *      0x70000000..0x77ffffff,
 *      0xf0000000..0xffffffff        -> L1 window, unchanged
 *   4. anything else                 -> L2 window.
 * Each window access is: lock -> RMW selector -> READ BACK selector (flushes
 * the posted write) -> access the window -> unlock. Bulk copies must be split
 * at 4 KiB (L2), 64 KiB (L1) and fixed-entry boundaries; the reference bulk
 * reader translates only the start address.                [static, measured]
 *
 * HAZARD: 0xdead2117 POISON. The WiFi-die side of the BAR can latch into a
 *         state where every access through the WiFi-die fixed windows (BAR
 *         0x40000, 0x54000, 0x70000, 0x80000, 0xb0000, 0xc0000) and the WHOLE
 *         L2 window returns 0xdead2117, independent of target (the same
 *         register reads 0 directly and poison through L2). The L1 window and
 *         host-side BAR 0x10000, 0xd0000, 0xd4000, 0xe0000, 0xe0010 and
 *         0xe00f0 stay healthy, and firmware download, the MCU mailbox and RX DMA keep
 *         working. Not cleared by WF subsystem reset, module reload, FLR,
 *         secondary bus reset, remove/rescan, PERST# or SoC reboot; a device
 *         power cycle clears it. Registers behind the poisoned windows remain
 *         reachable through the firmware register proxy (UNI REG_ACCESS,
 *         mcu_cmd.h). Never feed a poisoned word into a read-modify-write
 *         (it would write the sentinel back).                       [measured]
 *         Triggers seen: MCU-side execution of the ROM WFSYS_ON power-on
 *         sequence; a host read of the shared pool (bus 0xe0400000) after a
 *         ring test (with PCIe completion timeouts and an unrecoverable AER
 *         report); WM+WA concurrent pool packing; the BAR window after a
 *         WM-raised WA soft interrupt.                              [measured]
 *
 * HAZARD: HOST HARD LOCK + DEVICE WEDGE. A host READ of an unclocked WFSYS /
 *         UMAC domain hard-locks the ARM host (no panic path runs) and leaves
 *         the MT7996 wedged past host reboot, FLR, secondary bus reset and
 *         PERST#; config space and AER look clean; only a device power cycle
 *         recovers. Attributed: a 4 KiB read at bus 0x820c8000 (PSE, BAR
 *         0xc000). Also locked: a 256-byte write+read at 0x40000000 (UMAC
 *         SYSRAM, BAR 0x70000) before firmware was up (readable later with
 *         BN0/BN1 granted). Quarantine for host access unless OSC_MASK
 *         (conn.h) shows the matching grant: PLE/PSE 0x820c0000-0x820cffff,
 *         LMAC/PHY 0x820d0000-0x830fffff (registers: mac.h, phy.h), WFSYS_ON
 *         0x820b0000 (BAR 0xae000), UMAC SYSRAM 0x40000000. After any lock,
 *         only the FIRST hanging address is attributable: a wedged device
 *         hangs every read. After FLR on a wedged device, reads return fresh
 *         random data (two reads of a known-static window differ): always
 *         double-read a control window and require it static.       [measured]
 *
 * HAZARD: all-ones from any read may be a PCIe completion timeout (CTO is
 *         enabled at the default 50 us - 50 ms range on both link ends); never
 *         treat it as register content.                    [measured, static]
 */
#ifndef HW_MT7996_HOST_H
#define HW_MT7996_HOST_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------ */
/* PCI function and link                                                    */
/* ------------------------------------------------------------------------ */

#define MT7996_PCI_VENDOR_ID			0x14c3u
/* Primary interface. mt76 calls this "MT7996" (its MT7990 product is 0x7993);
 * the firmware files are named MT7990. WA present.                 [measured] */
#define MT7996_PCI_DEVICE_ID			0x7990u
/* Second PCIe interface of the same chip (separate root complex, own 2 MiB
 * BAR0). Register map not established.                              [measured]
 * HAZARD: never first-touch its BAR without a recorded read plan.  [measured] */
#define MT7996_PCI_DEVICE_ID_HIF2		0x7991u
#define MT7996_BAR0_SIZE			0x00200000u	/* 2 MiB, 32-bit memory BAR */
/*
 * Link: primary Gen3 x2; second interface Gen2 x1 (Gen3 x2 capable).
 * Reset methods offered: FLR, bus.  AER capability at config 0x200, no DPC.
 * Max payload stuck at 128 B (W1700K root port supports only 128; device
 * DevCap allows 256): ~1.66 GB/s TLP ceiling. Do not change MPS live.
 * ASPM L0s+L1 enabled by default: device->host DMA 0.32 GB/s with ASPM on and
 * host idle, 1.44 GB/s with ASPM off. Disable ASPM for streaming. [measured]
 * DMA masks: 36-bit streaming (data buffers), 32-bit coherent (descriptor
 * rings).                                                           [static]
 */
#define MT7996_DMA_STREAMING_BITS		36u
#define MT7996_DMA_COHERENT_BITS		32u

/* ------------------------------------------------------------------------ */
/* BAR0 windows and remap selectors                                         */
/* ------------------------------------------------------------------------ */

/* Direct region: any address below this is a BAR offset.            [static] */
#define MT7996_HOST_DIRECT_LIMIT		0x00100000u

/* L1 remap: 64 KiB data window, selector in CONN_BUS_CR_VON.
 * RW selector; bits 31:16 = target bits 31:16; low half preserved.
 * Healthy even when the L2/WiFi-die windows poison. Any 64 KiB-aligned base
 * may be programmed; an undecoded base reads 0, not poison.        [measured] */
#define MT7996_HOST_CONN_BUS_CR_VON_BASE	0x00155000u
#define MT7996_HOST_L1_REMAP			0x00155024u
#define   HOST_L1_REMAP_BASE			HW_GENMASK(31, 16)
#define MT7996_HOST_L1_WIN_BASE			0x00130000u
#define MT7996_HOST_L1_WIN_SIZE			0x00010000u
#define MT7996_HOST_L1_BAR(addr)		(MT7996_HOST_L1_WIN_BASE + ((addr) & 0xffffu))
#define MT7996_HOST_L1_SEL(addr)		((addr) & 0xffff0000u)

/* L2 remap: 4 KiB data window. Selector RW; bits 19:0 = target >> 12, upper
 * bits preserved.                                         [static, measured] */
#define MT7996_HOST_L2_REMAP			0x000001b4u
#define   HOST_L2_REMAP_BASE			HW_GENMASK(19, 0)
#define MT7996_HOST_L2_WIN_BASE			0x00001000u
#define MT7996_HOST_L2_WIN_SIZE			0x00001000u
#define MT7996_HOST_L2_BAR(addr)		(MT7996_HOST_L2_WIN_BASE + ((addr) & 0xfffu))
#define MT7996_HOST_L2_SEL(addr)		((addr) >> 12)

/* CBTOP remap (selector BAR 0x1f6554 bits 31:16, window BAR 0x1c0000).
 * Used only by the separate MT7990 product (0x7993); on 14c3:7990 the CBTOP
 * ranges go through L1.                                             [static] */
#define MT7996_HOST_CBTOP_REMAP			0x001f6554u
#define MT7996_HOST_CBTOP_WIN_BASE		0x001c0000u

/* L1-routed ranges (bus addresses).                                 [static] */
#define MT7996_BUS_INFRA_BASE			0x18000000u	/* CONN_INFRA, L1 view */
#define MT7996_BUS_INFRA_END			0x18bfffffu	/* incl. WFSYS0/1 PHY 0x184/0x188 */
#define MT7996_BUS_INFRA_MCU_BASE		0x7c000000u	/* CONN_INFRA, MCU view */
#define MT7996_BUS_INFRA_MCU_END		0x7c3fffffu
#define MT7996_BUS_CBTOP1_BASE			0x70000000u
#define MT7996_BUS_CBTOP1_END			0x77ffffffu
#define MT7996_BUS_CBTOP2_BASE			0xf0000000u
#define MT7996_BUS_INFRA_MCU_TO_L1(addr)	((addr) - MT7996_BUS_INFRA_MCU_BASE + \
						 MT7996_BUS_INFRA_BASE)

/*
 * Fixed map, in lookup order: X(bus, bar, size, name). First match wins;
 * 0x820cc000 appears twice (PP at BAR 0xe000 for 4 KiB is found first, so
 * 0x820cd000 resolves to BAR 0xa6000 through the later BN1 MUCOP entry).
 * A mapping is arithmetic only: it does not make the target powered,
 * clocked or safe to read (see HAZARDs above).               [static]
 */
#define MT7996_HOST_FIXED_MAP(X) \
	X(0x54000000u, 0x02000u, 0x1000u,  WFDMA_PCIE0_MCU_DMA0) \
	X(0x55000000u, 0x03000u, 0x1000u,  WFDMA_PCIE0_MCU_DMA1) \
	X(0x56000000u, 0x04000u, 0x1000u,  WFDMA_RESERVED) \
	X(0x57000000u, 0x05000u, 0x1000u,  WFDMA_MCU_WRAP_CR) \
	X(0x58000000u, 0x06000u, 0x1000u,  WFDMA_PCIE1_MCU_DMA0) \
	X(0x59000000u, 0x07000u, 0x1000u,  WFDMA_PCIE1_MCU_DMA1) \
	X(0x820c0000u, 0x08000u, 0x4000u,  WF_UMAC_TOP_PLE) \
	X(0x820c8000u, 0x0c000u, 0x2000u,  WF_UMAC_TOP_PSE) \
	X(0x820cc000u, 0x0e000u, 0x1000u,  WF_UMAC_TOP_PP) \
	X(0x74030000u, 0x10000u, 0x1000u,  PCIE_MAC) \
	X(0x820e0000u, 0x20000u, 0x0400u,  BN0_WF_CFG) \
	X(0x820e1000u, 0x20400u, 0x0200u,  BN0_WF_TRB) \
	X(0x820e2000u, 0x20800u, 0x0400u,  BN0_WF_AGG) \
	X(0x820e3000u, 0x20c00u, 0x0400u,  BN0_WF_ARB) \
	X(0x820e4000u, 0x21000u, 0x0400u,  BN0_WF_TMAC) \
	X(0x820e5000u, 0x21400u, 0x0800u,  BN0_WF_RMAC) \
	X(0x820ce000u, 0x21c00u, 0x0200u,  WF_SEC) \
	X(0x820e7000u, 0x21e00u, 0x0200u,  BN0_WF_DMA) \
	X(0x820cf000u, 0x22000u, 0x1000u,  WF_PF) \
	X(0x820e9000u, 0x23400u, 0x0200u,  BN0_WF_WTBLOFF) \
	X(0x820ea000u, 0x24000u, 0x0200u,  BN0_WF_ETBF) \
	X(0x820eb000u, 0x24200u, 0x0400u,  BN0_WF_LPON) \
	X(0x820ec000u, 0x24600u, 0x0200u,  BN0_WF_INT) \
	X(0x820ed000u, 0x24800u, 0x0800u,  BN0_WF_MIB) \
	X(0x820ca000u, 0x26000u, 0x2000u,  BN0_WF_MUCOP) \
	X(0x820d0000u, 0x30000u, 0x10000u, WF_WTBLON) \
	X(0x40000000u, 0x70000u, 0x10000u, WF_UMAC_SYSRAM) \
	X(0x00400000u, 0x80000u, 0x10000u, WF_MCU_SYSRAM) \
	X(0x00410000u, 0x90000u, 0x10000u, WF_MCU_SYSRAM_CFG) \
	X(0x820f0000u, 0xa0000u, 0x0400u,  BN1_WF_CFG) \
	X(0x820f1000u, 0xa0600u, 0x0200u,  BN1_WF_TRB) \
	X(0x820f2000u, 0xa0800u, 0x0400u,  BN1_WF_AGG) \
	X(0x820f3000u, 0xa0c00u, 0x0400u,  BN1_WF_ARB) \
	X(0x820f4000u, 0xa1000u, 0x0400u,  BN1_WF_TMAC) \
	X(0x820f5000u, 0xa1400u, 0x0800u,  BN1_WF_RMAC) \
	X(0x820f7000u, 0xa1e00u, 0x0200u,  BN1_WF_DMA) \
	X(0x820f9000u, 0xa3400u, 0x0200u,  BN1_WF_WTBLOFF) \
	X(0x820fa000u, 0xa4000u, 0x0200u,  BN1_WF_ETBF) \
	X(0x820fb000u, 0xa4200u, 0x0400u,  BN1_WF_LPON) \
	X(0x820fc000u, 0xa4600u, 0x0200u,  BN1_WF_INT) \
	X(0x820fd000u, 0xa4800u, 0x0800u,  BN1_WF_MIB) \
	X(0x820cc000u, 0xa5000u, 0x2000u,  BN1_WF_MUCOP) \
	X(0x820c4000u, 0xa8000u, 0x4000u,  BN1_WF_MUCOP2) \
	X(0x820b0000u, 0xae000u, 0x1000u,  WFSYS_ON) \
	X(0x80020000u, 0xb0000u, 0x10000u, WF_TOP_MISC_OFF) \
	X(0x81020000u, 0xc0000u, 0x10000u, WF_TOP_MISC_ON) \
	X(0x7c020000u, 0xd0000u, 0x10000u, CONN_INFRA_WFDMA) \
	X(0x7c060000u, 0xe0000u, 0x10000u, CONN_HOST_CSR_TOP) \
	X(0x7c000000u, 0xf0000u, 0x10000u, CONN_INFRA)
#define MT7996_HOST_FIXED_MAP_ENTRIES		49u

/* Frequently used fixed windows. */
#define MT7996_BAR_MCU_DMA0			0x00002000u	/* bus 0x54000000 */
#define MT7996_BAR_PCIE_MAC			0x00010000u	/* bus 0x74030000 */
#define MT7996_BAR_UMAC_SYSRAM			0x00070000u	/* bus 0x40000000, HAZARD above */
#define MT7996_BAR_MCU_SYSRAM			0x00080000u	/* bus 0x00400000 */
#define MT7996_BAR_WF_TOP_MISC_OFF		0x000b0000u	/* bus 0x80020000 */
#define MT7996_BAR_WF_TOP_MISC_ON		0x000c0000u	/* bus 0x81020000 */
#define MT7996_BAR_CONN_WFDMA			0x000d0000u	/* bus 0x7c020000 */
#define MT7996_BAR_CONN_HOST_CSR_TOP		0x000e0000u	/* bus 0x7c060000 */
#define MT7996_BAR_CONN_INFRA			0x000f0000u	/* bus 0x7c000000 */

/* Poison value returned by a faulted WiFi-die window (see HAZARD).  [measured] */
#define MT7996_HOST_POISON			0xdead2117u

/* ------------------------------------------------------------------------ */
/* CONN_HOST_CSR_TOP (BAR 0xe0000 = bus 0x7c060000)                         */
/* ------------------------------------------------------------------------ */

#define MT7996_TOP_BASE				0x000e0000u
#define MT7996_TOP_BUS_BASE			0x7c060000u

/* Host low-power control / ownership, per host band b = 0..3. RW.
 * Write DRV_OWN (full word 0x2) to request the host side; poll until
 * FW_OWN_STAT clears (reference: up to 51 reads, 10-20 ms apart); then write
 * 1 to IRQ_STAT(b) to acknowledge. Write FW_OWN to hand back.
 * Observed: band 0 reads 0 while the host owns; bands 1..3 read 0x4. After a
 * cold boot, and after every WF subsystem reset, the firmware owns band 0 and
 * host DMA/download is refused until DRV_OWN is re-requested. Reads correctly
 * while the L2 window is poisoned.                          [measured, static] */
#define MT7996_TOP_LPCR_HOST_BAND(b)		(0x000e0010u + (b) * 0x10u)
#define   TOP_LPCR_HOST_FW_OWN			HW_BIT(0)	/* W: give to firmware */
#define   TOP_LPCR_HOST_DRV_OWN			HW_BIT(1)	/* W: request for host */
#define   TOP_LPCR_HOST_FW_OWN_STAT		HW_BIT(2)	/* R: 1 = firmware owns */
/* Ownership-change interrupt status, per band. W1C.                 [static] */
#define MT7996_TOP_LPCR_HOST_BAND_IRQ_STAT(b)	(0x000e0014u + (b) * 0x10u)
#define   TOP_LPCR_HOST_BAND_STAT		HW_BIT(0)

/* Firmware state. RO. Values in FW_STATE. Readable while WiFi-die windows are
 * poisoned. Sequence: 0 after WF subsystem reset, 1 once the host owns band 0
 * and the ROM is in download mode, stays 1 through patch, WM and DSP starts,
 * 7 after the WA start. 2 seen with a WM image whose WA start fails; stays 1
 * if the DSP image is omitted. A wedged device reads garbage.   [measured] */
#define MT7996_TOP_MISC				0x000e00f0u
#define   TOP_MISC_FW_STATE			HW_GENMASK(2, 0)
#define     TOP_MISC_FW_STATE_INITIAL		0u
#define     TOP_MISC_FW_STATE_FW_DOWNLOAD	1u
#define     TOP_MISC_FW_STATE_NORMAL_OPERATION	2u
#define     TOP_MISC_FW_STATE_NORMAL_TRX	3u
#define     TOP_MISC_FW_STATE_RDY		7u
/* bits 31:3 undecoded */

/* Two live words (a low bit toggles): exclude from static digests. [measured] */
#define MT7996_TOP_LIVE0			0x000e0a00u
#define MT7996_TOP_LIVE1			0x000e0a08u
/* Reads 0xdeadbeef (filler, not poison).                            [measured] */
#define MT7996_TOP_FILLER			0x000e0c00u
#define   TOP_FILLER_VALUE			0xdeadbeefu

/* ------------------------------------------------------------------------ */
/* PCIe MAC (bus 0x74030000 = BAR 0x10000)                                  */
/* ------------------------------------------------------------------------ */

#define MT7996_PCIE_MAC_BASE			0x74030000u
/* Offset 0 reads 0x00511163 (stable, healthy while other windows poison).
 *                                                                 [measured] */
#define MT7996_PCIE_MAC_ID			0x74030000u
#define   PCIE_MAC_ID_VALUE			0x00511163u
#define MT7996_PCIE_MAC_INT_ENABLE		0x74030188u	/* RW [static] */
/* Second interface's MAC, reached through L1 only.                  [static] */
#define MT7996_PCIE1_MAC_BASE			0x74090000u
#define MT7996_PCIE1_MAC_INT_ENABLE		0x74090188u
/* No raw SerDes / PHY bypass identified; ROM PCIe PHY setup uses an internal
 * I2C path.                                                         [static] */

/* ------------------------------------------------------------------------ */
/* Firmware operation-mode word (MCU SYSRAM, bus 0x00401400 = BAR 0x81400)  */
/* ------------------------------------------------------------------------ */

/* SW_DEF block shared with firmware. MODE is NOT initialised by hardware or
 * by any firmware image (random SRAM content after reset, e.g. 0xfff20a23);
 * the reference driver writes 0 (normal) BEFORE download. Writable, holds
 * through boot.                                          [measured, static] */
#define MT7996_SWDEF_BASE			0x00401400u
#define MT7996_SWDEF_MODE			0x0040143cu
#define   SWDEF_MODE_FWOPMODE			HW_GENMASK(1, 0)
#define     SWDEF_MODE_FWOPMODE_NORMAL		0u
#define     SWDEF_MODE_FWOPMODE_ICAP		1u
#define     SWDEF_MODE_FWOPMODE_SPECTRUM	2u
#define MT7996_SWDEF_SER_STATS			0x00401440u	/* RO [static] */
#define MT7996_SWDEF_PLE_STATS			0x00401444u
#define MT7996_SWDEF_PSE_STATS			0x00401450u

/*
 * Bring-up / recovery recipe (all host-side):                       [measured]
 *  1. LPCR band 0: DRV_OWN, wait FW_OWN_STAT clear, ack IRQ_STAT.
 *  2. WFDMA0 reset/prefetch/enable, bind TX16/TX17/TX20/RX0/RX1 (wfdma.h).
 *  3. Wait FW_STATE == 1; download patch, WM, DSP, WA (fw_format.h,
 *     mcu_cmd.h); wait FW_STATE == 7.
 *  Reset: WF subsystem reset pulse (conn.h), clear PCI bus mastering, then
 *  and only then release DMA-exposed buffers; FW_STATE returns to 0 and
 *  band-0 ownership returns to firmware. Re-bring-up to RDY ~22 s.
 *  A timed-out UNI command stalls the mailbox (all later commands get no
 *  reply) until this reset + re-download.
 *  One unconsumed descriptor on a command ring blocks further commands; the
 *  WA ring never drains if the WA did not start.
 */

#endif /* HW_MT7996_HOST_H */
