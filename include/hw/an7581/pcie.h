/* SPDX-License-Identifier: MIT */
/*
 * AN7581 PCIe root complexes (MediaTek Gen3 MAC, Airoha variant) and the
 * W1700K's two PCIe links to the MT7996.
 *
 * ADDRESS SPACE:
 *   AN7581_PCIE_MAC_*   offsets relative to AN7581_PCIE_MAC_BASE(n) (memmap.h)
 *   AN7581_PBUS_*       offsets relative to AN7581_PBUS_CSR_BASE (memmap.h)
 *   AN7581_PCIE_CFG_*   PCI configuration-space offsets/values (standard
 *                       PCIe capability fields) of the root port or endpoint
 * Refclk, PERST# and the x2 lane mux are SCU bits (scu.h: AN7581_SCU_PCIC_*);
 * PHY-lane resets are SCU RST_CTRL1/RST_CTRL2 bits.
 *
 * Topology on the W1700K [measured]:
 *   domain 0: PCIe0 MAC + PCIe1 MAC as one x2 port (PCIe1 controller
 *             disabled), root port 0000:00:00.0, MT7996 primary function
 *             0000:01:00.0 (14c3:7990), BAR0 0x20000000..0x201fffff.
 *             Link: Gen3 x2.
 *   domain 2: PCIe2 MAC over the USB1 PHY's serdes, root port 0002:00:00.0
 *             (14c3:6899), MT7996 second host interface 0002:01:00.0
 *             (14c3:7991), BAR0 0x28000000, 2 MiB. Link: Gen2 x1 (from a
 *             stock boot record; not re-measured on this firmware).
 *   Domain 1 does not exist (PCIe1 disabled). Bus/device/function 00:00.0
 *   exists in both domains: name the domain explicitly in tools.
 *
 * DMA [static + measured]: inbound PCIe writes are not cache-coherent (no
 * dma-coherent on the PCIe nodes, no CCI). Every received buffer must be
 * invalidated before the CPU reads it. PCIe writes cannot target any CPU cache.
 *
 * HAZARD: an MMIO read the endpoint never completes can hard-lock the A53
 *   host (no ping, no panic reboot, nothing logged by AER) although both ends
 *   have completion timeout enabled [measured]. Only the SoC watchdog or a
 *   power cycle recovers the host; an MT7996 wedged this way survives FLR,
 *   secondary bus reset and PERST# and needs a power cycle [measured].
 * HAZARD: an inbound (device-mastered) write to an address the SoC fabric does
 *   not route completes without error on the device side, lands nowhere, and
 *   left the MT7996 command path dead until its reset [measured].
 */
#ifndef HW_AN7581_PCIE_H
#define HW_AN7581_PCIE_H

#include "../hw_bits.h"

/* ======================================================================== */
/* Link facts                                                               */
/* ======================================================================== */

/*
 * HAZARD (throughput): ASPM L0s+L1 is enabled by default on both ends of the
 * primary link and cuts MT7996-to-DRAM DMA to 0.32-0.33 GB/s. With ASPM off
 * the same transfer runs at 1.44 GB/s [measured]. ASPM must be off for
 * streaming; through Linux sysfs (link/l0s_aspm, link/l1_aspm = 0 on
 * 0000:01:00.0) the change is runtime-only and reverts at reboot.
 *
 * Max Payload Size is fixed at 128 B: the root port supports only 128 B
 * (Device Capabilities 0x00008020) while the MT7996 supports 256 B
 * (0x10008fe1) [measured]. With 128-byte TLPs the Gen3 x2 ceiling is about
 * 1.66 GB/s; 1.44 GB/s is ~87% of it [measured/derived].
 */
#define AN7581_PCIE_W1700K_PRIMARY_DOMAIN   0u
#define AN7581_PCIE_W1700K_SECONDARY_DOMAIN 2u
#define AN7581_PCIE_ROOT_PORT_DEVICE_ID     0x6899u /* vendor 0x14c3; seen on 0002:00:00.0 */
#define AN7581_PCIE_MPS_BYTES               128u
#define AN7581_PCIE_PRIMARY_DMA_BPS_ASPM_OFF 1440000000u /* measured */

/* ======================================================================== */
/* PCI configuration space (root port 0000:00:00.0 unless stated)           */
/* ======================================================================== */

/* Device Capabilities, observed value [measured]. MPSS = 0: 128 B. */
#define AN7581_PCIE_CFG_RP_DEVCAP_VAL       0x00008020u
#define AN7581_PCIE_CFG_DEVCAP_MPSS         HW_GENMASK(2, 0)

/* Device Capabilities 2 / Device Control 2, observed on root port and
 * endpoint [measured]: completion timeout supported and enabled at the
 * default 50 us..50 ms range. */
#define AN7581_PCIE_CFG_DEVCAP2_VAL         0x0010081fu
#define AN7581_PCIE_CFG_DEVCTL2_VAL         0x0400u

/* Link Control: ASPM control field [static, PCIe base spec] and observed
 * values [measured]: root port 0x0c43 / endpoint 0x0043 with ASPM on,
 * 0x0c40 / 0x0040 after disabling it. */
#define AN7581_PCIE_CFG_LNKCTL_ASPMC        HW_GENMASK(1, 0)
#define AN7581_PCIE_CFG_LNKCTL_ASPMC_L0S    1u
#define AN7581_PCIE_CFG_LNKCTL_ASPMC_L1     2u
#define AN7581_PCIE_CFG_RP_LNKCTL_ASPM_ON   0x0c43u
#define AN7581_PCIE_CFG_RP_LNKCTL_ASPM_OFF  0x0c40u

/* Extended capabilities [measured]: AER (id 0x0001, version 2) at 0x200 on
 * both root port and MT7996; Secondary PCIe (id 0x0019) present on both; no
 * DPC, no ECRC. */
#define AN7581_PCIE_CFG_AER_OFFSET          0x200u

/* ======================================================================== */
/* MAC registers (offsets from AN7581_PCIE_MAC_BASE(n)) [static]            */
/* ======================================================================== */

#define AN7581_PCIE_MAC_BASE_CFG            0x014u
#define AN7581_PCIE_MAC_BASE_CFG_SPEED      HW_GENMASK(15, 8)

/* Port setting: RC mode, lane width, supported generations. */
#define AN7581_PCIE_MAC_SETTING             0x080u
#define AN7581_PCIE_MAC_SETTING_GEN_SUPPORT HW_GENMASK(14, 12) /* bit k: Gen(k+2) */
#define AN7581_PCIE_MAC_SETTING_LINK_WIDTH  HW_GENMASK(11, 8)  /* 0 = x1, bit0 = x2 */
#define AN7581_PCIE_MAC_SETTING_RC_MODE     HW_BIT(0)

/* Class code presented by the root port, class << 8. */
#define AN7581_PCIE_MAC_PCI_IDS_1           0x09cu

/* Per-lane equalisation presets; Airoha writes 0x41474147 to each lane's MAC
 * (LN0/LN1 downstream 0x47, upstream 0x41). Lane 1 of an x2 port is
 * programmed through PCIe1's MAC. */
#define AN7581_PCIE_MAC_EQ_PRESET_01        0x100u
#define AN7581_PCIE_MAC_EQ_PRESET_01_LN1_UP   HW_GENMASK(30, 24)
#define AN7581_PCIE_MAC_EQ_PRESET_01_LN1_DOWN HW_GENMASK(22, 16)
#define AN7581_PCIE_MAC_EQ_PRESET_01_LN0_UP   HW_GENMASK(14, 8)
#define AN7581_PCIE_MAC_EQ_PRESET_01_LN0_DOWN HW_GENMASK(6, 0)
#define AN7581_PCIE_MAC_EQ_PRESET_01_AIROHA 0x41474147u

/* Config-request target for the window at +0x1000. */
#define AN7581_PCIE_MAC_CFGNUM              0x140u
#define AN7581_PCIE_MAC_CFGNUM_FORCE_BYTE_EN HW_BIT(20)
#define AN7581_PCIE_MAC_CFGNUM_BYTE_EN      HW_GENMASK(19, 16)
#define AN7581_PCIE_MAC_CFGNUM_BUS          HW_GENMASK(15, 8)
#define AN7581_PCIE_MAC_CFGNUM_DEVFN        HW_GENMASK(7, 0)

/* Internal resets, active low (1 = released). PE_RSTB is not used to drive
 * PERST# on the AN7581; PERST# comes from the SCU PERSTOUT bits. */
#define AN7581_PCIE_MAC_RST_CTRL            0x148u
#define AN7581_PCIE_MAC_RST_CTRL_PE_RSTB    HW_BIT(3)
#define AN7581_PCIE_MAC_RST_CTRL_BRG_RSTB   HW_BIT(2)
#define AN7581_PCIE_MAC_RST_CTRL_PHY_RSTB   HW_BIT(1)
#define AN7581_PCIE_MAC_RST_CTRL_MAC_RSTB   HW_BIT(0)

#define AN7581_PCIE_MAC_LTSSM_STATUS        0x150u
#define AN7581_PCIE_MAC_LTSSM_STATUS_STATE  HW_GENMASK(28, 24)
#define AN7581_PCIE_MAC_LTSSM_STATE_L2_IDLE 0x14u

#define AN7581_PCIE_MAC_LINK_STATUS         0x154u
#define AN7581_PCIE_MAC_LINK_STATUS_LINKUP  HW_BIT(8)

#define AN7581_PCIE_MAC_INT_ENABLE          0x180u
#define AN7581_PCIE_MAC_INT_ENABLE_INTX     HW_GENMASK(27, 24)
#define AN7581_PCIE_MAC_INT_ENABLE_MSI      HW_GENMASK(15, 8)  /* one bit per MSI set */
#define AN7581_PCIE_MAC_INT_STATUS          0x184u             /* same layout, W1C */
#define AN7581_PCIE_MAC_MSI_SET_ENABLE      0x190u
#define AN7581_PCIE_MAC_MSI_SET_ENABLE_SETS HW_GENMASK(7, 0)

#define AN7581_PCIE_MAC_ICMD_PM             0x198u
#define AN7581_PCIE_MAC_ICMD_PM_TURN_OFF_LINK HW_BIT(4)

/* Gen3 equalisation request. Airoha value 0x1018020f: query + timeout,
 * 16G preset 0x80, preset 0x2, fine-tune max 0xf. */
#define AN7581_PCIE_MAC_PIPE4_PIE8          0x338u
#define AN7581_PCIE_MAC_PIPE4_PIE8_PRESET_16G HW_GENMASK(31, 21)
#define AN7581_PCIE_MAC_PIPE4_PIE8_QUERY_TIMEOUT HW_BIT(20)
#define AN7581_PCIE_MAC_PIPE4_PIE8_PHYPARAM_QUERY HW_BIT(19)
#define AN7581_PCIE_MAC_PIPE4_PIE8_PRESET   HW_GENMASK(18, 8)
#define AN7581_PCIE_MAC_PIPE4_PIE8_FINETUNE_ERR HW_GENMASK(7, 6)
#define AN7581_PCIE_MAC_PIPE4_PIE8_FINETUNE_MAX HW_GENMASK(5, 0)
#define AN7581_PCIE_MAC_PIPE4_PIE8_AIROHA   0x1018020fu

#define AN7581_PCIE_MAC_MISC_CTRL           0x348u
#define AN7581_PCIE_MAC_MISC_CTRL_DISABLE_DVFSRC_VLT_REQ HW_BIT(1)

/*
 * Outbound address-translation tables: 8 sets of 0x20 bytes from +0x800.
 * Word 0: CPU address [31:12] | SIZE | EN, SIZE = log2(window bytes) - 1,
 * minimum window 4 KiB. The W1700K windows are identity-mapped 64 MiB each.
 */
#define AN7581_PCIE_MAC_ATR(n)              (0x800u + 0x20u * (n))
#define AN7581_PCIE_MAC_ATR_COUNT           8u
#define AN7581_PCIE_MAC_ATR_SRC_LSB         0x00u
#define AN7581_PCIE_MAC_ATR_SRC_LSB_SIZE    HW_GENMASK(6, 1)
#define AN7581_PCIE_MAC_ATR_SRC_LSB_EN      HW_BIT(0)
#define AN7581_PCIE_MAC_ATR_SRC_MSB         0x04u
#define AN7581_PCIE_MAC_ATR_TRSL_LSB        0x08u
#define AN7581_PCIE_MAC_ATR_TRSL_MSB        0x0cu
#define AN7581_PCIE_MAC_ATR_TRSL_PARAM      0x10u
#define AN7581_PCIE_MAC_ATR_TRSL_PARAM_TLP_TYPE HW_GENMASK(18, 16) /* 0 mem, 2 I/O */
#define AN7581_PCIE_MAC_ATR_TRSL_PARAM_ID   HW_GENMASK(3, 0)       /* 0 mem, 1 I/O */

/* MSI: 8 sets of 32 vectors. */
#define AN7581_PCIE_MAC_MSI_SET(n)          (0xc00u + 0x10u * (n)) /* +0 address */
#define AN7581_PCIE_MAC_MSI_SET_STATUS      0x04u
#define AN7581_PCIE_MAC_MSI_SET_ENABLE_OFS  0x08u
#define AN7581_PCIE_MAC_MSI_SET_ADDR_HI(n)  (0xc80u + 0x4u * (n))

#define AN7581_PCIE_MAC_RESOURCE_CTRL       0xd2cu
#define AN7581_PCIE_MAC_RESOURCE_CTRL_SYS_CLK_RDY_TIME HW_GENMASK(7, 0)

/* Configuration window: the request selected by CFGNUM, or the root port's
 * own config space. The root port's PCIe capability sits at config 0x80
 * [inferred from the Link Control 2 address below]. */
#define AN7581_PCIE_MAC_CFG_WINDOW          0x1000u
#define AN7581_PCIE_MAC_CONF_LINK2_CTL_STS  0x10b0u /* root-port Link Control 2 */
#define AN7581_PCIE_MAC_CONF_LINK2_SPEED    HW_GENMASK(3, 0)

/* ======================================================================== */
/* PBus CSR: address decode per port (offsets from AN7581_PBUS_CSR_BASE)    */
/* ======================================================================== */

/*
 * The fabric forwards a CPU access to port n when (addr & MASK) == BASE.
 * BASE = window base; MASK = GENMASK(31, log2(window size)), 0xfc000000 for
 * the 64 MiB windows [static].
 */
#define AN7581_PBUS_PCIE_BASE(n)            (0x08u * (n))
#define AN7581_PBUS_PCIE_MASK(n)            (0x08u * (n) + 0x04u)
#define AN7581_PBUS_PCIE_MASK_64M           0xfc000000u

/* ======================================================================== */
/* Bring-up sequence (Airoha, x1 or x2) [static]                            */
/* ======================================================================== */

/*
 * 1. Assert the PHY-lane resets (SCU RST1 PCIE0/PCIE1 or RST2 PCIE2), wait
 *    100 ms.
 * 2. Program the PBus BASE/MASK for the port's window.
 * 3. Assert PERSTOUT (SCU PCIC bit = 0) for the port (and its second lane).
 * 4. x2 only: SCU PCIC.PCIE_CTRL = 2.
 * 5. PHY init and power-on, then release the PHY-lane resets, wait 100 ms.
 * 6. Enable the refclk (SCU PCIC REFCLK_EN0|EN1), wait T_PVPERL (100 ms).
 * 7. Per lane MAC: EQ_PRESET_01 = 0x41474147, PIPE4_PIE8 = 0x1018020f.
 * 8. Release PERSTOUT, then train (SETTING: RC mode, width, generations).
 */
#define AN7581_PCIE_RESET_WAIT_MS           100u

#endif /* HW_AN7581_PCIE_H */
