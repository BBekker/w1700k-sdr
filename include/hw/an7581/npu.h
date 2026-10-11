/* SPDX-License-Identifier: MIT */
/*
 * AN7581 NPU: an eight-core RISC-V cluster with a host mailbox, Wi-Fi
 * offload rings, per-core watchdogs, two on-chip RAMs, and a "bridge" packet
 * DMA into the frame engine.
 *
 * ADDRESS SPACE:
 *   AN7581_NPU_* register offsets   relative to AN7581_NPU_BASE (0x1e900000,
 *                                   memmap.h); the registers start at
 *                                   +0x300000 (physical 0x1ec00000)
 *   AN7581_NPU_LOCAL_*               addresses as the NPU cores issue them
 *   AN7581_NPU_BRIDGE_*              offsets relative to AN7581_NPU_BRIDGE
 * DRAM is identity-mapped for the cores (an image at physical 0x84000000 runs
 * at 0x84000000) [measured]. On-chip DMA masters (the bridge) address the
 * NPU RAMs with the core address masked to 29 bits: NPU 0x3e8xxxxx is bus
 * 0x1e8xxxxx [static].
 *
 * HAZARD: an A53 read anywhere in 0x1e900000..0x1ebfffff (NPU block +0 ..
 *   +0x2fffff) hangs the whole SoC; recovered only by a power cycle
 *   [measured]. A53 writes there are accepted. Registers from +0x300000 read
 *   normally.
 * HAZARD: NPU cores reading 0x3e800000, 0x3e900000 and 0x1e900000 and then
 *   writing markers at doubling offsets up to 2 MiB into one of those windows
 *   hung the whole SoC [measured]. A trap handler cannot contain a write that
 *   lands in live hardware.
 * HAZARD: an MT7996 PCIe write to bus 0x1e890000 did not reach NPU 0x3e890000
 *   and left the MT7996 command path dead until its reset [measured]: PCIe
 *   inbound does not route to the NPU RAMs (or lands in another alias).
 *
 * Cores [measured, identical on all eight harts, mhartid 0..7]:
 *   misa 0x40801105 = RV32 I M A C + X (no F, D, V); mvendorid 0; marchid 1
 *   (Rocket); mimpid 0x20210428. 800.0 MHz (top of the 800/750/720/600 MHz
 *   clock options, scu.h). F/D instructions trap as illegal (mcause 2) even
 *   with mstatus.FS set. mtvec at reset 0x10000. mstatus at entry
 *   0x00001800 on harts 0-6; 0x80019800 on hart 7 (XS dirty: the RoCC
 *   coprocessor below).
 *   L1 D-cache 8 KiB, 64-byte lines, 2.5-cycle hits, blocking (one miss in
 *   flight per core). Nothing caches DRAM beyond L1: a DRAM miss costs ~117
 *   cycles (~146 ns) idle and ~425 ns while a PCIe master writes 1.28 GB/s.
 *   Stores are posted, one in flight per core; the cost of a store is the
 *   target RAM's per-transaction cost, whatever its size.
 *   Summed integer throughput ~6.4 G simple ops/s; no floating point.
 *
 * Measured throughput (all 8 cores unless stated):
 *   DRAM read 341 MB/s per core; 1.55-1.69 GB/s cluster; 1.09 GB/s while a
 *     PCIe master writes 1.28 GB/s into DRAM.
 *   DRAM copy 122 MB/s per core, 434 MB/s cluster.
 *   28-bit IQ repack, DRAM -> DRAM 164 Msps; DRAM -> pool RAM 238 Msps;
 *     best schedule 313 Msps idle, 235 Msps under a 1.28 GB/s PCIe stream.
 *   The NPU shares the DRAM ceiling with the A53s; it adds no bandwidth.
 *
 * Ownership [static]: the NPU has no QDMA ring of its own; its only path
 * into the frame engine is the bridge (PSE port 6).
 */
#ifndef HW_AN7581_NPU_H
#define HW_AN7581_NPU_H

#include "../hw_bits.h"

#define AN7581_NPU_NUM_CORES            8u
#define AN7581_NPU_CLK_HZ               800000000u

/* ======================================================================== */
/* NPU-local address map (core view)                                        */
/* ======================================================================== */

/* Reset trap vector. */
#define AN7581_NPU_LOCAL_MTVEC_RESET    0x00010000u

/*
 * Firmware pool RAM, 480 KiB [static: the firmware allocator hands out
 * 0x3e800000 + offset for offset < 0x78000, 0x3e878000 = empty]. Banked:
 * cores in different banks do not contend [measured]. Core loads 12 cycles;
 * stores 11 cycles on one core (290 MB/s); eight cores 16 cycles each and
 * 1.6 GB/s total at 32 KiB spacing, 32 cycles and 800 MB/s at 4 KiB spacing
 * [measured]. The bridge packet buffer is allocated here.
 */
#define AN7581_NPU_LOCAL_POOL_BASE      0x3e800000u
#define AN7581_NPU_LOCAL_POOL_SIZE      0x00078000u

/*
 * "L2C" RAM, 256 KiB, usable after L2C_EN = 1 (the firmware then zeroes it)
 * [static + measured]. It is a plain on-chip RAM, not a DRAM cache: L1 does
 * not cache it, loads cost 62.85 cycles flat (~79 ns), and DRAM latency is
 * unchanged with it enabled [measured]. One serialized port: one transaction
 * per 12 core cycles (15 ns) for the whole cluster, 66.7 M/s, so 4-byte
 * stores cap at ~267 MB/s total and byte stores cost the same; loads 62.7 /
 * 64.5 / 128 / 256 cycles with 1 / 2 / 4 / 8 cores active (~25 M loads/s
 * total) [measured].
 */
#define AN7581_NPU_LOCAL_L2C_BASE       0x3e880000u
#define AN7581_NPU_LOCAL_L2C_SIZE       0x00040000u

/* Core address -> on-chip bus address (bridge, other fabric masters). */
#define AN7581_NPU_LOCAL_TO_BUS(a)      ((a) & 0x1fffffffu)

/* ======================================================================== */
/* Register map (offsets from AN7581_NPU_BASE) [static unless tagged]       */
/* ======================================================================== */

/* Local SRAM window: the driver writes the data image (<= 64 KiB) at +0.
 * HAZARD: A53 reads of +0 .. +0x2fffff hang the SoC (see top). */
#define AN7581_NPU_DATA_IMAGE_OFFSET    0x000000u
#define AN7581_NPU_DATA_IMAGE_MAX       0x10000u
#define AN7581_NPU_RV32_IMAGE_MAX       0x200000u /* code image, in DRAM */

/* Per-core debug: +0 PC, +4 SP, +8 LR. RO. Shows each running core's PC
 * [measured]; all read 0 before the first boot [measured]. */
#define AN7581_NPU_PC_DBG(core)         (0x305000u + 0x100u * (core))
#define AN7581_NPU_PC_DBG_PC            0x0u
#define AN7581_NPU_PC_DBG_SP            0x4u
#define AN7581_NPU_PC_DBG_LR            0x8u

/*
 * Cluster boot.
 * BOOT_TRIGGER: write 1 to start every core enabled in BOOT_CONFIG.
 * BOOT_CONFIG: bit n enables core n (0xff = all). Reads 0x10000 before the
 *   first boot [measured].
 * BOOT_BASE(n): core n's reset entry address (DRAM). Reads 0 before the first
 *   boot [measured].
 * A core parked in wfi does not restart on BOOT_TRIGGER, nor on clearing and
 * re-setting BOOT_CONFIG; pulse SCU RST_CTRL2 bit 9 first [measured].
 */
#define AN7581_NPU_BOOT_TRIGGER         0x306000u
#define AN7581_NPU_BOOT_TRIGGER_GO      HW_BIT(0)
#define AN7581_NPU_BOOT_CONFIG          0x306004u
#define AN7581_NPU_BOOT_CONFIG_CORE(n)  HW_BIT(n)
#define AN7581_NPU_BOOT_CONFIG_ALL      0xffu
#define AN7581_NPU_BOOT_CONFIG_RESET_VAL 0x00010000u
#define AN7581_NPU_BOOT_BASE(core)      (0x306020u + 0x4u * (core))

/*
 * Host <-> NPU mailbox.
 * MBQ0 (host -> NPU), words: [0] payload DMA address, [1] payload length
 * (<= 256 B in mainline), [2] sequence counter (host increments), [3] message
 * word (written last; the NPU sets DONE and STATUS in it). The payload buffer
 * is also the reply buffer.
 * MBQ8 (NPU -> host): the host acknowledges by writing DONE (STATUS clear)
 * into word 3 after clearing INT_STATUS.
 */
#define AN7581_NPU_MBOX_INT_STATUS      0x30c000u
#define AN7581_NPU_MBOX_INT_STATUS_MBOX HW_BIT(8)    /* write 1 to clear */
#define AN7581_NPU_MBOX_INT_MASK(n)     (0x30c004u + 0x4u * (n))
#define AN7581_NPU_MBQ0_CTRL(w)         (0x30c030u + 0x4u * (w))
#define AN7581_NPU_MBQ8_CTRL(w)         (0x30c0b0u + 0x4u * (w))
#define AN7581_NPU_MBOX_MSG_FUNC_ID     HW_GENMASK(14, 11)
#define AN7581_NPU_MBOX_MSG_STATIC_BUF  HW_BIT(5)
#define AN7581_NPU_MBOX_MSG_STATUS      HW_GENMASK(4, 2)  /* 1 success, 0 error */
#define AN7581_NPU_MBOX_MSG_DONE        HW_BIT(1)
#define AN7581_NPU_MBOX_MSG_WAIT_RSP    HW_BIT(0)
#define AN7581_NPU_MBOX_FUNC_WIFI       0u
#define AN7581_NPU_MBOX_FUNC_TUNNEL     1u
#define AN7581_NPU_MBOX_FUNC_NOTIFY     2u
#define AN7581_NPU_MBOX_FUNC_DBA        3u
#define AN7581_NPU_MBOX_FUNC_TR471      4u
#define AN7581_NPU_MBOX_FUNC_PPE        5u
#define AN7581_NPU_MBOX_OP_SET          1u /* payload func_type */
#define AN7581_NPU_MBOX_OP_SET_NO_WAIT  2u
#define AN7581_NPU_MBOX_OP_GET          3u
#define AN7581_NPU_MBOX_OP_GET_NO_WAIT  4u
#define AN7581_NPU_MBOX_MAX_PAYLOAD     256u

/*
 * NPU "MIB" scratch words, read by the firmware at boot. Mainline writes
 * MIB(10) = DRAM scratch (image base + 2 MiB), MIB(11) = 0x40000 (SRAM size,
 * 256 KiB), MIB(12) = 0, MIB(21) = 1. MIB(31) (0x1ec0c1bc) takes test-command
 * codes from the vendor bridge test tool.
 */
#define AN7581_NPU_MIB(n)               (0x30c140u + 0x4u * (n))
#define AN7581_NPU_MIB_DRAM_SCRATCH     10u
#define AN7581_NPU_MIB_SRAM_SIZE        11u
#define AN7581_NPU_MIB_SRAM_SIZE_VAL    0x40000u
#define AN7581_NPU_MIB_TEST_CMD         31u

/* Wi-Fi offload rings (CPU <-> NPU <-> radio; mainline wires them for MT7992
 * only). Ring n: +0 base, +4 descriptor count, +8 CPU index, +0xc DMA index. */
#define AN7581_NPU_WLAN_IRQ_STATUS      0x30d030u
#define AN7581_NPU_WLAN_IRQ_RXDONE(q)   (0x30d034u + 0x4u * (q))
#define AN7581_NPU_WLAN_IRQ_RX(q)       ((q) == 1 ? HW_BIT(17) : HW_BIT(16))
#define AN7581_NPU_WLAN_TX_RING(n)      (0x30d080u + 0x10u * (n))
#define AN7581_NPU_WLAN_RX_RING(n)      (0x30d180u + 0x10u * (n))
#define AN7581_NPU_WLAN_RING_BASE       0x0u
#define AN7581_NPU_WLAN_RING_DSCP_NUM   0x4u
#define AN7581_NPU_WLAN_RING_CPU_IDX    0x8u
#define AN7581_NPU_WLAN_RING_DMA_IDX    0xcu

/* Enables the 256 KiB L2C RAM at NPU 0x3e880000. RW; reads back 1 after
 * a write of 1 [measured]. Off after an NPU reset until the firmware's L2
 * init (or the host) sets it [inferred]. */
#define AN7581_NPU_L2C_EN               0x30f200u
#define AN7581_NPU_L2C_EN_EN            HW_BIT(0)

/* Per-core watchdog control. */
#define AN7581_NPU_WDT_TIMER_CTRL(core) (0x310100u + 0x100u * (core))
#define AN7581_NPU_WDT_TIMER_CTRL_EN    HW_BIT(25)
#define AN7581_NPU_WDT_TIMER_CTRL_INTR  HW_BIT(21) /* write 1 to clear */

/* ======================================================================== */
/* Bridge: NPU packet DMA into the frame engine (physical 0x1ec12000)       */
/* ======================================================================== */

/*
 * Moves packets from NPU-addressable RAM into the frame engine as PSE port 6,
 * with header add/remove, "create a new packet", header-buffer release and
 * direct hardware forwarding to a GDM or a QDMA. 8 channels, 64 KiB packet
 * buffer, 128-byte header unit [static: vendor firmware and test tool].
 * Register reads are harmless; unimplemented offsets read 0xdeadbeef without
 * a bus error [measured]. "Reset" values below are after an NPU reset with
 * the NPU otherwise idle [measured].
 */
#define AN7581_NPU_BRIDGE               0x312000u
#define AN7581_NPU_BRIDGE_PHYS          0x1ec12000u
#define AN7581_NPU_BRIDGE_UNIMPL_VAL    0xdeadbeefu
#define AN7581_NPU_BRIDGE_NUM_CH        8u
#define AN7581_NPU_BRIDGE_PKT_BUF_BYTES 0x10000u

/* Packet-buffer bus address (core address masked to 29 bits). Reset 0. */
#define AN7581_NPU_BRIDGE_PKT_BUF_ADDR  0x008u
/* Init configuration; firmware writes 0x40800. Reset 0x00020800. Undecoded. */
#define AN7581_NPU_BRIDGE_INIT_CFG      0x010u
#define AN7581_NPU_BRIDGE_INIT_CFG_FW_VAL 0x00040800u
#define AN7581_NPU_BRIDGE_INIT_CFG_RESET_VAL 0x00020800u
/* Init start; firmware writes 1 after INIT_CFG. Reset 0. */
#define AN7581_NPU_BRIDGE_INIT_START    0x018u
/* Output-queue priority (vendor test tool). */
#define AN7581_NPU_BRIDGE_OQ_PRIO       0x01cu
/* rbus burst size (16..128 B) and TXMBI maximum packet length (up to
 * 0xffff). Reset 0xffff0107; field positions undecoded. */
#define AN7581_NPU_BRIDGE_RBUS_CFG      0x020u
#define AN7581_NPU_BRIDGE_RBUS_CFG_RESET_VAL 0xffff0107u
/* Per-channel words, undecoded. Reset: CH_CFG0(n) = 0x00030003 +
 * 0x08000800 * n; CH_CFG1(n) = 0x800. */
#define AN7581_NPU_BRIDGE_CH_CFG0(ch)   (0x030u + 0x4u * (ch))
#define AN7581_NPU_BRIDGE_CH_CFG1(ch)   (0x050u + 0x4u * (ch))
/* Per-channel buffer init: firmware polls bit 0 ("buf init success"), then
 * writes 1. Reset 0. */
#define AN7581_NPU_BRIDGE_CH_INIT(ch)   (0x210u + 0x10u * (ch))
#define AN7581_NPU_BRIDGE_CH_INIT_DONE  HW_BIT(0)
/* Debug counters, RO; all 0 after reset. */
#define AN7581_NPU_BRIDGE_DBG_FSM       0x290u
#define AN7581_NPU_BRIDGE_DBG_FREE_PKTBUF 0x294u
#define AN7581_NPU_BRIDGE_DBG_TXMBI_IN  0x298u
#define AN7581_NPU_BRIDGE_DBG_TXCMD_IN  0x29cu
#define AN7581_NPU_BRIDGE_DBG_TXCMD_OUT 0x2a0u
#define AN7581_NPU_BRIDGE_DBG_RXCMD_IN  0x2a4u
#define AN7581_NPU_BRIDGE_DBG_RXCMD_OUT 0x2a8u
#define AN7581_NPU_BRIDGE_DBG_RXMBI_OUT 0x2acu
/* Per-channel counter banks at 0x2b0, 0x2d0, 0x2f0, 0x310, 0x330, 0x350,
 * word n = channel n. */
#define AN7581_NPU_BRIDGE_DBG_CH(bank, ch) (0x2b0u + 0x20u * (bank) + 0x4u * (ch))
#define AN7581_NPU_BRIDGE_DBG_MODE      0x370u

/* ======================================================================== */
/* Running our own code on the cores [measured procedure]                   */
/* ======================================================================== */

/*
 * 1. Pulse SCU RST_CTRL2 bit 9 (scu.h).
 * 2. Write a position-independent RV32 image into DRAM (the reserved
 *    0x84000000 region while the NPU driver is absent), mapped uncached.
 * 3. BOOT_BASE(n) = entry for every core; BOOT_CONFIG = 0xff;
 *    BOOT_TRIGGER = 1. Optionally L2C_EN = 1 for the L2C RAM.
 * 4. Cores publish results with CFLUSH.D.L1; they read host-written words
 *    after CDISCARD.D.L1.
 */

/* Rocket L1 D-cache maintenance (custom SYSTEM encodings): flush (write back
 * and invalidate) or discard the line holding address rs1. Both work on these
 * cores [measured]. */
#define AN7581_NPU_INSN_CFLUSH_D_L1(rs1)   (0xfc000073u | ((unsigned int)(rs1) << 15))
#define AN7581_NPU_INSN_CDISCARD_D_L1(rs1) (0xfc200073u | ((unsigned int)(rs1) << 15))

/* ======================================================================== */
/* Hart 7 RoCC coprocessor: SM4 engine                                      */
/* ======================================================================== */

/*
 * Harts 0-6: custom-0..3 trap as illegal instructions [measured].
 * Hart 7: all four are accepted [measured]. custom-3 with funct7 0 in the
 * response form (xd = xs1 = xs2 = 1) computes the Zksed sm4ed step with
 * bs = 0: rd = rs1 ^ L'(SBOX(rs2[7:0])), only byte 0 of rs2 read
 * [measured: rs2 = 0 -> rs1 ^ 0x5b5bd58e, rs2 = 1 -> 0x424292d0,
 * rs2 = 0x100 -> 0x5b5bd58e]. Other funct7 values (probably byte selects and
 * key steps) are unexplored. The vendor firmware never issues these
 * opcodes [static].
 * HAZARD: custom-0, -1 or -2 with zero operands leave the coprocessor busy;
 *   the hart then never retires its next fence-class instruction (a
 *   CFLUSH.D.L1 loop) and stays stuck until the NPU reset [measured].
 */
#define AN7581_NPU_ROCC_HART            7u
#define AN7581_NPU_ROCC_OPCODE_CUSTOM0  0x0bu
#define AN7581_NPU_ROCC_OPCODE_CUSTOM1  0x2bu
#define AN7581_NPU_ROCC_OPCODE_CUSTOM2  0x5bu
#define AN7581_NPU_ROCC_OPCODE_CUSTOM3  0x7bu
#define AN7581_NPU_ROCC_INSN(opc, funct7, rd, rs1, rs2, xd, xs1, xs2) \
	(((unsigned int)(funct7) << 25) | ((unsigned int)(rs2) << 20) | \
	 ((unsigned int)(rs1) << 15) | ((unsigned int)(xd) << 14) | \
	 ((unsigned int)(xs1) << 13) | ((unsigned int)(xs2) << 12) | \
	 ((unsigned int)(rd) << 7) | (unsigned int)(opc))
/* SM4 L'(SBOX(b)) for b = 0 and b = 1 (the custom-3 results with rs1 = 0). */
#define AN7581_NPU_SM4_LSBOX_0          0x5b5bd58eu
#define AN7581_NPU_SM4_LSBOX_1          0x424292d0u

#endif /* HW_AN7581_NPU_H */
