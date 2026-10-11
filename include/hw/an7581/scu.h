/* SPDX-License-Identifier: MIT */
/*
 * AN7581 system control: SCU (resets, PCIe refclk/PERST, SerDes protocol
 * muxes, product ID, boot strap), chip SCU (pin mux, pad configuration,
 * clock-source selects), GPIO controller, watchdog timer and UARTs.
 *
 * ADDRESS SPACE: offsets relative to the block bases in memmap.h:
 *   AN7581_SCU_*       relative to AN7581_SCU_BASE      (0x1fb00000)
 *   AN7581_CHIP_SCU_*  relative to AN7581_CHIP_SCU_BASE (0x1fa20000)
 *   AN7581_GPIO_*      relative to AN7581_GPIO_BASE     (0x1fbf0200)
 *   AN7581_WDT_*       relative to AN7581_WDT_BASE      (0x1fbf0100)
 *   AN7581_UART_*      relative to any AN7581_UARTn_BASE
 * All registers are 32 bits wide.
 *
 * GPIO numbering: "GPIO n" is the GPIO controller's line offset n (n = 0..49
 * in use), which is pin-controller pin n + 13. Pins 0..7 are the dedicated
 * uart1_txd, uart1_rxd, i2c_scl, i2c_sda, spi_cs0, spi_clk, spi_mosi and
 * spi_miso pads; GPIO 47/48/49 are the pcie_reset0/1/2 pads [static].
 */
#ifndef HW_AN7581_SCU_H
#define HW_AN7581_SCU_H

#include "../hw_bits.h"

/* ======================================================================== */
/* SCU (0x1fb00000)                                                         */
/* ======================================================================== */

/* Product/stepping ID. RO [static]. One reported reading on a W1700K2-class
 * board: 0x00000002 (E2 stepping) [inferred]. */
#define AN7581_SCU_PDIDR                0x05cu
#define AN7581_SCU_PDIDR_PRODUCT_ID     HW_GENMASK(15, 0)

/* WAN MAC/serdes routing. RW [static]. WAN_SEL selects the PON-serdes protocol
 * the PON PCS drives (GDM2 on the W1700K: USXGMII). ETH_MAC_SEL is written
 * only on AN7583 by mainline. */
#define AN7581_SCU_WAN_CONF             0x070u
#define AN7581_SCU_WAN_CONF_ETH_MAC_SEL HW_BIT(24)  /* 0 XFI MAC, 1 PON MAC */
#define AN7581_SCU_WAN_CONF_WAN_SEL     HW_GENMASK(7, 0)
#define AN7581_SCU_WAN_CONF_WAN_SEL_SGMII   0x10u
#define AN7581_SCU_WAN_CONF_WAN_SEL_HSGMII  0x11u
#define AN7581_SCU_WAN_CONF_WAN_SEL_USXGMII 0x12u

/*
 * PCIe control: refclk enables, PERST# outputs, port mux. RW [static].
 * PERSTOUTn: 0 asserts PERST# to the endpoint, 1 releases it (inverted
 * relative to every other reset bit). Linux exposes them as reset lines
 * PCIC_PERSTOUT0/1/2; PCIe0's PERST# reaches the MT7996.
 * REFCLK_EN0|REFCLK_EN1 both set = the "PCIe" clock is on.
 * PCIE_CTRL: written 3 at clock-driver init; written 2 (bit 1) when PCIe0
 * runs x2, which takes PCIe1's lane away from the PCIe1 controller.
 */
#define AN7581_SCU_PCIC                 0x088u
#define AN7581_SCU_PCIC_PERSTOUT0       HW_BIT(29)
#define AN7581_SCU_PCIC_PERSTOUT1       HW_BIT(26)
#define AN7581_SCU_PCIC_REFCLK_EN0      HW_BIT(23)
#define AN7581_SCU_PCIC_REFCLK_EN1      HW_BIT(22)
#define AN7581_SCU_PCIC_PERSTOUT2       HW_BIT(16)
#define AN7581_SCU_PCIC_PCIE_CTRL       HW_GENMASK(7, 0)
#define AN7581_SCU_PCIC_PCIE_CTRL_INIT  3u
#define AN7581_SCU_PCIC_PCIE_CTRL_X2    2u

/* Ethernet-serdes (XSI) protocol for the ETH PCS (GDM4). RW [static]. */
#define AN7581_SCU_SSR3                 0x094u
#define AN7581_SCU_SSR3_ETH_XSI_SEL     HW_GENMASK(14, 13)
#define AN7581_SCU_XSI_SEL_PCIE         0u  /* SSTR PCIe fields: serdes stays PCIe */
#define AN7581_SCU_XSI_SEL_USXGMII      1u
#define AN7581_SCU_XSI_SEL_HSGMII       2u  /* value 3: meaning not established */

/*
 * SerDes protocol select for the two PCIe serdes and the PON serdes. RW
 * [static]. Encoding as AN7581_SCU_XSI_SEL_*. The clock driver clears
 * PCIE_XSI0/XSI1 at init, which leaves both PCIe serdes as PCIe; values 1/2
 * turn a PCIe serdes into an Ethernet port behind the PCIe PCS (unused on the
 * W1700K, where both PCIe0 lanes carry the MT7996).
 */
#define AN7581_SCU_SSTR                 0x09cu
#define AN7581_SCU_SSTR_PCIE_XSI0_SEL   HW_GENMASK(14, 13)
#define AN7581_SCU_SSTR_PCIE_XSI1_SEL   HW_GENMASK(12, 11)
#define AN7581_SCU_SSTR_PON_XSI_SEL     HW_GENMASK(10, 9)

/* Boot-device strap. RO [static, read by the stock bootloader's flash
 * workers]. Other bits undecoded. */
#define AN7581_SCU_BOOTSTRAP            0x0b8u
#define AN7581_SCU_BOOTSTRAP_EMMC       HW_BIT(6)   /* 0 SPI-NAND, 1 eMMC */

/* Undecoded. Bit 9 gates the stock bootloader's write of an indicator value
 * to switch block +0x1640 when its TFTP receiver starts [static]. */
#define AN7581_SCU_REG_284              0x284u
#define AN7581_SCU_REG_284_IND_EN       HW_BIT(9)

/*
 * Reset controls. RW, 1 = held in reset, 0 = released [static]. Bit
 * positions, not Linux reset-binding IDs. Bits not listed are unassigned.
 */
#define AN7581_SCU_RST_CTRL2            0x830u
#define AN7581_SCU_RST2_XPON_PHY        HW_BIT(0)
#define AN7581_SCU_RST2_CPU_TIMER2      HW_BIT(2)
#define AN7581_SCU_RST2_HSUART          HW_BIT(3)
#define AN7581_SCU_RST2_UART4           HW_BIT(4)
#define AN7581_SCU_RST2_UART5           HW_BIT(5)
#define AN7581_SCU_RST2_I2C2            HW_BIT(6)
#define AN7581_SCU_RST2_XSI_MAC         HW_BIT(7)   /* ETH PCS MAC (GDM4) */
#define AN7581_SCU_RST2_XSI_PHY         HW_BIT(8)   /* ETH PCS PHY (GDM4) */
/* NPU cluster reset [measured]: pulsing it (1 then 0) stops all eight cores
 * and clears their state; the next BOOT_TRIGGER starts them from their boot
 * base. Cores parked in wfi do not restart on BOOT_TRIGGER alone. */
#define AN7581_SCU_RST2_NPU             HW_BIT(9)
#define AN7581_SCU_RST2_I2S             HW_BIT(10)
#define AN7581_SCU_RST2_TRNG            HW_BIT(11)
#define AN7581_SCU_RST2_TRNG_MSTART     HW_BIT(12)
#define AN7581_SCU_RST2_DUAL_HSI0       HW_BIT(13)
#define AN7581_SCU_RST2_DUAL_HSI1       HW_BIT(14)
#define AN7581_SCU_RST2_HSI             HW_BIT(15)
#define AN7581_SCU_RST2_DUAL_HSI0_MAC   HW_BIT(16)
#define AN7581_SCU_RST2_DUAL_HSI1_MAC   HW_BIT(17)
#define AN7581_SCU_RST2_HSI_MAC         HW_BIT(18)
#define AN7581_SCU_RST2_WDMA            HW_BIT(19)
#define AN7581_SCU_RST2_WOE0            HW_BIT(20)
#define AN7581_SCU_RST2_WOE1            HW_BIT(21)
#define AN7581_SCU_RST2_HSDMA           HW_BIT(22)
#define AN7581_SCU_RST2_TDMA            HW_BIT(24)
#define AN7581_SCU_RST2_EMMC            HW_BIT(25)
#define AN7581_SCU_RST2_SOE             HW_BIT(26)
#define AN7581_SCU_RST2_PCIE2           HW_BIT(27)  /* PCIe2 PHY lane */
#define AN7581_SCU_RST2_XFP_MAC         HW_BIT(28)
#define AN7581_SCU_RST2_USB_HOST_P1     HW_BIT(29)
#define AN7581_SCU_RST2_USB_HOST_P1_U3_PHY HW_BIT(30)

#define AN7581_SCU_RST_CTRL1            0x834u
#define AN7581_SCU_RST1_PCM1_ZSI_ISI    HW_BIT(0)
#define AN7581_SCU_RST1_FE_PDMA         HW_BIT(1)
#define AN7581_SCU_RST1_FE_QDMA         HW_BIT(2)
#define AN7581_SCU_RST1_PCM_SPIWP       HW_BIT(4)
#define AN7581_SCU_RST1_CRYPTO          HW_BIT(6)
#define AN7581_SCU_RST1_TIMER           HW_BIT(8)
#define AN7581_SCU_RST1_PCM1            HW_BIT(11)
#define AN7581_SCU_RST1_UART            HW_BIT(12)
#define AN7581_SCU_RST1_GPIO            HW_BIT(13)
#define AN7581_SCU_RST1_GDMA            HW_BIT(14)
#define AN7581_SCU_RST1_I2C_MASTER      HW_BIT(16)
#define AN7581_SCU_RST1_PCM2_ZSI_ISI    HW_BIT(17)
#define AN7581_SCU_RST1_SFC             HW_BIT(18)  /* serial flash controller */
#define AN7581_SCU_RST1_UART2           HW_BIT(19)
#define AN7581_SCU_RST1_GDMP            HW_BIT(20)
#define AN7581_SCU_RST1_FE              HW_BIT(21)
#define AN7581_SCU_RST1_USB_HOST_P0     HW_BIT(22)
#define AN7581_SCU_RST1_GSW             HW_BIT(23)  /* built-in switch */
#define AN7581_SCU_RST1_SFC2_PCM        HW_BIT(25)
#define AN7581_SCU_RST1_PCIE0           HW_BIT(26)  /* PCIe0 PHY lane */
#define AN7581_SCU_RST1_PCIE1           HW_BIT(27)  /* PCIe1 PHY lane */
#define AN7581_SCU_RST1_CPU_TIMER       HW_BIT(28)
/* PCIe host bridge; the clock driver pulses it once at init. */
#define AN7581_SCU_RST1_PCIE_HB         HW_BIT(29)
#define AN7581_SCU_RST1_XPON_MAC        HW_BIT(31)  /* PON PCS MAC (GDM2) */

/* ======================================================================== */
/* Chip SCU (0x1fa20000): pad configuration                                 */
/* ======================================================================== */

/*
 * Dedicated-pad configuration: drive strength E2/E4, pull-up, pull-down. RW
 * [static]. The four registers share one bit layout (AN7581_CHIP_SCU_PAD_*).
 */
#define AN7581_CHIP_SCU_PADCFG_E2       0x01cu
#define AN7581_CHIP_SCU_PADCFG_E4       0x020u
#define AN7581_CHIP_SCU_PADCFG_PU       0x044u
#define AN7581_CHIP_SCU_PADCFG_PD       0x048u
#define AN7581_CHIP_SCU_PAD_SPI_MISO    HW_BIT(14)
#define AN7581_CHIP_SCU_PAD_SPI_MOSI    HW_BIT(13)
#define AN7581_CHIP_SCU_PAD_SPI_CLK     HW_BIT(12)
#define AN7581_CHIP_SCU_PAD_SPI_CS0     HW_BIT(11)
#define AN7581_CHIP_SCU_PAD_PCIE2_RESET HW_BIT(10)
#define AN7581_CHIP_SCU_PAD_PCIE1_RESET HW_BIT(9)
#define AN7581_CHIP_SCU_PAD_PCIE0_RESET HW_BIT(8)
#define AN7581_CHIP_SCU_PAD_UART1_RXD   HW_BIT(3)
#define AN7581_CHIP_SCU_PAD_UART1_TXD   HW_BIT(2)
#define AN7581_CHIP_SCU_PAD_I2C_SCL     HW_BIT(1)
#define AN7581_CHIP_SCU_PAD_I2C_SDA     HW_BIT(0)

/* GPIO-pad configuration. RW [static]. "_L" registers: bit n = GPIO n
 * (n = 0..31); "_H" registers: bit n - 32 = GPIO n (n = 32..46). */
#define AN7581_CHIP_SCU_GPIO_L_E2       0x024u
#define AN7581_CHIP_SCU_GPIO_L_E4       0x028u
#define AN7581_CHIP_SCU_GPIO_H_E2       0x02cu
#define AN7581_CHIP_SCU_GPIO_H_E4       0x030u
#define AN7581_CHIP_SCU_GPIO_L_PU       0x04cu
#define AN7581_CHIP_SCU_GPIO_L_PD       0x050u
#define AN7581_CHIP_SCU_GPIO_H_PU       0x054u
#define AN7581_CHIP_SCU_GPIO_H_PD       0x058u
#define AN7581_CHIP_SCU_GPIO_PAD_BIT(gpio) HW_BIT((gpio) & 31u)

/* PCIe PERST# pads in open-drain mode. RW [static]. The W1700K sets OD on
 * pcie_reset0 and pcie_reset2. */
#define AN7581_CHIP_SCU_PCIE_RESET_OD   0x18cu
#define AN7581_CHIP_SCU_PCIE_RESET_OD_PCIE2 HW_BIT(2)
#define AN7581_CHIP_SCU_PCIE_RESET_OD_PCIE1 HW_BIT(1)
#define AN7581_CHIP_SCU_PCIE_RESET_OD_PCIE0 HW_BIT(0)

/* ======================================================================== */
/* Chip SCU: clock-source selects [static]                                  */
/* ======================================================================== */

/*
 * Rate = base / div. For GSW, EMI, BUS and NPU the divider field DIV holds
 * (div - 1). For SPI the base is a fixed 400 MHz and DIV holds div / 2, with
 * 0 meaning /40. Rates read back through the clock framework, not measured,
 * except the NPU (800 MHz measured on every core).
 */
#define AN7581_CHIP_SCU_CLK_DIV         HW_GENMASK(2, 0)

#define AN7581_CHIP_SCU_GSW_CLK_SEL     0x1b4u
#define AN7581_CHIP_SCU_GSW_CLK_SEL_BASE HW_BIT(8)          /* 400 / 500 MHz */

#define AN7581_CHIP_SCU_EMI_CLK_SEL     0x1b8u
#define AN7581_CHIP_SCU_EMI_CLK_SEL_BASE HW_GENMASK(9, 8)   /* 540/480/400/300 MHz */

#define AN7581_CHIP_SCU_BUS_CLK_SEL     0x1bcu
#define AN7581_CHIP_SCU_BUS_CLK_SEL_BASE HW_BIT(8)          /* 600 / 540 MHz */

#define AN7581_CHIP_SCU_SPI_CLK_DIV_SEL 0x1c4u
#define AN7581_CHIP_SCU_SPI_CLK_DIV_SEL_SLIC_DIV HW_GENMASK(28, 24) /* 0 = /20, else x2 */
#define AN7581_CHIP_SCU_SPI_CLK_DIV_SEL_SPI_DIV  HW_GENMASK(12, 8)  /* 0 = /40, else x2 */
#define AN7581_CHIP_SCU_SPI_CLK_BASE_HZ 400000000u

#define AN7581_CHIP_SCU_SPI_CLK_FREQ_SEL 0x1c8u
#define AN7581_CHIP_SCU_SPI_CLK_FREQ_SEL_SLIC_BASE HW_BIT(0) /* 100 MHz / 3.125 MHz */

/* NPU core clock: BASE 0..3 = 800/750/720/600 MHz. */
#define AN7581_CHIP_SCU_NPU_CLK_SEL     0x1fcu
#define AN7581_CHIP_SCU_NPU_CLK_SEL_BASE HW_GENMASK(9, 8)

#define AN7581_CHIP_SCU_CRYPTO_CLKSRC2  0x20cu
#define AN7581_CHIP_SCU_CRYPTO_CLKSRC2_EMMC   HW_BIT(12)    /* 200 / 150 MHz */
#define AN7581_CHIP_SCU_CRYPTO_CLKSRC2_CRYPTO HW_BIT(0)     /* 540 / 480 MHz */

/* ======================================================================== */
/* Chip SCU: pin mux [static]                                               */
/* ======================================================================== */

/* The pin controller writes 0 to 0x214, 0x218, 0x21c, 0x224 and 0x228 at
 * probe, then sets the functions the device tree requests. */
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE 0x214u
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_MDC_IO_MASTER HW_BIT(14) /* "mdio" function */
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_I2C_MASTER    HW_BIT(13)
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_I2S           HW_BIT(12)
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_I2C_SLAVE     HW_BIT(11)
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_LAN_LED(port, led) HW_BIT(3 + 2 * (port) + (led))
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_PON_TOD_1PPS  HW_BIT(2)
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_GSW_TOD_1PPS  HW_BIT(1)
#define AN7581_CHIP_SCU_GPIO_2ND_I2C_MODE_2ND_I2C       HW_BIT(0)

#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE 0x218u
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI_CS4   HW_BIT(21)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI_CS3   HW_BIT(20)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI_CS2_P156 HW_BIT(19)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI_CS2_P128 HW_BIT(18)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI_CS1   HW_BIT(17)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_SPI       HW_BIT(16)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM2          HW_BIT(13)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM1          HW_BIT(12)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_INT       HW_BIT(9)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_PCM_RESET     HW_BIT(8)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_SPI_QUAD      HW_BIT(4)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_SPI_CS4       HW_BIT(3)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_SPI_CS3       HW_BIT(2)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_SPI_CS2       HW_BIT(1)
#define AN7581_CHIP_SCU_GPIO_SPI_CS1_MODE_SPI_CS1       HW_BIT(0)

/* PCIE_RESETn bits: 0 = pad is the controller's PERST# output, 1 = GPIO. */
#define AN7581_CHIP_SCU_GPIO_PON_MODE   0x21cu
#define AN7581_CHIP_SCU_GPIO_PON_MODE_PARALLEL_NAND     HW_BIT(14)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_SGMII_MDIO        HW_BIT(13)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_PCIE_RESET2       HW_BIT(12)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_SIPO_RCLK         HW_BIT(11)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_PCIE_RESET1       HW_BIT(10)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_PCIE_RESET0       HW_BIT(9)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_UART5             HW_BIT(8)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_UART4             HW_BIT(7)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_HSUART_CTS_RTS    HW_BIT(6)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_HSUART            HW_BIT(5)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_UART2_CTS_RTS     HW_BIT(4)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_UART2             HW_BIT(3)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_SIPO              HW_BIT(2)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_EMMC              HW_BIT(1)
#define AN7581_CHIP_SCU_GPIO_PON_MODE_PON               HW_BIT(0)

#define AN7581_CHIP_SCU_NPU_UART_EN     0x224u
#define AN7581_CHIP_SCU_NPU_UART_EN_JTAG_UDI HW_BIT(4)
#define AN7581_CHIP_SCU_NPU_UART_EN_JTAG_DFD HW_BIT(3)

/*
 * Force a pad to plain GPIO, overriding its mux function. RW.
 * FORCE_GPIO_EN: bit n = GPIO n, n = 0..31 [static]; the "mdio" function
 * sets bits 1 and 2. FORCE_GPIO_EN_H: bit n - 32 = GPIO n [inferred: to read
 * the board-revision straps on GPIO 30/18/45/44 the stock firmware writes
 * 0x40040000 to FORCE_GPIO_EN and 0x3000 to FORCE_GPIO_EN_H, reads, then
 * writes 0 to both]. Full-word writes change pin ownership until restored.
 */
#define AN7581_CHIP_SCU_FORCE_GPIO_EN   0x228u
#define AN7581_CHIP_SCU_FORCE_GPIO_EN_H 0x22cu
#define AN7581_CHIP_SCU_FORCE_GPIO_EN_BIT(gpio) HW_BIT((gpio) & 31u)

/* Which PHY LED drives each LAN LED pad (LED0 and LED1 sets). RW [static]. */
#define AN7581_CHIP_SCU_LAN_LED0_MAPPING 0x27cu
#define AN7581_CHIP_SCU_LAN_LED1_MAPPING 0x280u
#define AN7581_CHIP_SCU_LAN_LED_MAP(port) HW_GENMASK(2 + 4 * (port), 4 * (port)) /* port 0..4 */

/* ======================================================================== */
/* GPIO controller (0x1fbf0200) [static]                                    */
/* ======================================================================== */

/*
 * Banks of 32 for data, output enable and interrupt status; banks of 16 (two
 * bits per GPIO) for direction and interrupt type. To drive GPIO n: set its
 * OE bit and direction bit 2*(n%16) (the bit above it stays 0), then its
 * DATA bit.
 */
#define AN7581_GPIO_CTRL                0x00u   /* direction, GPIO 0..15 */
#define AN7581_GPIO_DATA                0x04u   /* level/drive, GPIO 0..31 */
#define AN7581_GPIO_INT                 0x08u   /* irq status, GPIO 0..31, W1C */
#define AN7581_GPIO_INT_EDGE            0x0cu   /* edge enable, GPIO 0..15 */
#define AN7581_GPIO_INT_LEVEL           0x10u   /* level enable, GPIO 0..15 */
#define AN7581_GPIO_OE                  0x14u   /* output enable, GPIO 0..31 */
#define AN7581_GPIO_CTRL1               0x20u   /* direction, GPIO 16..31 */
#define AN7581_GPIO_FLASH_MODE_CFG      0x34u   /* PWM/flash mode, bit n = GPIO n (0..15) */
#define AN7581_GPIO_CTRL2               0x60u   /* direction, GPIO 32..47 */
#define AN7581_GPIO_CTRL3               0x64u   /* direction, GPIO 48..63 */
/* PWM/flash mode: bits 15:0 = GPIO 16..31, bits 31:16 = GPIO 36..51. */
#define AN7581_GPIO_FLASH_MODE_CFG_EXT  0x68u
#define AN7581_GPIO_DATA1               0x70u   /* GPIO 32..63 */
#define AN7581_GPIO_OE1                 0x78u
#define AN7581_GPIO_INT1                0x7cu
#define AN7581_GPIO_INT_EDGE1           0x80u   /* GPIO 16..31 */
#define AN7581_GPIO_INT_EDGE2           0x84u   /* GPIO 32..47 */
#define AN7581_GPIO_INT_EDGE3           0x88u   /* GPIO 48..63 */
#define AN7581_GPIO_INT_LEVEL1          0x8cu
#define AN7581_GPIO_INT_LEVEL2          0x90u
#define AN7581_GPIO_INT_LEVEL3          0x94u

#define AN7581_GPIO_DATA_REG(n)         ((n) < 32 ? AN7581_GPIO_DATA : AN7581_GPIO_DATA1)
#define AN7581_GPIO_OE_REG(n)           ((n) < 32 ? AN7581_GPIO_OE : AN7581_GPIO_OE1)
#define AN7581_GPIO_INT_REG(n)          ((n) < 32 ? AN7581_GPIO_INT : AN7581_GPIO_INT1)
#define AN7581_GPIO_DIR_REG(n)          ((n) < 16 ? AN7581_GPIO_CTRL : (n) < 32 ? AN7581_GPIO_CTRL1 : \
                                         (n) < 48 ? AN7581_GPIO_CTRL2 : AN7581_GPIO_CTRL3)
#define AN7581_GPIO_EDGE_REG(n)         ((n) < 16 ? AN7581_GPIO_INT_EDGE : (n) < 32 ? AN7581_GPIO_INT_EDGE1 : \
                                         (n) < 48 ? AN7581_GPIO_INT_EDGE2 : AN7581_GPIO_INT_EDGE3)
#define AN7581_GPIO_LEVEL_REG(n)        ((n) < 16 ? AN7581_GPIO_INT_LEVEL : (n) < 32 ? AN7581_GPIO_INT_LEVEL1 : \
                                         (n) < 48 ? AN7581_GPIO_INT_LEVEL2 : AN7581_GPIO_INT_LEVEL3)
#define AN7581_GPIO_BIT(n)              HW_BIT((n) & 31u)
/* Two-bit fields in DIR/EDGE/LEVEL registers. */
#define AN7581_GPIO_DIR_OUT(n)          HW_BIT(2 * ((n) & 15u))
#define AN7581_GPIO_TYPE_FIELD(n)       HW_GENMASK(2 * ((n) & 15u) + 1, 2 * ((n) & 15u))
#define AN7581_GPIO_TYPE_RISING_HIGH(n) HW_BIT(2 * ((n) & 15u))     /* EDGE: rising, LEVEL: high */
#define AN7581_GPIO_TYPE_FALLING_LOW(n) HW_BIT(2 * ((n) & 15u) + 1) /* EDGE: falling, LEVEL: low */

/* ======================================================================== */
/* Watchdog (timer 3 of the timer block at 0x1fbf0100) [static]             */
/* ======================================================================== */

/*
 * The counter runs at the bus clock / 2 and loads from LOAD when started.
 * Through the Linux driver: maximum timeout 28 s (30 s refused), magic close
 * stops it, and an expiry resets the SoC, which then boots unaided
 * [measured]. It recovered the host from PCIe MMIO-read lockups that left the
 * A53s unresponsive; it does not reset a wedged PCIe endpoint [measured].
 */
#define AN7581_WDT_TIMER_CTRL           0x00u
#define AN7581_WDT_TIMER_CTRL_WDT_EN    HW_BIT(25)
#define AN7581_WDT_TIMER_CTRL_WDT_INT   HW_BIT(21)
#define AN7581_WDT_TIMER_CTRL_TIMER3_EN HW_BIT(5)
#define AN7581_WDT_LOAD                 0x2cu   /* timeout in counter ticks */
#define AN7581_WDT_CUR                  0x30u   /* RO, current count */
#define AN7581_WDT_RELOAD               0x38u
#define AN7581_WDT_RELOAD_RLD           HW_BIT(0) /* write 1: kick */

/* ======================================================================== */
/* UART (16550-compatible) [static]                                         */
/* ======================================================================== */

/*
 * UART1 (console): ns16550, 32-bit access, register stride 4, input clock
 * 1.8432 MHz. UART2/4/5 and HSUART3: same register file, 7.3728 MHz input
 * clock, Airoha baud extension (Linux 8250_airoha). Console: 115200 8N1.
 * Standard 16550 registers at a 4-byte stride [inferred from the 16550
 * binding].
 */
#define AN7581_UART1_CLK_HZ             1843200u
#define AN7581_UART_AUX_CLK_HZ          7372800u
#define AN7581_UART_RBR_THR_DLL         0x00u
#define AN7581_UART_IER_DLM             0x04u
#define AN7581_UART_IIR_FCR             0x08u
#define AN7581_UART_LCR                 0x0cu
#define AN7581_UART_MCR                 0x10u
#define AN7581_UART_LSR                 0x14u
#define AN7581_UART_MSR                 0x18u
#define AN7581_UART_SCR                 0x1cu
#define AN7581_UART_LSR_THRE            HW_BIT(5)
#define AN7581_UART_LSR_DR              HW_BIT(0)

#endif /* HW_AN7581_SCU_H */
