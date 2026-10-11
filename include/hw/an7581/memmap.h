/* SPDX-License-Identifier: MIT */
/*
 * Airoha AN7581 (EN7581 family) SoC: physical memory map, interrupts, CPU and
 * DRAM facts, as populated on the Gemtek W1700K.
 *
 * ADDRESS SPACE: AN7581 physical addresses as the Cortex-A53 cluster sees
 * them. Every block below sits under 4 GiB. Sizes are the register-resource
 * lengths the drivers map, not necessarily the decode width. Block-internal
 * offsets are in the per-block headers (scu.h, pcie.h, eth.h, npu.h, snfi.h).
 * Other views: the NPU cores address their own RAMs and DRAM through a
 * different map (npu.h); PCI config space is in pcie.h; NAND offsets are in
 * snfi.h and w1700k_board.h.
 *
 * Interrupts: AN7581_IRQ_* are GIC SPI numbers (GIC-v3 INTID = SPI + 32).
 *
 * Coherency [static]: there is no CCI node, no dma-coherent and no dma-ranges
 * anywhere in the SoC device tree. No bus master (PCIe, QDMA, SPI-NAND DMA,
 * NPU) is I/O-coherent with the A53 caches: buffers a device writes must be
 * invalidated before the CPU reads them, and buffers a device reads must be
 * cleaned first. The A53 (r0p4) cannot accept cache stashing, so no device can
 * write into a CPU cache [static].
 */
#ifndef HW_AN7581_MEMMAP_H
#define HW_AN7581_MEMMAP_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------ */
/* CPU cluster                                                              */
/* ------------------------------------------------------------------------ */

/*
 * 4 x Cortex-A53, MIDR part number 0xd03, revision r0p4 [measured].
 * Clock: 1.199 GHz measured with a dependent-add chain [measured]; the
 * cpufreq OPP table spans 500..1200 MHz in 50 MHz steps, set through an SMC
 * performance domain [static].
 * Shared L2: 512 KiB, 64-byte lines [static].
 * Compute [measured, NEON]: single-precision FFT (FFTW) at 262,144 points
 * ~11 Msamples/s on four cores (DRAM-bound; 4.3 on one), at 4,096 points
 * 92 Msamples/s; 28-bit IQ repack 178.5 / 203 / 216 / 227 Msamples/s on
 * 1 / 2 / 3 / 4 cores.
 */
#define AN7581_CPU_COUNT                4u
#define AN7581_CPU_MIDR_PARTNUM         0xd03u
#define AN7581_CPU_MAX_HZ               1200000000u
#define AN7581_CPU_MIN_HZ               500000000u
#define AN7581_CPU_L2_BYTES             0x80000u
#define AN7581_CPU_CACHE_LINE           64u

/* Reference oscillator "clkxtal", 25 MHz [static]. */
#define AN7581_XTAL_HZ                  25000000u

/* ARM generic timer PPIs: secure phys 13, non-secure phys 14, virt 11, hyp 10;
 * GIC maintenance PPI 9 [static]. */
#define AN7581_PPI_TIMER_SEC            13u
#define AN7581_PPI_TIMER_NONSEC         14u
#define AN7581_PPI_TIMER_VIRT           11u
#define AN7581_PPI_TIMER_HYP            10u
#define AN7581_PPI_GIC_MAINT            9u

/* GIC-v3 [static]. */
#define AN7581_GICD_BASE                0x09000000u
#define AN7581_GICD_SIZE                0x00020000u
#define AN7581_GICR_BASE                0x09080000u /* redistributors, 4 cores */
#define AN7581_GICR_SIZE                0x00080000u
#define AN7581_GICC_BASE                0x09400000u
#define AN7581_GICC_SIZE                0x00002000u
#define AN7581_GICH_BASE                0x09500000u
#define AN7581_GICH_SIZE                0x00002000u
#define AN7581_GICV_BASE                0x09600000u
#define AN7581_GICV_SIZE                0x00020000u

/* ------------------------------------------------------------------------ */
/* DRAM                                                                     */
/* ------------------------------------------------------------------------ */

/*
 * One bank at 0x80000000, 2 GiB on the W1700K [measured: bootloader and Linux
 * both report 2 GiB; Linux sees 523,968 pages]. The vendor device tree
 * declares 8 GiB; the stock bootloader's /memory fixup corrects it. The
 * fixup does not touch reserved-memory [static].
 *
 * Throughput [measured, idle unless stated]:
 *   A53 read, 1/2/3/4 threads:     2.19-2.25 / 2.56-2.62 / 2.35-2.40 / 2.34 GB/s
 *   A53 memcpy, one core:           0.87-1.18 GB/s
 *   A53 streaming read, one core:   2.4 GB/s (separate tool)
 *   Read while a PCIe master writes 1.28 GB/s: 1.62-1.69 GB/s (2 threads),
 *     i.e. ~2.9 GB/s total DRAM traffic with ~0.37 GB/s left over
 *   Write-heavy workloads (in-DRAM repacking): ~2.5 GB/s total ceiling,
 *     shared by the A53s and the NPU
 *   NPU cluster read: 1.55-1.69 GB/s idle, 1.09 GB/s under a 1.28 GB/s
 *     PCIe write stream (each NPU core has one blocking miss in flight)
 */
#define AN7581_DRAM_BASE                0x80000000u
#define AN7581_DRAM_SIZE_W1700K         0x80000000u

/* ------------------------------------------------------------------------ */
/* Reserved DRAM regions                                                    */
/* ------------------------------------------------------------------------ */

/* Secure firmware (BL31) [static]. Linux reserves 2 MiB; upstream U-Boot
 * reserves only the first 256 KiB. Never map or DMA into it. */
#define AN7581_RSV_ATF_BASE             0x80000000u
#define AN7581_RSV_ATF_SIZE             0x00200000u

/* NPU firmware region: the rv32 image (<= 2 MiB) runs from here and every NPU
 * core's boot base points here; the driver also places DRAM scratch right
 * after the image [static]. Reserved 10 MiB in the SoC device tree; the OEM
 * device tree reserves 1 MiB. Free for other use only while the NPU is off. */
#define AN7581_RSV_NPU_BIN_BASE         0x84000000u
#define AN7581_RSV_NPU_BIN_SIZE         0x00a00000u

/* Frame-engine QDMA hardware-forwarding buffers, required by airoha_eth
 * [static]. */
#define AN7581_RSV_QDMA0_BUF_BASE       0x87000000u
#define AN7581_RSV_QDMA0_BUF_SIZE       0x02000000u
#define AN7581_RSV_QDMA1_BUF_BASE       0x89000000u
#define AN7581_RSV_QDMA1_BUF_SIZE       0x01000000u

/* NPU Wi-Fi offload pools (only for an NPU-offloaded radio; not reserved when
 * NPU Wi-Fi offload is unused) [static]. */
#define AN7581_RSV_NPU_PKT_BASE         0x8a000000u
#define AN7581_RSV_NPU_PKT_SIZE         0x02c00000u
#define AN7581_RSV_NPU_TXPKT_BASE       0x8cc00000u
#define AN7581_RSV_NPU_TXPKT_SIZE       0x04000000u
#define AN7581_RSV_NPU_TXBUFID_BASE     0x90c00000u
#define AN7581_RSV_NPU_TXBUFID_SIZE     0x00006800u
#define AN7581_RSV_NPU_BA_BASE          0x90c06800u
#define AN7581_RSV_NPU_BA_SIZE          0x00200000u

/* Alternative NPU pools in the upstream U-Boot SoC tree (flag, pkt, phyaddr,
 * rxdesc). They overlap a typical initrd and QDMA0's buffers [static]. */
#define AN7581_RSV_UBOOT_NPU_FLAG_BASE  0x84b00000u
#define AN7581_RSV_UBOOT_NPU_FLAG_SIZE  0x00100000u
#define AN7581_RSV_UBOOT_NPU_PKT_BASE   0x85000000u
#define AN7581_RSV_UBOOT_NPU_PKT_SIZE   0x01a00000u
#define AN7581_RSV_UBOOT_NPU_PHYADDR_BASE 0x86b00000u
#define AN7581_RSV_UBOOT_NPU_PHYADDR_SIZE 0x00100000u
#define AN7581_RSV_UBOOT_NPU_RXDESC_BASE 0x86d00000u
#define AN7581_RSV_UBOOT_NPU_RXDESC_SIZE 0x00100000u

/* ------------------------------------------------------------------------ */
/* NPU window                                                               */
/* ------------------------------------------------------------------------ */

/*
 * NPU block, 0x313000 bytes [static]: local SRAM part 0x1e900000..0x1ebfffff,
 * register pages from 0x1ec00000 (npu.h).
 *
 * HAZARD: an A53 read anywhere in 0x1e900000..0x1ebfffff hangs the whole SoC
 *   (no ping, no panic reboot; only a power cycle recovered it) [measured].
 *   A53 writes there were accepted [measured]. The register pages from
 *   0x1ec00000 read normally [measured].
 * HAZARD: an NPU-side search that read 0x3e800000, 0x3e900000 and 0x1e900000
 *   and then wrote markers at doubling offsets up to 2 MiB into one of them
 *   also hung the whole SoC [measured]. Map this window read-only, one word
 *   at a time, before writing anything.
 */
#define AN7581_NPU_BASE                 0x1e900000u
#define AN7581_NPU_SIZE                 0x00313000u
#define AN7581_NPU_SRAM_WINDOW_BASE     0x1e900000u
#define AN7581_NPU_SRAM_WINDOW_SIZE     0x00300000u
#define AN7581_NPU_REGS_BASE            0x1ec00000u
#define AN7581_NPU_REGS_SIZE            0x00013000u

/*
 * Bus alias of the NPU's on-chip RAMs as other on-chip masters see them
 * (NPU-local 0x3e8xxxxx masked to 29 bits) [static from NPU firmware].
 * HAZARD: an MT7996 PCIe write targeting 0x1e890000 completed on the device
 *   side but did not land in the NPU RAM, and left the MT7996 host interface
 *   unable to answer commands until its reset [measured]. Point PCIe DMA only
 *   at addresses known to be reachable.
 */
#define AN7581_NPU_RAM_BUS_ALIAS        0x1e800000u

/* ------------------------------------------------------------------------ */
/* Peripheral register blocks [static unless stated]                        */
/* ------------------------------------------------------------------------ */

#define AN7581_THERMAL_BASE             0x1efbd000u
#define AN7581_THERMAL_SIZE             0x00000d5cu

/* SerDes PCS blocks; each has further sub-blocks (eth.h). */
#define AN7581_PCS_PCIE0_MAC_BASE       0x1fa04000u /* serdes 0 as Ethernet */
#define AN7581_PCS_PCIE1_MAC_BASE       0x1fa05000u /* serdes 1 as Ethernet */
#define AN7581_PCS_USB_MAC_BASE         0x1fa07000u
#define AN7581_PCS_PON_MAC_BASE         0x1fa08000u /* GDM2 */
#define AN7581_PCS_ETH_MAC_BASE         0x1fa09000u /* GDM4 */
#define AN7581_PCS_MAC_SIZE             0x00001000u

#define AN7581_EMMC_TOP_BASE            0x1fa0c000u
#define AN7581_EMMC_TOP_SIZE            0x00000060u
#define AN7581_MMC_BASE                 0x1fa0e000u
#define AN7581_MMC_SIZE                 0x00001000u

/* SPI-NAND controller: SPI control part and NFI part (snfi.h). The NFI
 * registers in use extend to +0x55c; U-Boot maps 0x600, Linux 0x160. */
#define AN7581_SNFI_CTRL_BASE           0x1fa10000u
#define AN7581_SNFI_CTRL_SIZE           0x00000140u
#define AN7581_SNFI_NFI_BASE            0x1fa11000u
#define AN7581_SNFI_NFI_SIZE            0x00000600u

/* Chip SCU: pin mux, pad configuration, clock-source selects (scu.h). */
#define AN7581_CHIP_SCU_BASE            0x1fa20000u
#define AN7581_CHIP_SCU_SIZE            0x00000388u

/* PCIe PHY (serdes 0/1 for PCIe0/PCIe1) [static]. */
#define AN7581_PCIE_PHY_CSR_2L_BASE     0x1fa5a000u
#define AN7581_PCIE_PHY_PMA0_BASE       0x1fa5b000u
#define AN7581_PCIE_PHY_PMA1_BASE       0x1fa5c000u
#define AN7581_PCIE_PHY_BLOCK_SIZE      0x00000fffu

#define AN7581_TRNG_BASE                0x1faa1000u
#define AN7581_TRNG_SIZE                0x00000c04u

/* USB 3 host 0/1 (xHCI MAC + IPPC) and their PHYs. USB1's PHY serdes is the
 * one PCIe2 uses; it carries either USB 3 or PCIe2. */
#define AN7581_USB0_BASE                0x1fab0000u
#define AN7581_USB0_IPPC_BASE           0x1fab3e00u
#define AN7581_USB0_PHY_BASE            0x1fac0000u
#define AN7581_USB1_BASE                0x1fad0000u
#define AN7581_USB1_IPPC_BASE           0x1fad3e00u
#define AN7581_USB1_PHY_BASE            0x1fae0000u
#define AN7581_USB_MAC_SIZE             0x00003e00u
#define AN7581_USB_PHY_SIZE             0x00010000u

/* SCU: clocks gates/PCIe refclk/PERST, resets, SerDes protocol muxes,
 * product ID (scu.h). */
#define AN7581_SCU_BASE                 0x1fb00000u
#define AN7581_SCU_SIZE                 0x00000970u

/* Frame engine and its two QDMAs (eth.h). */
#define AN7581_FE_BASE                  0x1fb50000u
#define AN7581_FE_SIZE                  0x00002600u
#define AN7581_QDMA0_BASE               0x1fb54000u
#define AN7581_QDMA1_BASE               0x1fb56000u
#define AN7581_QDMA_SIZE                0x00002000u
#define AN7581_QDMA_BASE(n)             ((n) ? AN7581_QDMA1_BASE : AN7581_QDMA0_BASE)

/* Built-in gigabit switch (GSW, MT7530-family) with its MDIO bus. */
#define AN7581_SWITCH_BASE              0x1fb58000u
#define AN7581_SWITCH_SIZE              0x00008000u

/* SafeXcel EIP-93 crypto engine. */
#define AN7581_CRYPTO_BASE              0x1fb70000u
#define AN7581_CRYPTO_SIZE              0x00001000u

#define AN7581_HSUART3_BASE             0x1fbe1000u
#define AN7581_HSUART3_SIZE             0x00000040u
#define AN7581_AFE_BASE                 0x1fbe2200u /* audio front end */
#define AN7581_AFE_SIZE                 0x00009000u

/* PBus CSR: per-PCIe-port address-decode base/mask (pcie.h). */
#define AN7581_PBUS_CSR_BASE            0x1fbe3400u
#define AN7581_PBUS_CSR_SIZE            0x000000ffu

/* UARTs are 16550-compatible, 32-bit registers at a 4-byte stride (scu.h). */
#define AN7581_UART1_BASE               0x1fbf0000u /* console */
#define AN7581_UART2_BASE               0x1fbf0300u
#define AN7581_UART4_BASE               0x1fbf0600u
#define AN7581_UART5_BASE               0x1fbf0700u
#define AN7581_UART_SIZE                0x00000030u

/* Timer block whose timer 3 is the watchdog (scu.h). */
#define AN7581_WDT_BASE                 0x1fbf0100u
#define AN7581_WDT_SIZE                 0x00000038u

/* GPIO controller, GPIO interrupt and PWM registers (scu.h). */
#define AN7581_GPIO_BASE                0x1fbf0200u
#define AN7581_GPIO_SIZE                0x000000c0u

/* I2C controllers (MT7621-compatible), 20 MHz controller clock. */
#define AN7581_I2C0_BASE                0x1fbf8000u
#define AN7581_I2C1_BASE                0x1fbf8100u
#define AN7581_I2C_SIZE                 0x00000100u
#define AN7581_I2C_CLK_HZ               20000000u

/* PCIe controllers (MediaTek Gen3 MAC), three ports, one domain each. PCIe1's
 * MAC doubles as the second-lane MAC of PCIe0 in x2 mode (pcie.h). */
#define AN7581_PCIE0_MAC_BASE           0x1fc00000u
#define AN7581_PCIE1_MAC_BASE           0x1fc20000u
#define AN7581_PCIE2_MAC_BASE           0x1fc40000u
#define AN7581_PCIE_MAC_SIZE            0x00001670u
#define AN7581_PCIE_MAC_BASE(n)         (0x1fc00000u + 0x20000u * (n))
/* PHY tuning words outside the PHY block. */
#define AN7581_PCIE_P0_XR_DTIME         0x1fc10044u
#define AN7581_PCIE_P1_XR_DTIME         0x1fc30044u
#define AN7581_PCIE_RX_AEQ_BASE         0x1fc15030u
#define AN7581_PCIE_RX_AEQ_SIZE         0x00000104u

/* PCIe outbound MMIO windows, identity-mapped (CPU = PCI address) [static]. */
#define AN7581_PCIE0_WIN_BASE           0x20000000u
#define AN7581_PCIE1_WIN_BASE           0x24000000u
#define AN7581_PCIE2_WIN_BASE           0x28000000u
#define AN7581_PCIE_WIN_SIZE            0x04000000u
#define AN7581_PCIE_WIN_BASE(n)         (0x20000000u + 0x04000000u * (n))

/* ------------------------------------------------------------------------ */
/* Interrupts (GIC SPI numbers, all level-high) [static]                    */
/* ------------------------------------------------------------------------ */

#define AN7581_IRQ_UART1                18u
#define AN7581_IRQ_UART5                18u /* shares UART1's line */
#define AN7581_IRQ_THERMAL              23u
#define AN7581_IRQ_GPIO                 26u /* pinctrl / GPIO interrupt */
#define AN7581_IRQ_SWITCH               31u
#define AN7581_IRQ_UART2                32u
#define AN7581_IRQ_USB0                 33u
#define AN7581_IRQ_TRNG                 35u
#define AN7581_IRQ_PCIE0                39u
#define AN7581_IRQ_PCIE1                40u
#define AN7581_IRQ_PCIE2                41u
#define AN7581_IRQ_PCIE(n)              (39u + (n))
#define AN7581_IRQ_CRYPTO               44u
#define AN7581_IRQ_AFE                  48u
#define AN7581_IRQ_HSUART3              54u
#define AN7581_IRQ_UART4                61u
#define AN7581_IRQ_USB1                 150u
#define AN7581_IRQ_MMC                  170u

/* Frame engine: four IRQ banks per QDMA. Bank 0 is 37 (QDMA0) / 38 (QDMA1),
 * banks 1-3 are 55-57 (QDMA0) / 58-60 (QDMA1). Two further FE lines, 49 and
 * 64, are listed for the block; mainline airoha_eth requests neither. */
#define AN7581_IRQ_QDMA(q, bank)        ((bank) == 0 ? 37u + (q) : 54u + 3u * (q) + (bank))
#define AN7581_IRQ_FE_EXTRA0            49u
#define AN7581_IRQ_FE_EXTRA1            64u

/* NPU: one mailbox line, one watchdog line per core, six Wi-Fi offload
 * lines. */
#define AN7581_IRQ_NPU_MBOX             125u
#define AN7581_IRQ_NPU_WDT(core)        ((core) == 0 ? 103u : (core) == 1 ? 109u : \
                                         (core) == 2 ? 113u : (core) == 3 ? 117u : \
                                         134u + ((core) - 4))
#define AN7581_IRQ_NPU_WLAN(n)          (118u + (n)) /* n = 0..5 */

#endif /* HW_AN7581_MEMMAP_H */
