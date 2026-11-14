/** @file
  Google Tensor G1 (GS101) register bases used by the bluejay UEFI port.

  Every address below is measured on a Pixel 6a (bluejay) or taken from the
  gs101 device tree; see docs/EVIDENCE.md. Nothing here is guessed.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __GS101_H__
#define __GS101_H__

//
// UFS host (UFSHCI + Exynos/Google vendor block)
// Measured: HCI 0x14700000, vendor HCI +0x1100, PHY/PMA +0x4000, UNIPRO +0x80000.
//
#define GS101_UFS_BASE                 0x14700000U
#define GS101_UFS_VS_BASE              (GS101_UFS_BASE + 0x1100U)
#define GS101_UFS_PHY_PMA_BASE         (GS101_UFS_BASE + 0x4000U)
#define GS101_UFS_UNIPRO_BASE          (GS101_UFS_BASE + 0x80000U)

//
// Power management unit. The UFS MPHY isolation bypass is PMU + 0x3EC8 bit 0
// (the same word the Android bootloader leaves set; pixelios only writes it
// when the bit is clear).
//
#define GS101_PMU_BASE                 0x17460000U
#define GS101_PMU_UFS_PHY_ISO          (GS101_PMU_BASE + 0x3EC8U)
#define GS101_PMU_UFS_PHY_ISO_BYPASS   0x00000001U

//
// HSI1 SYSREG. Bits 0..1 make UFS I/O coherent; pixelios preserves the
// bootloader value and only sets missing bits.
//
#define GS101_SYSREG_HSI1_BASE         0x14420000U
#define GS101_SYSREG_HSI1_IOCOHERENCY  (GS101_SYSREG_HSI1_BASE + 0x0710U)
#define GS101_SYSREG_HSI1_IOCOHERENCY_BITS 0x00000003U

//
// UNIPRO MCLK period register. The bootloader programs it to 16e9 / mclk;
// the host software reset reverts it, so the value is saved before reset.
//
#define GS101_UNIPRO_MCLK_PERIOD       0x0044U
#define GS101_UFS_MCLK_NUMERATOR       16000000000ULL
#define GS101_UFS_MCLK_MIN_KHZ         50000U
#define GS101_UFS_MCLK_MAX_KHZ         400000U

//
// Watchdogs (s3c2410 layout). WTCON bit 5 = ENABLE, bit 0 = RSTEN.
//
#define GS101_WDT_CL0_BASE             0x10060000U
#define GS101_WDT_CL1_BASE             0x10070000U

//
// Display controller 0 (DECON0). The bootloader clears the hardware trigger;
// writing the command-mode trigger value re-arms direct framebuffer writes.
//
#define GS101_DECON0_BASE              0x1C300000U
#define GS101_DECON_TRIG_CON           0x0030U
#define GS101_DECON_TRIG_EN            0x00003061U

//
// GIC and UART (documented for the memory map; the UEFI console uses the
// bootloader framebuffer, not the UART).
//
#define GS101_GICD_BASE                0x10400000U
#define GS101_GICR_BASE                0x10440000U
#define GS101_UART_BASE                0x10A00000U

#endif /* __GS101_H__ */
