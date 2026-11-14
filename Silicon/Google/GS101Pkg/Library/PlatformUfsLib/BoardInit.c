/** @file
  GS101 UFS board initialization.

  Deliberately small: the Android bootloader (ABL) already brings the UFS
  controller, its MPHY reference clock and the XBOOTLDO rail out of reset
  before it hands over. The pixelios cold-init sequence only repairs what the
  host software reset reverts, and this library follows exactly that model.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UfsHostBridge.h>
#include <Library/PlatformUfsLib.h>

#include <GS101.h>

//
// The device power input of the Samsung UFS driver is a PMU GPIO on Exynos.
// GS101 instead has the UFS rail on a PMIC (XBOOTLDO) that ABL leaves enabled,
// so the only writable per-device line is the vendor GPIO_OUT reset word.
// Using it makes UfsDevicePower a no-op and leaves UfsDeviceReset in charge.
//
#define GS101_UFS_VS_GPIO_OUT  0x0070U

EFI_STATUS
UfsBoardInit (
  struct UfsHost *Ufs
  )
{
  UINT32 Comp;
  UINT32 MclkRate;
  UINT32 Share;

  if (Ufs == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // UNIPRO+0x44 holds COMP_CLK_PERIOD = 16e9 / mclk, programmed by ABL.
  // The host software reset reverts it, so UfsCalPreLink restores this value.
  //
  Comp = MmioRead32 (GS101_UFS_UNIPRO_BASE + GS101_UNIPRO_MCLK_PERIOD);
  if ((Comp < (GS101_UFS_MCLK_NUMERATOR / (GS101_UFS_MCLK_MAX_KHZ * 1000ULL))) ||
      (Comp > (GS101_UFS_MCLK_NUMERATOR / (GS101_UFS_MCLK_MIN_KHZ * 1000ULL))))
  {
    DEBUG ((DEBUG_ERROR, "UFS: implausible COMP_CLK_PERIOD %u, ABL clock state unknown\n", Comp));
    return EFI_DEVICE_ERROR;
  }

  MclkRate = (UINT32)(GS101_UFS_MCLK_NUMERATOR / Comp);

  Ufs->IoAddr     = (VOID *)(UINTN)GS101_UFS_BASE;
  Ufs->VsAddr     = (VOID *)(UINTN)GS101_UFS_VS_BASE;
  Ufs->PhyPma     = (VOID *)(UINTN)GS101_UFS_PHY_PMA_BASE;
  Ufs->UniProAddr = (VOID *)(UINTN)GS101_UFS_UNIPRO_BASE;

  Ufs->DevPwrAddr = (VOID *)(UINTN)(GS101_UFS_VS_BASE + GS101_UFS_VS_GPIO_OUT);
  Ufs->DevPwrShift = 0;

  Ufs->PhyIsoAddr = (VOID *)(UINTN)GS101_PMU_UFS_PHY_ISO;

  Ufs->MclkRate = MclkRate;
  Ufs->GearMode = 4;

  //
  // I/O coherency for the UFS host. Only add the missing bits, as pixelios
  // does: ABL may already have configured this and a blind write has no
  // documented effect on the surrounding fields.
  //
  Share = MmioRead32 (GS101_SYSREG_HSI1_IOCOHERENCY);
  if ((Share & GS101_SYSREG_HSI1_IOCOHERENCY_BITS) != GS101_SYSREG_HSI1_IOCOHERENCY_BITS) {
    MmioWrite32 (GS101_SYSREG_HSI1_IOCOHERENCY,
                 Share | GS101_SYSREG_HSI1_IOCOHERENCY_BITS);
    DEBUG ((DEBUG_INFO, "UFS: HSI1 IO coherency 0x%08x -> 0x%08x\n",
            Share, MmioRead32 (GS101_SYSREG_HSI1_IOCOHERENCY)));
  }

  DEBUG ((DEBUG_INFO, "UFS: GS101 board init, mclk %u Hz, 2 lanes, HS-G4\n", MclkRate));
  return EFI_SUCCESS;
}
