/* SPDX-License-Identifier: MIT */
/*
 * AN7581 Ethernet: frame engine (FE) with its packet switch engine (PSE),
 * GDM ports, CDM ports and PPEs; the two QDMA DMA engines; the SerDes PCS
 * blocks behind GDM2/GDM4; behaviour of the built-in switch.
 *
 * ADDRESS SPACE:
 *   AN7581_FE_*, AN7581_GDM_*, AN7581_CDM_*, AN7581_PPE_*
 *                        offsets relative to AN7581_FE_BASE (0x1fb50000)
 *   AN7581_QDMA_*        offsets relative to AN7581_QDMA_BASE(n)
 *                        (QDMA0 0x1fb54000, QDMA1 0x1fb56000)
 *   AN7581_PCS_*         absolute serdes sub-block bases, or offsets within
 *                        the XFI MAC block AN7581_PCS_{PON,ETH}_MAC_BASE
 * Register meanings are [static] (mainline airoha_eth / pcs-airoha) unless
 * tagged otherwise.
 *
 * Data path [static]: every byte that reaches a GDM is fetched by a DMA
 * master: QDMA (from DRAM), the NPU bridge (from NPU on-chip RAM, npu.h) or
 * the PCE copy engine (frames already in the fabric). No core-writable path
 * into a MAC or PHY exists; MDIO is management only.
 *
 * QDMA ownership [static]: QDMA0 serves the LAN GDM (GDM1, the internal
 * switch link); QDMA1 serves the WAN GDM. GDM2 is always WAN. GDM4 is WAN
 * only when no other WAN GDM exists, so with GDM2 present it is LAN
 * [inferred: probe order GDM2 first].
 *
 * Measured on the W1700K (GDM2 = RTL8261N 10GBASE-T, USXGMII):
 *   GDM2 links at 10 Gb/s full duplex.
 *   Line rate from a hot buffer: 1500-byte frames 820 k frames/s
 *     (10.1 Gb/s on the wire); 8250-byte frames 151,092 frames/s (9.97 Gb/s).
 *   From DRAM while a PCIe master writes 1.28 GB/s into it: ~1.14 GB/s with
 *     MAX_ISSUE_NUM 2, ~1.22 GB/s (~149 k 8-KiB frames/s) with 3.
 *   8192-byte UDP payloads cost 8274 bytes on the wire: at most 1.238 GB/s
 *     of payload per 10GbE link.
 *   Concurrent QDMA0 traffic lowers QDMA1 throughput (~138 k vs ~149 k
 *     8-KiB frames/s) [measured]; a shared DRAM read path is the likely
 *     cause [inferred].
 *   Two TX queues of one QDMA channel under the driver defaults: queue 1
 *     starves behind queue 0 (strict priority).
 *   The built-in switch drops traffic arriving at 10 Gb/s for a 1 Gb/s port
 *     and does not pause the CPU port; a 1G egress through the switch has to
 *     be paced in software (600-900 Mb/s lossless).
 *   Linux IRQ numbers on the project kernel: 23 = QDMA0, 27 = QDMA1 (bank 0
 *     lines; GIC SPI 37/38, memmap.h). QDMA1's completion IRQ wants a CPU of
 *     its own at line rate.
 */
#ifndef HW_AN7581_ETH_H
#define HW_AN7581_ETH_H

#include "../hw_bits.h"

/* ======================================================================== */
/* Frame engine sub-blocks (offsets from AN7581_FE_BASE)                    */
/* ======================================================================== */

#define AN7581_FE_PSE                   0x0100u
#define AN7581_FE_CSR_IFC               0x0200u
#define AN7581_FE_CDM1                  0x0400u
#define AN7581_FE_GDM1                  0x0500u
#define AN7581_FE_PPE1                  0x0c00u
#define AN7581_FE_GDM3                  0x1100u
#define AN7581_FE_CDM2                  0x1400u
#define AN7581_FE_GDM2                  0x1500u
#define AN7581_FE_PPE2                  0x1c00u
#define AN7581_FE_GDM4                  0x2500u

#define AN7581_FE_GDM_BASE(n)           ((n) == 4 ? AN7581_FE_GDM4 : (n) == 3 ? AN7581_FE_GDM3 : \
                                         (n) == 2 ? AN7581_FE_GDM2 : AN7581_FE_GDM1)
#define AN7581_FE_CDM_BASE(n)           ((n) == 2 ? AN7581_FE_CDM2 : AN7581_FE_CDM1)
#define AN7581_FE_PPE_BASE(n)           ((n) ? AN7581_FE_PPE2 : AN7581_FE_PPE1)

/* PSE port numbers (FPORT values, PSE queue-config PORT_ID). The NPU bridge
 * enters the fabric as PSE port 6. */
#define AN7581_PSE_PORT_CDM1            0u
#define AN7581_PSE_PORT_GDM1            1u
#define AN7581_PSE_PORT_GDM2            2u
#define AN7581_PSE_PORT_GDM3            3u
#define AN7581_PSE_PORT_PPE1            4u
#define AN7581_PSE_PORT_CDM2            5u
#define AN7581_PSE_PORT_CDM3            6u
#define AN7581_PSE_PORT_CDM4            7u
#define AN7581_PSE_PORT_PPE2            8u
#define AN7581_PSE_PORT_GDM4            9u
#define AN7581_PSE_PORT_CDM5            10u
#define AN7581_PSE_PORT_DROP            15u
#define AN7581_PSE_PORT_NPU_BRIDGE      6u

/* ======================================================================== */
/* FE global registers                                                      */
/* ======================================================================== */

#define AN7581_FE_DMA_GLO_CFG           0x0000u
#define AN7581_FE_DMA_GLO_CFG_L2_SPACE  HW_GENMASK(7, 4)
#define AN7581_FE_DMA_GLO_CFG_PG_SZ     HW_BIT(3)

#define AN7581_FE_RST_GLO_CFG           0x0004u
#define AN7581_FE_RST_GLO_CFG_GDM4_MBI_ARB HW_BIT(3)
#define AN7581_FE_RST_GLO_CFG_GDM3_MBI_ARB HW_BIT(2)
#define AN7581_FE_RST_GLO_CFG_CORE      HW_BIT(0)

#define AN7581_FE_FOE_TS                0x0010u /* PPE flow timestamp */

#define AN7581_FE_WAN_PORT              0x0024u
#define AN7581_FE_WAN_PORT_WAN1_EN      HW_BIT(16)
#define AN7581_FE_WAN_PORT_WAN1         HW_GENMASK(12, 8)
#define AN7581_FE_WAN_PORT_WAN0         HW_GENMASK(4, 0)

/* MAC address match: +0 high bytes, +4 low bound, +8 high bound. */
#define AN7581_FE_WAN_MAC_H             0x0030u
#define AN7581_FE_LAN_MAC_H             0x0040u
#define AN7581_FE_MAC_LMIN(base)        ((base) + 0x04u)
#define AN7581_FE_MAC_LMAX(base)        ((base) + 0x08u)

#define AN7581_FE_CDM1_OQ_MAP(n)        (0x0050u + 0x4u * (n)) /* n = 0..3 */

/* Packet copy engine: DPI, keepalive and multicast duplication. */
#define AN7581_FE_PCE_CFG               0x0070u
#define AN7581_FE_PCE_CFG_DPI_EN        HW_BIT(2)
#define AN7581_FE_PCE_CFG_KA_EN         HW_BIT(1)
#define AN7581_FE_PCE_CFG_MC_EN         HW_BIT(0)

/* PSE output-queue reservation, written indirectly. */
#define AN7581_FE_PSE_QUEUE_CFG_WR      0x0080u
#define AN7581_FE_PSE_QUEUE_CFG_WR_PORT_ID  HW_GENMASK(27, 24)
#define AN7581_FE_PSE_QUEUE_CFG_WR_QUEUE_ID HW_GENMASK(20, 16)
#define AN7581_FE_PSE_QUEUE_CFG_WR_WR_EN    HW_BIT(8)
#define AN7581_FE_PSE_QUEUE_CFG_WR_OQRSV_SEL HW_BIT(0)
#define AN7581_FE_PSE_QUEUE_CFG_VAL     0x0084u
#define AN7581_FE_PSE_QUEUE_CFG_VAL_OQ_RSV  HW_GENMASK(13, 0)

#define AN7581_FE_PSE_FQ_CFG            0x008cu
#define AN7581_FE_PSE_FQ_CFG_LIMIT      HW_GENMASK(14, 0)

#define AN7581_FE_PSE_BUF_SET           0x0090u
#define AN7581_FE_PSE_BUF_SET_SHARE_USED_LTHD HW_GENMASK(31, 16)
#define AN7581_FE_PSE_BUF_SET_ALLRSV    HW_GENMASK(14, 0)

#define AN7581_FE_PSE_SHARE_USED_THD    0x0094u
#define AN7581_FE_PSE_SHARE_USED_THD_M  HW_GENMASK(31, 16)
#define AN7581_FE_PSE_SHARE_USED_THD_H  HW_GENMASK(15, 0)

#define AN7581_FE_PSE_IQ_REV1           0x0108u
#define AN7581_FE_PSE_IQ_REV1_RES1_P2   HW_GENMASK(23, 16)
#define AN7581_FE_PSE_IQ_REV2           0x010cu
#define AN7581_FE_PSE_IQ_REV2_RES2_P5   HW_GENMASK(15, 8)
#define AN7581_FE_PSE_IQ_REV2_RES2_P4   HW_GENMASK(7, 0)

#define AN7581_FE_GDM_MISC_CFG          0x0148u
#define AN7581_FE_GDM_MISC_CFG_GDM2_RDM_ACK_WAIT_PREF HW_BIT(9)
#define AN7581_FE_GDM_MISC_CFG_GDM2_CHN_VLD_MODE      HW_BIT(5)

#define AN7581_FE_VIP_PORT_EN           0x01f0u /* bit n = GDM n */
#define AN7581_FE_IFC_PORT_EN           0x01f4u /* bit n = GDM n */

#define AN7581_FE_CSR_IFC_CFG           0x0200u
#define AN7581_FE_CSR_IFC_CFG_EN        HW_BIT(0)

/* VIP (very-important-packet) pattern n. */
#define AN7581_FE_VIP_EN(n)             (0x0300u + 0x8u * (n))
#define AN7581_FE_VIP_EN_FCPU           HW_BIT(7)
#define AN7581_FE_VIP_EN_SWP            HW_BIT(6)
#define AN7581_FE_VIP_EN_DP             HW_BIT(5)
#define AN7581_FE_VIP_EN_SP             HW_BIT(4)
#define AN7581_FE_VIP_EN_TYPE           HW_GENMASK(3, 1)
#define AN7581_FE_VIP_EN_EN             HW_BIT(0)
#define AN7581_FE_VIP_PATN(n)           (0x0304u + 0x8u * (n))
#define AN7581_FE_VIP_PATN_DP           HW_GENMASK(31, 16)
#define AN7581_FE_VIP_PATN_SP           HW_GENMASK(15, 0)

#define AN7581_FE_IP_FRAG_FP            0x2010u
#define AN7581_FE_IP_FRAG_FP_ASSEMBLE_PORT HW_GENMASK(24, 21)
#define AN7581_FE_IP_FRAG_FP_ASSEMBLE_NBQ  HW_GENMASK(20, 16)
#define AN7581_FE_IP_FRAG_FP_FRAGMENT_PORT HW_GENMASK(8, 5)
#define AN7581_FE_IP_FRAG_FP_FRAGMENT_NBQ  HW_GENMASK(4, 0)

/* GDM4 MBI fragmenting toward its two SGMII lanes (TX and RX). */
#define AN7581_FE_GDM4_TMBI_FRAG        0x2028u
#define AN7581_FE_GDM4_RMBI_FRAG        0x202cu
#define AN7581_FE_GDM4_MBI_FRAG_SGMII1_WEIGHT HW_GENMASK(31, 26)
#define AN7581_FE_GDM4_MBI_FRAG_SGMII1_SIZE   HW_GENMASK(25, 16)
#define AN7581_FE_GDM4_MBI_FRAG_SGMII0_WEIGHT HW_GENMASK(15, 10)
#define AN7581_FE_GDM4_MBI_FRAG_SGMII0_SIZE   HW_GENMASK(9, 0)

#define AN7581_FE_SP_DFT_CPORT(n)       (0x20e0u + 0x4u * (n))
#define AN7581_FE_MC_VLAN_EN            0x2100u
#define AN7581_FE_MC_VLAN_CFG           0x2104u
#define AN7581_FE_MC_VLAN_CFG_CMD_DONE  HW_BIT(31)
#define AN7581_FE_MC_VLAN_CFG_TABLE_ID  HW_GENMASK(21, 16)
#define AN7581_FE_MC_VLAN_CFG_PORT_ID   HW_GENMASK(11, 8)
#define AN7581_FE_MC_VLAN_CFG_TABLE_SEL HW_BIT(4)
#define AN7581_FE_MC_VLAN_CFG_RW        HW_BIT(0)
#define AN7581_FE_MC_VLAN_DATA          0x2108u

/* Flow-control ID per source port (byte n = 5-bit FC ID of source n). */
#define AN7581_FE_SRC_PORT_FC_MAP6      0x2298u
#define AN7581_FE_SRC_PORT_FC_MAP6_DEFAULT 0x1b1a1918u

#define AN7581_FE_WAN_MTU0              0x2300u
#define AN7581_FE_WAN_MTU0_MTU1         HW_GENMASK(29, 16)
#define AN7581_FE_WAN_MTU0_MTU0         HW_GENMASK(13, 0)

#define AN7581_FE_CDM5_RX_OQ1_DROP_CNT  0x29d4u

/* CPU port (CDM side of GDM1). */
#define AN7581_FE_CPORT_CFG             (AN7581_FE_GDM1 + 0x40u)
#define AN7581_FE_CPORT_CFG_PAD         HW_BIT(26)
#define AN7581_FE_CPORT_CFG_PORT_XFC    HW_BIT(25)
#define AN7581_FE_CPORT_CFG_QUEUE_XFC   HW_BIT(24)

/* ======================================================================== */
/* CDM (CPU DMA port) registers: AN7581_FE_CDM_BASE(n) + offset             */
/* ======================================================================== */

#define AN7581_CDM_VLAN_CTRL            0x00u
#define AN7581_CDM_VLAN_CTRL_VLAN       HW_GENMASK(31, 16)
#define AN7581_CDM_FWD_CFG              0x08u
#define AN7581_CDM_FWD_CFG_OAM_QSEL     HW_GENMASK(31, 27)
#define AN7581_CDM_FWD_CFG_VIP_QSEL     HW_GENMASK(24, 20)
#define AN7581_CDM_CRSN_QSEL(m)         (0x10u + 0x4u * (m)) /* CPU-reason -> RX queue, 4 per word */
#define AN7581_CDM_LRO_RXQ(m)           (0x78u + ((m) & 0x4u))
#define AN7581_CDM_LRO_EN               0x80u
#define AN7581_CDM_LRO_EN_RXQ           HW_GENMASK(7, 0)
#define AN7581_CDM_LRO_LIMIT            0x84u
#define AN7581_CDM_LRO_LIMIT_AGG_NUM    HW_GENMASK(23, 16)
#define AN7581_CDM_LRO_LIMIT_AGG_SIZE   HW_GENMASK(15, 0)
#define AN7581_CDM_LRO_AGE_TIME         0x88u
#define AN7581_CDM_LRO_AGE_TIME_AGE     HW_GENMASK(31, 16)
#define AN7581_CDM_LRO_AGE_TIME_AGG     HW_GENMASK(15, 0)

/* ======================================================================== */
/* GDM (Ethernet MAC port) registers: AN7581_FE_GDM_BASE(n) + offset        */
/* ======================================================================== */

/* Forwarding: offloads, CRC handling and the PSE destination for unicast,
 * broadcast, multicast and other frames received on this GDM. */
#define AN7581_GDM_FWD_CFG              0x000u
#define AN7581_GDM_FWD_CFG_PAD_EN       HW_BIT(28)
#define AN7581_GDM_FWD_CFG_DROP_CRC_ERR HW_BIT(23)
#define AN7581_GDM_FWD_CFG_IP4_CKSUM    HW_BIT(22)
#define AN7581_GDM_FWD_CFG_TCP_CKSUM    HW_BIT(21)
#define AN7581_GDM_FWD_CFG_UDP_CKSUM    HW_BIT(20)
#define AN7581_GDM_FWD_CFG_STRIP_CRC    HW_BIT(16)
#define AN7581_GDM_FWD_CFG_UCFQ         HW_GENMASK(15, 12)
#define AN7581_GDM_FWD_CFG_BCFQ         HW_GENMASK(11, 8)
#define AN7581_GDM_FWD_CFG_MCFQ         HW_GENMASK(7, 4)
#define AN7581_GDM_FWD_CFG_OCFQ         HW_GENMASK(3, 0)

#define AN7581_GDM_INGRESS_CFG          0x010u
#define AN7581_GDM_INGRESS_CFG_FC_EN    HW_BIT(1)
#define AN7581_GDM_INGRESS_CFG_STAG_EN  HW_BIT(0)

#define AN7581_GDM_LEN_CFG              0x014u
#define AN7581_GDM_LEN_CFG_LONG         HW_GENMASK(29, 16)
#define AN7581_GDM_LEN_CFG_SHORT        HW_GENMASK(13, 0)

/* Loopback: sends a GDM's traffic back into the fabric (mainline uses it on
 * GDM2 to carry GDM3/4 WAN traffic). */
#define AN7581_GDM_LPBK_CFG             0x01cu
#define AN7581_GDM_LPBK_CFG_GAP         HW_GENMASK(31, 24)
#define AN7581_GDM_LPBK_CFG_LEN         HW_GENMASK(23, 10)
#define AN7581_GDM_LPBK_CFG_CHAN        HW_GENMASK(8, 4)
#define AN7581_GDM_LPBK_CFG_MODE        HW_GENMASK(3, 1)
#define AN7581_GDM_LPBK_CFG_EN          HW_BIT(0)

#define AN7581_GDM_CHN_RLS              0x020u
#define AN7581_GDM_CHN_RLS_MBI_RX_AGE_SEL HW_GENMASK(26, 25)
#define AN7581_GDM_CHN_RLS_MBI_TX_AGE_SEL HW_GENMASK(18, 17)
#define AN7581_GDM_TXCHN_EN             0x024u
#define AN7581_GDM_RXCHN_EN             0x028u

/* MIB counters. RO; counters clear on read, so any other reader takes counts
 * from the driver's statistics. _L/_H are the low/high words of 64-bit
 * counts. */
#define AN7581_GDM_MIB_CLEAR            0x0f0u
#define AN7581_GDM_MIB_CLEAR_RX         HW_BIT(1)
#define AN7581_GDM_MIB_CLEAR_TX         HW_BIT(0)
#define AN7581_GDM_MIB_CFG              0x0f4u
#define AN7581_GDM_MIB_CFG_STRICT_RFC2819 HW_BIT(31)
#define AN7581_GDM_MIB_CFG_TX_SPLIT_EN  HW_BIT(17)
#define AN7581_GDM_MIB_CFG_RX_SPLIT_EN  HW_BIT(16)
#define AN7581_GDM_MIB_CFG_TX_MIB_ID    HW_GENMASK(15, 8)
#define AN7581_GDM_MIB_CFG_RX_MIB_ID    HW_GENMASK(7, 0)
#define AN7581_GDM_TX_OK_PKT_CNT_L      0x104u
#define AN7581_GDM_TX_OK_BYTE_CNT_L     0x10cu
#define AN7581_GDM_TX_ETH_PKT_CNT_L     0x110u
#define AN7581_GDM_TX_ETH_BYTE_CNT_L    0x114u
#define AN7581_GDM_TX_ETH_DROP_CNT      0x118u
#define AN7581_GDM_TX_ETH_BC_CNT        0x11cu
#define AN7581_GDM_TX_ETH_MC_CNT        0x120u
#define AN7581_GDM_TX_ETH_RUNT_CNT      0x124u
#define AN7581_GDM_TX_ETH_LONG_CNT      0x128u
#define AN7581_GDM_RX_OK_PKT_CNT_L      0x148u
#define AN7581_GDM_RX_FC_DROP_CNT       0x14cu
#define AN7581_GDM_RX_RC_DROP_CNT       0x150u
#define AN7581_GDM_RX_OVERFLOW_DROP_CNT 0x154u
#define AN7581_GDM_RX_ERROR_DROP_CNT    0x158u
#define AN7581_GDM_RX_OK_BYTE_CNT_L     0x15cu
#define AN7581_GDM_RX_ETH_PKT_CNT_L     0x160u
#define AN7581_GDM_RX_ETH_BYTE_CNT_L    0x164u
#define AN7581_GDM_RX_ETH_DROP_CNT      0x168u
#define AN7581_GDM_RX_ETH_CRC_ERR_CNT   0x174u
#define AN7581_GDM_TX_OK_PKT_CNT_H      0x280u
#define AN7581_GDM_TX_OK_BYTE_CNT_H     0x284u
#define AN7581_GDM_TX_ETH_PKT_CNT_H     0x288u
#define AN7581_GDM_TX_ETH_BYTE_CNT_H    0x28cu
#define AN7581_GDM_RX_OK_PKT_CNT_H      0x290u
#define AN7581_GDM_RX_OK_BYTE_CNT_H     0x294u
#define AN7581_GDM_RX_ETH_PKT_CNT_H     0x298u
#define AN7581_GDM_RX_ETH_BYTE_CNT_H    0x29cu
/* Frame-size histograms: TX E64/L64/L127/L255/L511/L1023 low words at
 * 0x12c..0x140, RX at 0x188..0x19c; high words at 0x2b8.. and 0x2e8.. */
#define AN7581_GDM_TX_ETH_SIZE_HIST_L   0x12cu
#define AN7581_GDM_RX_ETH_SIZE_HIST_L   0x188u
#define AN7581_GDM_TX_ETH_SIZE_HIST_H   0x2b8u
#define AN7581_GDM_RX_ETH_SIZE_HIST_H   0x2e8u

#define AN7581_GDM_SRC_PORT_SET         0x23cu
#define AN7581_GDM_SRC_PORT_SET_OFF2    HW_GENMASK(19, 16)
#define AN7581_GDM_SRC_PORT_SET_OFF1    HW_GENMASK(15, 12)
#define AN7581_GDM_SRC_PORT_SET_OFF0    HW_GENMASK(11, 8)

/* ======================================================================== */
/* PPE (flow offload) registers: AN7581_FE_PPE_BASE(n) + offset             */
/* ======================================================================== */

#define AN7581_PPE_GLO_CFG              0x200u
#define AN7581_PPE_GLO_CFG_BUSY         HW_BIT(31)
#define AN7581_PPE_GLO_CFG_FLOW_DROP_UPDATE HW_BIT(9)
#define AN7581_PPE_GLO_CFG_PSE_HASH_OFS HW_BIT(6)
#define AN7581_PPE_GLO_CFG_PPE_BSWAP    HW_BIT(5)
#define AN7581_PPE_GLO_CFG_TTL_DROP     HW_BIT(4)
#define AN7581_PPE_GLO_CFG_IP4_CS_DROP  HW_BIT(3)
#define AN7581_PPE_GLO_CFG_IP4_L4_CS_DROP HW_BIT(2)
#define AN7581_PPE_GLO_CFG_EN           HW_BIT(0)
#define AN7581_PPE_FLOW_CFG             0x204u
#define AN7581_PPE_IP_PROTO_CHK         0x208u
#define AN7581_PPE_TB_CFG               0x21cu
#define AN7581_PPE_TB_CFG_SRAM_NUM_ENTRY HW_GENMASK(26, 24)
#define AN7581_PPE_TB_CFG_ENTRY_SIZE    HW_BIT(3)
#define AN7581_PPE_TB_CFG_DRAM_NUM_ENTRY HW_GENMASK(2, 0)
#define AN7581_PPE_TB_BASE              0x220u  /* DRAM flow table base */
#define AN7581_PPE_BIND_RATE            0x228u
#define AN7581_PPE_HASH_SEED            0x244u
#define AN7581_PPE_HASH_SEED_VAL        0x12345678u
#define AN7581_PPE_DFT_CPORT(n)         (0x248u + 0x4u * ((n) / 8))
#define AN7581_PPE_TB_HASH_CFG          0x250u
#define AN7581_PPE_MTU(n)               (0x304u + 0x4u * ((n) / 2))
/* Indirect access to the on-chip (SRAM) flow table. */
#define AN7581_PPE_RAM_CTRL             0x31cu
#define AN7581_PPE_RAM_CTRL_ACK         HW_BIT(31)
#define AN7581_PPE_RAM_CTRL_DUAL_SUCCESS HW_BIT(30)
#define AN7581_PPE_RAM_CTRL_ENTRY       HW_GENMASK(23, 8)
#define AN7581_PPE_RAM_CTRL_WR_DUAL     HW_BIT(2)
#define AN7581_PPE_RAM_CTRL_WR          HW_BIT(1)
#define AN7581_PPE_RAM_CTRL_REQ         HW_BIT(0)
#define AN7581_PPE_RAM_ENTRY(n)         (0x320u + 0x4u * (n))
#define AN7581_PPE_UPDMEM_CTRL          0x370u
#define AN7581_PPE_UPDMEM_DATA          0x374u

/* ======================================================================== */
/* QDMA registers (offsets from AN7581_QDMA_BASE(n))                        */
/* ======================================================================== */

/*
 * Global configuration. RW.
 * Observed with the project driver on both QDMAs: 0x76380075 [measured] =
 * DMA_PREFERENCE 3 | CPU_TXR_RR | PAYLOAD_BYTE_SWAP | MULTICAST_MODIFY_FP |
 * MULTICAST_EN | IRQ1_EN | IRQ0_EN | TX_WB_DONE | MAX_ISSUE_NUM 3 |
 * RX_DMA_EN | TX_DMA_EN. Mainline writes MAX_ISSUE_NUM 2.
 * MAX_ISSUE_NUM is the number of outstanding DRAM reads the TX side issues;
 * 3 is the field maximum and is what reaches 10GbE line rate from DRAM under
 * concurrent PCIe write load [measured].
 */
#define AN7581_QDMA_GLOBAL_CFG          0x004u
#define AN7581_QDMA_GLOBAL_CFG_RX_2B_OFFSET     HW_BIT(31)
#define AN7581_QDMA_GLOBAL_CFG_DMA_PREFERENCE   HW_GENMASK(30, 29)
#define AN7581_QDMA_GLOBAL_CFG_CPU_TXR_RR       HW_BIT(28)
#define AN7581_QDMA_GLOBAL_CFG_DSCP_BYTE_SWAP   HW_BIT(27)
#define AN7581_QDMA_GLOBAL_CFG_PAYLOAD_BYTE_SWAP HW_BIT(26)
#define AN7581_QDMA_GLOBAL_CFG_MULTICAST_MODIFY_FP HW_BIT(25)
#define AN7581_QDMA_GLOBAL_CFG_OAM_MODIFY       HW_BIT(24)
#define AN7581_QDMA_GLOBAL_CFG_RESET            HW_BIT(23)
#define AN7581_QDMA_GLOBAL_CFG_RESET_DONE       HW_BIT(22)
#define AN7581_QDMA_GLOBAL_CFG_MULTICAST_EN     HW_BIT(21)
#define AN7581_QDMA_GLOBAL_CFG_IRQ1_EN          HW_BIT(20)
#define AN7581_QDMA_GLOBAL_CFG_IRQ0_EN          HW_BIT(19)
#define AN7581_QDMA_GLOBAL_CFG_LOOPCNT_EN       HW_BIT(18)
#define AN7581_QDMA_GLOBAL_CFG_RD_BYPASS_WR     HW_BIT(17)
#define AN7581_QDMA_GLOBAL_CFG_QDMA_LOOPBACK    HW_BIT(16)
#define AN7581_QDMA_GLOBAL_CFG_LPBK_RXQ_SEL     HW_GENMASK(13, 8)
#define AN7581_QDMA_GLOBAL_CFG_CHECK_DONE       HW_BIT(7)
#define AN7581_QDMA_GLOBAL_CFG_TX_WB_DONE       HW_BIT(6)
#define AN7581_QDMA_GLOBAL_CFG_MAX_ISSUE_NUM    HW_GENMASK(5, 4)
#define AN7581_QDMA_GLOBAL_CFG_RX_DMA_BUSY      HW_BIT(3)
#define AN7581_QDMA_GLOBAL_CFG_RX_DMA_EN        HW_BIT(2)
#define AN7581_QDMA_GLOBAL_CFG_TX_DMA_BUSY      HW_BIT(1)
#define AN7581_QDMA_GLOBAL_CFG_TX_DMA_EN        HW_BIT(0)
#define AN7581_QDMA_GLOBAL_CFG_OBSERVED         0x76380075u

/* Hardware-forwarding descriptor and buffer pools (in the QDMAn reserved
 * DRAM regions, memmap.h). */
#define AN7581_QDMA_FWD_DSCP_BASE       0x010u
#define AN7581_QDMA_FWD_BUF_BASE        0x014u
#define AN7581_QDMA_HW_FWD_DSCP_CFG     0x018u
#define AN7581_QDMA_HW_FWD_DSCP_CFG_PAYLOAD_SIZE    HW_GENMASK(29, 28)
#define AN7581_QDMA_HW_FWD_DSCP_CFG_SCATTER_LEN     HW_GENMASK(17, 16)
#define AN7581_QDMA_HW_FWD_DSCP_CFG_MIN_SCATTER_LEN HW_GENMASK(15, 0)

/* Interrupt status (W1C) and per-bank enable, five register groups 0..4. */
#define AN7581_QDMA_INT_STATUS(n)       ((n) == 4 ? 0x730u : (n) == 3 ? 0x724u : \
                                         (n) == 2 ? 0x720u : (n) == 1 ? 0x024u : 0x020u)
#define AN7581_QDMA_INT_ENABLE(bank, n) ((n) == 4 ? 0x750u + 0x20u * (bank) : \
                                         (n) == 3 ? 0x744u + 0x20u * (bank) : \
                                         (n) == 2 ? 0x740u + 0x20u * (bank) : \
                                         (n) == 1 ? 0x02cu + 0x8u * (bank) : \
                                                    0x028u + 0x8u * (bank))
/* Group 0 bits. */
#define AN7581_QDMA_INT0_RX_COHERENT(q) HW_BIT(16 + (q))   /* RX ring q = 0..15 */
#define AN7581_QDMA_INT0_TX_COHERENT(q) HW_BIT(8 + (q))    /* TX ring q = 0..7 */
#define AN7581_QDMA_INT0_CNT_OVERFLOW   HW_BIT(7)
#define AN7581_QDMA_INT0_IRQ1_FULL      HW_BIT(5)
#define AN7581_QDMA_INT0_IRQ1           HW_BIT(4)          /* TX-done ring 1 */
#define AN7581_QDMA_INT0_HWFWD_DSCP_LOW HW_BIT(3)
#define AN7581_QDMA_INT0_HWFWD_DSCP_EMPTY HW_BIT(2)
#define AN7581_QDMA_INT0_IRQ0_FULL      HW_BIT(1)
#define AN7581_QDMA_INT0_IRQ0           HW_BIT(0)          /* TX-done ring 0 */
/* Group 1: RX rings 0..15; group 2: RX rings 16..31 (ring - 16). */
#define AN7581_QDMA_INT12_RX_NO_CPU_DSCP(q) HW_BIT(16 + ((q) & 15u))
#define AN7581_QDMA_INT12_RX_DONE(q)    HW_BIT((q) & 15u)
/* Group 3: RX rings 16..31 coherent; group 4: TX rings 8..31 coherent. */
#define AN7581_QDMA_INT3_RX_COHERENT(q) HW_BIT(16 + ((q) & 15u))
#define AN7581_QDMA_INT4_TX_COHERENT(q) HW_BIT(q)          /* q = 8..31 */

/* TX-completion ("IRQ") rings 0/1: DRAM ring of completion entries. */
#define AN7581_QDMA_TX_IRQ_BASE(n)      ((n) ? 0x048u : 0x050u)
#define AN7581_QDMA_TX_IRQ_CFG(n)       ((n) ? 0x04cu : 0x054u)
#define AN7581_QDMA_TX_IRQ_CFG_THR      HW_GENMASK(27, 16)
#define AN7581_QDMA_TX_IRQ_CFG_DEPTH    HW_GENMASK(11, 0)
#define AN7581_QDMA_IRQ_CLEAR_LEN(n)    ((n) ? 0x064u : 0x058u)
#define AN7581_QDMA_IRQ_CLEAR_LEN_LEN   HW_GENMASK(7, 0)
#define AN7581_QDMA_IRQ_STATUS(n)       ((n) ? 0x068u : 0x05cu)
#define AN7581_QDMA_IRQ_STATUS_ENTRY_LEN HW_GENMASK(27, 16)
#define AN7581_QDMA_IRQ_STATUS_HEAD_IDX HW_GENMASK(11, 0)
/* One completion entry. */
#define AN7581_QDMA_IRQ_ENTRY_RING      HW_GENMASK(20, 16)
#define AN7581_QDMA_IRQ_ENTRY_DESC      HW_GENMASK(15, 0)

#define AN7581_QDMA_INGRESS_TRTCM_CFG   0x070u

/* Per-channel TX queue close: bit (queue + 8 * (channel % 4)). */
#define AN7581_QDMA_QUEUE_CLOSE_CFG(ch) (0x0a0u + ((ch) & 0xfcu))
#define AN7581_QDMA_QUEUE_CLOSE_BIT(ch, q) HW_BIT((q) + 8 * ((ch) & 3u))

/* TX rings 0..31 (mainline: 1024 descriptors each). Doorbell = CPU_IDX. */
#define AN7581_QDMA_TX_RING(n)          ((n) < 8 ? 0x100u + 0x20u * (n) : 0xb00u + 0x20u * ((n) - 8))
#define AN7581_QDMA_TX_RING_BASE        0x00u
#define AN7581_QDMA_TX_RING_BLOCKING    0x04u
#define AN7581_QDMA_TX_RING_BLOCKING_IRQ_MAP     HW_BIT(6)
#define AN7581_QDMA_TX_RING_BLOCKING_IRQ_CFG     HW_BIT(4)
#define AN7581_QDMA_TX_RING_BLOCKING_TX_DROP_EN  HW_BIT(2)
#define AN7581_QDMA_TX_RING_BLOCKING_MAX_TH_EN   HW_BIT(1)
#define AN7581_QDMA_TX_RING_BLOCKING_MIN_TH_EN   HW_BIT(0)
#define AN7581_QDMA_TX_RING_CPU_IDX     0x08u
#define AN7581_QDMA_TX_RING_DMA_IDX     0x0cu
#define AN7581_QDMA_RING_IDX            HW_GENMASK(15, 0)

/* RX rings 0..31. */
#define AN7581_QDMA_RX_RING(n)          ((n) < 16 ? 0x200u + 0x20u * (n) : 0xe00u + 0x20u * ((n) - 16))
#define AN7581_QDMA_RX_RING_BASE        0x00u
#define AN7581_QDMA_RX_RING_SIZE        0x04u
#define AN7581_QDMA_RX_RING_SIZE_THR    HW_GENMASK(31, 16)
#define AN7581_QDMA_RX_RING_SIZE_SIZE   HW_GENMASK(15, 0)
#define AN7581_QDMA_RX_RING_CPU_IDX     0x08u
#define AN7581_QDMA_RX_RING_DMA_IDX     0x0cu
#define AN7581_QDMA_RX_RING_DELAY_INT   0x10u
#define AN7581_QDMA_RX_RING_SCATTER_CFG 0x14u
#define AN7581_QDMA_RX_RING_SCATTER_CFG_SG_EN HW_BIT(0)

/* Statistic counters. */
#define AN7581_QDMA_CNTR_CFG(n)         (0x400u + 0x8u * (n))
#define AN7581_QDMA_CNTR_CFG_EN         HW_BIT(31)
#define AN7581_QDMA_CNTR_CFG_ALL_CHAN   HW_BIT(30)
#define AN7581_QDMA_CNTR_CFG_ALL_QUEUE  HW_BIT(29)
#define AN7581_QDMA_CNTR_CFG_ALL_DSCP_RING HW_BIT(28)
#define AN7581_QDMA_CNTR_CFG_SRC        HW_GENMASK(27, 24)
#define AN7581_QDMA_CNTR_CFG_DSCP_RING  HW_GENMASK(20, 16)
#define AN7581_QDMA_CNTR_CFG_CHAN       HW_GENMASK(7, 3)
#define AN7581_QDMA_CNTR_CFG_QUEUE      HW_GENMASK(2, 0)
#define AN7581_QDMA_CNTR_VAL(n)         (0x404u + 0x8u * (n))

/* Link manager: hardware-forwarding descriptor pool init. */
#define AN7581_QDMA_LMGR_INIT_CFG       0x1000u
#define AN7581_QDMA_LMGR_INIT_CFG_START HW_BIT(31)
#define AN7581_QDMA_LMGR_INIT_CFG_SRAM_MODE HW_BIT(30)
#define AN7581_QDMA_LMGR_INIT_CFG_PKTSIZE_OVERHEAD HW_GENMASK(27, 20)
#define AN7581_QDMA_LMGR_INIT_CFG_DESC_NUM HW_GENMASK(16, 0)
#define AN7581_QDMA_FWD_DSCP_LOW_THR    0x1004u
#define AN7581_QDMA_FWD_DSCP_LOW_THR_THR HW_GENMASK(17, 0)

/* Shapers/meters (trTCM blocks: +0 cfg, +4 param, +8 data low, +0xc high). */
#define AN7581_QDMA_EGRESS_RATE_METER_CFG 0x100cu
#define AN7581_QDMA_EGRESS_TRTCM_CFG    0x1010u
#define AN7581_QDMA_GLB_TRTCM_CFG       0x1080u
#define AN7581_QDMA_SLA_TRTCM_CFG       0x1150u
#define AN7581_QDMA_TRTCM_CFG_EN        HW_BIT(31)
#define AN7581_QDMA_TRTCM_CFG_MODE      HW_BIT(30)
#define AN7581_QDMA_TRTCM_CFG_SLOW_TICK_RATIO HW_GENMASK(29, 16)
#define AN7581_QDMA_TRTCM_CFG_FAST_TICK HW_GENMASK(15, 0)
#define AN7581_QDMA_TRTCM_PARAM         0x4u
#define AN7581_QDMA_TRTCM_DATA_LOW      0x8u
#define AN7581_QDMA_TRTCM_DATA_HIGH     0xcu

/* Weighted round robin between the 8 queues of a channel. */
#define AN7581_QDMA_TXWRR_MODE_CFG      0x1020u
#define AN7581_QDMA_TXWRR_MODE_CFG_WEIGHT_SCALE HW_BIT(31)
#define AN7581_QDMA_TXWRR_MODE_CFG_WEIGHT_BASE  HW_BIT(3)
#define AN7581_QDMA_TXWRR_WEIGHT_CFG    0x1024u
#define AN7581_QDMA_TXWRR_WEIGHT_CFG_RW_CMD   HW_BIT(31)
#define AN7581_QDMA_TXWRR_WEIGHT_CFG_DONE     HW_BIT(30)
#define AN7581_QDMA_TXWRR_WEIGHT_CFG_CHAN     HW_GENMASK(23, 19)
#define AN7581_QDMA_TXWRR_WEIGHT_CFG_QUEUE    HW_GENMASK(18, 16)
#define AN7581_QDMA_TXWRR_WEIGHT_CFG_VALUE    HW_GENMASK(15, 0)

#define AN7581_QDMA_PSE_BUF_USAGE_CFG   0x1028u
#define AN7581_QDMA_PSE_BUF_USAGE_CFG_ESTIMATE_EN HW_BIT(29)

/* Scheduling mode per channel (8 channels per register, 3-bit field at
 * 4 * (ch % 8)). Values: 0 WRR over 8 queues, 1 strict priority, 2..7 =
 * (mode - 1) strict-priority queues above WRR over the rest. */
#define AN7581_QDMA_CHAN_QOS_MODE(ch)   (0x1040u + 0x4u * ((ch) >> 3))
#define AN7581_QDMA_CHAN_QOS_MODE_FIELD(ch) HW_GENMASK(2 + 4 * ((ch) & 7u), 4 * ((ch) & 7u))
#define AN7581_QDMA_QOS_MODE_WRR8       0u
#define AN7581_QDMA_QOS_MODE_SP         1u

/*
 * TX-queue congestion handling. RW.
 * HAZARD (data loss): with DROP_EN set (mainline default) a congested TX
 *   queue drops frames between the driver and the GDM, counted nowhere: at
 *   4.6x line rate 78% of frames vanished [measured]. Cleared, the ring
 *   fills and the netdev queue stops instead (backpressure); 200,000 of
 *   200,000 frames left at 9.97 Gb/s [measured].
 * Observed: 0xe00700fa with both drop bits set, 0x200700fa with both
 * cleared [measured]. Bit 29, bits 18:16 (7) and 7:0 (0xfa) undecoded.
 */
#define AN7581_QDMA_TXQ_CNGST_CFG       0x10a0u
#define AN7581_QDMA_TXQ_CNGST_CFG_DROP_EN     HW_BIT(31)
#define AN7581_QDMA_TXQ_CNGST_CFG_DEI_DROP_EN HW_BIT(30)
#define AN7581_QDMA_TXQ_CNGST_CFG_DROP_ON  0xe00700fau
#define AN7581_QDMA_TXQ_CNGST_CFG_DROP_OFF 0x200700fau

/* ======================================================================== */
/* QDMA descriptors (32 bytes, little-endian words, in DRAM)                */
/* ======================================================================== */

#define AN7581_QDMA_DESC_BYTES          32u
#define AN7581_QDMA_DESC_TCP_TS         0x00u
#define AN7581_QDMA_DESC_CTRL           0x04u
#define AN7581_QDMA_DESC_ADDR           0x08u   /* 32-bit bus address */
#define AN7581_QDMA_DESC_DATA           0x0cu
#define AN7581_QDMA_DESC_MSG0           0x10u
#define AN7581_QDMA_DESC_MSG1           0x14u
#define AN7581_QDMA_DESC_MSG2           0x18u
#define AN7581_QDMA_DESC_MSG3           0x1cu

#define AN7581_QDMA_DESC_CTRL_DONE      HW_BIT(31)
#define AN7581_QDMA_DESC_CTRL_DROP      HW_BIT(30) /* TX: drop; RX: overflow */
#define AN7581_QDMA_DESC_CTRL_MORE      HW_BIT(29) /* scatter-gather continues */
#define AN7581_QDMA_DESC_CTRL_DEI       HW_BIT(25)
#define AN7581_QDMA_DESC_CTRL_NO_DROP   HW_BIT(24)
#define AN7581_QDMA_DESC_CTRL_LEN       HW_GENMASK(15, 0)
#define AN7581_QDMA_DESC_DATA_NEXT_ID   HW_GENMASK(15, 0)

/* TX MSG0. */
#define AN7581_QDMA_TXMSG0_MIC_IDX      HW_BIT(30)
#define AN7581_QDMA_TXMSG0_SP_TAG       HW_GENMASK(29, 14)
#define AN7581_QDMA_TXMSG0_ICO          HW_BIT(13) /* IPv4 checksum offload */
#define AN7581_QDMA_TXMSG0_UCO          HW_BIT(12) /* UDP checksum offload */
#define AN7581_QDMA_TXMSG0_TCO          HW_BIT(11) /* TCP checksum offload */
#define AN7581_QDMA_TXMSG0_TSO          HW_BIT(10)
#define AN7581_QDMA_TXMSG0_FAST         HW_BIT(9)
#define AN7581_QDMA_TXMSG0_OAM          HW_BIT(8)
#define AN7581_QDMA_TXMSG0_CHAN         HW_GENMASK(7, 3)
#define AN7581_QDMA_TXMSG0_QUEUE        HW_GENMASK(2, 0)
/* TX MSG1. */
#define AN7581_QDMA_TXMSG1_NO_DROP      HW_BIT(31)
#define AN7581_QDMA_TXMSG1_METER        HW_GENMASK(30, 24) /* 0x7f: no meter */
#define AN7581_QDMA_TXMSG1_FPORT        HW_GENMASK(23, 20) /* destination PSE port */
#define AN7581_QDMA_TXMSG1_NBOQ         HW_GENMASK(19, 15)
#define AN7581_QDMA_TXMSG1_HWF          HW_BIT(14)
#define AN7581_QDMA_TXMSG1_HOP          HW_BIT(13)
#define AN7581_QDMA_TXMSG1_PTP          HW_BIT(12)
#define AN7581_QDMA_TXMSG1_ACNT_G1      HW_GENMASK(10, 6)  /* 0x1f: not counted */
#define AN7581_QDMA_TXMSG1_ACNT_G0      HW_GENMASK(5, 0)   /* 0x3f: not counted */
/* RX MSG0..3. */
#define AN7581_QDMA_RXMSG0_SPTAG        HW_GENMASK(21, 14)
#define AN7581_QDMA_RXMSG1_DEI          HW_BIT(31)
#define AN7581_QDMA_RXMSG1_IP6          HW_BIT(30)
#define AN7581_QDMA_RXMSG1_IP4          HW_BIT(29)
#define AN7581_QDMA_RXMSG1_IP4F         HW_BIT(28)
#define AN7581_QDMA_RXMSG1_L4_VALID     HW_BIT(27)
#define AN7581_QDMA_RXMSG1_L4F          HW_BIT(26)
#define AN7581_QDMA_RXMSG1_SPORT        HW_GENMASK(25, 21)
#define AN7581_QDMA_RXMSG1_CRSN         HW_GENMASK(20, 16) /* CPU reason */
#define AN7581_QDMA_RXMSG1_PPE_ENTRY    HW_GENMASK(15, 0)
#define AN7581_QDMA_RXMSG2_AGG_COUNT    HW_GENMASK(31, 24)
#define AN7581_QDMA_RXMSG2_L2_LEN       HW_GENMASK(6, 0)
#define AN7581_QDMA_RXMSG3_AGG_LEN      HW_GENMASK(31, 16)
#define AN7581_QDMA_RXMSG3_TCP_WIN      HW_GENMASK(15, 0)

/* Hardware-forwarding descriptor (32 bytes: addr, ctrl0..2, msg0..1, rsv). */
#define AN7581_QDMA_FWD_DESC_CTRL0_CTX  HW_BIT(31)
#define AN7581_QDMA_FWD_DESC_CTRL0_RING HW_GENMASK(30, 28)
#define AN7581_QDMA_FWD_DESC_CTRL0_IDX  HW_GENMASK(27, 16)
#define AN7581_QDMA_FWD_DESC_CTRL0_LEN  HW_GENMASK(15, 0)

/* Mainline allocation [static]. */
#define AN7581_QDMA_NUM_TX_RING         32u
#define AN7581_QDMA_NUM_RX_RING         32u
#define AN7581_QDMA_TX_RING_DESCS       1024u
#define AN7581_QDMA_NUM_IRQ_BANKS       4u
#define AN7581_QDMA_NUM_QOS_CHANNELS    4u
#define AN7581_QDMA_NUM_QOS_QUEUES      8u
#define AN7581_ETH_MAX_MTU              9220u

/* ======================================================================== */
/* SerDes PCS blocks [static]                                               */
/* ======================================================================== */

/* Serdes sub-block bases; add the AN7581_PCS_SUB_* offset. The PCIe PCS
 * shares its ANA/PMA pages with the PCIe PHY block (memmap.h). */
#define AN7581_PCS_PCIE0_SERDES         0x1fa50000u
#define AN7581_PCS_PCIE1_SERDES         0x1fa60000u
#define AN7581_PCS_ETH_SERDES           0x1fa70000u /* GDM4 */
#define AN7581_PCS_PON_SERDES           0x1fa80000u /* GDM2 */
#define AN7581_PCS_USB_SERDES           0x1fa90000u
#define AN7581_PCS_SUB_HSGMII_AN        0x0000u /* 0x60 bytes */
#define AN7581_PCS_SUB_HSGMII_PCS       0x0a00u /* 0x164 */
#define AN7581_PCS_SUB_MULTI_SGMII      0x4000u /* 0x450 */
#define AN7581_PCS_SUB_USXGMII          0x5900u /* 0x338 */
#define AN7581_PCS_SUB_HSGMII_RATE_ADP  0x6000u /* 0x300 */
#define AN7581_PCS_SUB_ANA              0xa000u /* 0x1000 (USB: 0x600) */
#define AN7581_PCS_SUB_PMA              0xb000u /* 0x1000 */

/* XFI MAC registers (offsets from AN7581_PCS_PON_MAC_BASE / _ETH_MAC_BASE). */
#define AN7581_PCS_XFI_GIB_CFG          0x000u
#define AN7581_PCS_XFI_GIB_CFG_RX_FRAG_LEN HW_GENMASK(26, 22)
#define AN7581_PCS_XFI_GIB_CFG_TX_FRAG_LEN HW_GENMASK(21, 17)
#define AN7581_PCS_XFI_GIB_CFG_IPG_NUM  HW_GENMASK(15, 10)
#define AN7581_PCS_XFI_GIB_CFG_TX_FC_EN HW_BIT(5)
#define AN7581_PCS_XFI_GIB_CFG_RX_FC_EN HW_BIT(4)
#define AN7581_PCS_XFI_GIB_CFG_RXMPI_STOP HW_BIT(3)
#define AN7581_PCS_XFI_GIB_CFG_RXMBI_STOP HW_BIT(2)
#define AN7581_PCS_XFI_GIB_CFG_TXMPI_STOP HW_BIT(1)
#define AN7581_PCS_XFI_GIB_CFG_TXMBI_STOP HW_BIT(0)
#define AN7581_PCS_XFI_LOGIC_RST        0x010u
#define AN7581_PCS_XFI_LOGIC_RST_MAC    HW_BIT(0)
#define AN7581_PCS_XFI_MACADDRH         0x060u /* [15:0] */
#define AN7581_PCS_XFI_MACADDRL         0x064u
#define AN7581_PCS_XFI_CNT_CLR          0x100u
#define AN7581_PCS_XFI_CNT_CLR_GLB      HW_BIT(0)

#endif /* HW_AN7581_ETH_H */
