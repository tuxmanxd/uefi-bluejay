/** @file
  GS101 UFS link/PHY calibration hooks.

  This is a transcription of the cold-init sequence that pixelios proves on a
  Pixel 6a (soc/google/gs101/shim/ufsinit.c, itself derived from the mainline
  phy-gs101-ufs.c, ufs-exynos.c and ufshcd.c). The host controller bring-up
  before these hooks (isolation bypass, software reset, device reset, HCE,
  clock ungating) is already done by SamsungPkg/Drivers/UfsDxe; only the PHY
  and UniPro programming that is specific to GS101 lives here.

  Call order in UfsDxe:
    UfsBoardInit -> UfsCalInit -> UfsPreSetup -> UfsCalPreLink -> link startup
    -> UfsCalPostLink -> UfsCalPrePmc -> power mode change -> UfsCalPostPmc

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UfsHostBridge.h>
#include <Library/UfsCalAdapterLib.h>

#include <GS101.h>
#include "UfsPhyTables.h"

//
// UFS power modes, as used by UfsDxe (UFS_POWER_MODE 1, UFS_RATE 2).
//
#define GS101_UFS_FAST_MODE   1
#define GS101_UFS_HS_MODE_B   2

//
// M-RX attribute selectors start after PA_MAXDATALANES (4).
//
#define GS101_UFS_MIB_RX(Lane)  (4U + (Lane))

//
// Vendor HC registers used by the post-link sequence.
//
#define GS101_UFS_VS_AXIDMA_BURST_LEN   0x006CU
#define GS101_UFS_VS_ERR_EN_0           0x007CU
#define GS101_UFS_VS_ERR_EN_1           0x0080U
#define GS101_UFS_VS_ERR_EN_2           0x0084U

#define GS101_UFS_VS_W(Ufs, Off, Val) \
  MmioWrite32 ((UINTN)(Ufs)->VsAddr + (Off), (Val))

STATIC
VOID
Gs101UfsDmeSet (
  VOID   *Host,
  UINT32 Attr,
  UINT32 Sel,
  UINT32 Val
  )
{
  ufs_lld_dme_set (Host, UIC_ARG_MIB_SEL (Attr, Sel), Val);
}

STATIC
VOID
Gs101UfsPhyWrite (
  VOID                     *Host,
  CONST GS101_UFS_PHY_ROW  *Rows,
  UINTN                     Count
  )
{
  UINTN Index;

  for (Index = 0; Index < Count; Index++) {
    ufs_lld_pma_write (Host, Rows[Index].Val, (UINT32)Rows[Index].Off << 2);
    if (Rows[Index].Trsv != 0) {
      ufs_lld_pma_write (Host, Rows[Index].Val,
                         ((UINT32)Rows[Index].Off + 0x200U) << 2);
    }
  }
}

STATIC
BOOLEAN
Gs101UfsPoll (
  UINTN   Address,
  UINT32  Mask,
  UINT32  Value,
  UINT32  TimeoutUs
  )
{
  UINT32 Elapsed;

  for (Elapsed = 0; Elapsed < TimeoutUs; Elapsed++) {
    if ((MmioRead32 (Address) & Mask) == Value) {
      return TRUE;
    }
    MicroSecondDelay (1);
  }

  return FALSE;
}

STATIC
UINT32
Gs101UfsMclkRate (
  struct UfsCalParam  *Param
  )
{
  if (Param->MclkRate != 0) {
    return Param->MclkRate;
  }

  //
  // UfsDxe always fills this from Ufs->MclkRate; the fallback documents the
  // only rate the bluejay bootloader leaves running.
  //
  return 166000000U;
}

UfsCalError
UfsCalInit (
  struct UfsCalParam  *Param
  )
{
  if (Param == NULL) {
    return UFS_CAL_INV_ARG;
  }

  DEBUG ((DEBUG_INFO, "UFS: GS101 calibration init\n"));
  return UFS_CAL_NO_ERROR;
}

UfsCalError
UfsCalPreLink (
  struct UfsCalParam  *Param
  )
{
  struct UfsHost  *Ufs;
  UINT32           Mclk;
  UINT32           Period;
  UINT32           RxLineReset;
  UINT32           TxLineReset;
  UINT32           Lane;
  UINT32           Lanes;

  if ((Param == NULL) || (Param->Host == NULL)) {
    return UFS_CAL_INV_ARG;
  }

  Ufs   = Param->Host;
  Lanes = Param->AvailableLane;
  if ((Lanes == 0) || (Lanes > 2)) {
    DEBUG ((DEBUG_ERROR, "UFS: GS101 supports 2 lanes, got %u\n", Lanes));
    return UFS_CAL_INV_ARG;
  }

  Mclk = Gs101UfsMclkRate (Param);

  //
  // Step 11: UFS error enables (the vendor block at VS+0x7C..0x84).
  //
  GS101_UFS_VS_W (Ufs, GS101_UFS_VS_ERR_EN_0, 0x80002020U);
  GS101_UFS_VS_W (Ufs, GS101_UFS_VS_ERR_EN_1, 0x80000007U);
  GS101_UFS_VS_W (Ufs, GS101_UFS_VS_ERR_EN_2, 0x80000017U);

  //
  // UniPro attributes. 0x1564 = PA_TXTRAILINGCLOCKS, 0x956A/0x956D are the
  // GS101 vendor attributes from the Linux driver.
  //
  Gs101UfsDmeSet (Ufs, 0x1564, 0, 0xFF);
  Gs101UfsDmeSet (Ufs, 0x956A, 0, 0x90913C1CU);
  Gs101UfsDmeSet (Ufs, 0x956D, 0, 0xE01C115FU);

  //
  // Step 12: restore COMP_CLK_PERIOD (the software reset reverted it) and
  // derive the UniPro timing constants from the ABL clock.
  //
  MmioWrite32 (GS101_UFS_UNIPRO_BASE + GS101_UNIPRO_MCLK_PERIOD,
               (UINT32)(GS101_UFS_MCLK_NUMERATOR / Mclk));

  Period      = (UINT32)((1000000000ULL + Mclk - 1) / Mclk);
  RxLineReset = (UINT32)(1000ULL * Mclk / 1000000ULL);
  TxLineReset = (UINT32)(3200ULL * Mclk / 1000000ULL);

  //
  // Step 13a: both directions must report two lanes before touching the PHY.
  // UfsDxe already read PA_ActiveRxDataLanes for us.
  //
  Gs101UfsDmeSet (Ufs, 0x0200, 0, 0x40);

  for (Lane = 0; Lane < Lanes; Lane++) {
    Gs101UfsDmeSet (Ufs, 0x0012, GS101_UFS_MIB_RX (Lane), Period);
    Gs101UfsDmeSet (Ufs, 0x0011, GS101_UFS_MIB_RX (Lane), 0);
    Gs101UfsDmeSet (Ufs, 0x001B, GS101_UFS_MIB_RX (Lane), (RxLineReset >> 16) & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x001C, GS101_UFS_MIB_RX (Lane), (RxLineReset >> 8) & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x001D, GS101_UFS_MIB_RX (Lane), RxLineReset & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x002F, GS101_UFS_MIB_RX (Lane), 0x69);
    Gs101UfsDmeSet (Ufs, 0x0084, GS101_UFS_MIB_RX (Lane), 0x01);
    Gs101UfsDmeSet (Ufs, 0x0025, GS101_UFS_MIB_RX (Lane), 0xF6);
  }

  for (Lane = 0; Lane < Lanes; Lane++) {
    Gs101UfsDmeSet (Ufs, 0x00AA, Lane, Period);
    Gs101UfsDmeSet (Ufs, 0x00A9, Lane, 0x02);
    Gs101UfsDmeSet (Ufs, 0x00AB, Lane, (TxLineReset >> 16) & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x00AC, Lane, (TxLineReset >> 8) & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x00AD, Lane, TxLineReset & 0xFF);
    Gs101UfsDmeSet (Ufs, 0x0004, Lane, 1);
    Gs101UfsDmeSet (Ufs, 0x007F, Lane, 0);
  }

  Gs101UfsDmeSet (Ufs, 0x0200, 0, 0x00);
  Gs101UfsDmeSet (Ufs, 0x155E, 0, 0x00);
  Gs101UfsDmeSet (Ufs, 0x3000, 0, 0x00);
  Gs101UfsDmeSet (Ufs, 0x3001, 0, 0x01);
  Gs101UfsDmeSet (Ufs, 0x4021, 0, 0x01);
  Gs101UfsDmeSet (Ufs, 0x4020, 0, 0x01);
  Gs101UfsDmeSet (Ufs, 0xA006, 0, 0x8000);

  //
  // Step 13: PHY pre-init table, then wait for RX calibration on each lane.
  //
  Gs101UfsPhyWrite (Ufs, gGs101UfsPhyPreInitCfg,
                    ARRAY_SIZE (gGs101UfsPhyPreInitCfg));

  for (Lane = 0; Lane < Lanes; Lane++) {
    if (!Gs101UfsPoll ((UINTN)Ufs->PhyPma + (((0x338U + Lane * 0x200U) << 2)),
                       0x8, 0x8, 40000))
    {
      DEBUG ((DEBUG_ERROR, "UFS: PHY RX calibration timeout on lane %u\n", Lane));
      return UFS_CAL_TIMEOUT;
    }
  }

  DEBUG ((DEBUG_INFO, "UFS: pre-link done, mclk %u Hz, %u lanes\n", Mclk, Lanes));
  return UFS_CAL_NO_ERROR;
}

UfsCalError
UfsCalPostLink (
  struct UfsCalParam  *Param
  )
{
  struct UfsHost  *Ufs;

  if ((Param == NULL) || (Param->Host == NULL)) {
    return UFS_CAL_INV_ARG;
  }

  Ufs = Param->Host;

  //
  // Step 16: post-link UniPro state.
  //
  Gs101UfsDmeSet (Ufs, 0x4020, 0, 0);
  Gs101UfsDmeSet (Ufs, 0x3000, 0, 0);
  Gs101UfsDmeSet (Ufs, 0x3001, 0, 1);
  Gs101UfsDmeSet (Ufs, 0x4021, 0, 1);
  Gs101UfsDmeSet (Ufs, 0x4022, 0, 0);
  Gs101UfsDmeSet (Ufs, 0x4025, 0, 6);
  Gs101UfsDmeSet (Ufs, 0x4023, 0, 0);
  Gs101UfsDmeSet (Ufs, 0x4020, 0, 1);

  //
  // GS101 DMA burst / auto-hibernate unit configuration. UfsDxe programs the
  // rest of the vendor block in UfsVendorSetup; these writes are not covered
  // there and are part of the proven pixelios sequence.
  //
  GS101_UFS_VS_W (Ufs, GS101_UFS_VS_AXIDMA_BURST_LEN, 0x0000000FU);
  Gs101UfsDmeSet (Ufs, 0x9529, 0, 1);
  Gs101UfsDmeSet (Ufs, 0x15AA, 0, 6);
  Gs101UfsDmeSet (Ufs, 0x9529, 0, 0);
  Gs101UfsDmeSet (Ufs, 0x15A8, 0, 3);
  Gs101UfsDmeSet (Ufs, 0x15A7, 0, 0x20);
  GS101_UFS_VS_W (Ufs, GS101_UFS_VS_AXIDMA_BURST_LEN, 0x98000003U);
  Gs101UfsDmeSet (Ufs, 0x9529, 0, 1);
  Gs101UfsDmeSet (Ufs, 0x15A4, 0, 0x3E8);
  Gs101UfsDmeSet (Ufs, 0x9529, 0, 0);

  DEBUG ((DEBUG_INFO, "UFS: post-link done\n"));
  return UFS_CAL_NO_ERROR;
}

UfsCalError
UfsCalPrePmc (
  struct UfsCalParam  *Param
  )
{
  struct UfsHost  *Ufs;

  if ((Param == NULL) || (Param->Host == NULL) || (Param->Pmd == NULL)) {
    return UFS_CAL_INV_ARG;
  }

  //
  // HS-G4 Series B is the only mode both the controller and the Pixel 6a
  // device are qualified for here. UfsDxe asks for exactly this mode
  // (UFS_POWER_MODE 1, UFS_RATE 2). Anything else stays in PWM, which the
  // post-link sequence above already left usable.
  //
  if ((Param->Pmd->Mode != GS101_UFS_FAST_MODE) ||
      (Param->Pmd->HsSeries != GS101_UFS_HS_MODE_B))
  {
    DEBUG ((DEBUG_WARN, "UFS: skipping GS101 HS PHY config for mode %u series %u\n",
            Param->Pmd->Mode, Param->Pmd->HsSeries));
    return UFS_CAL_NO_ERROR;
  }

  Ufs = Param->Host;

  Gs101UfsPhyWrite (Ufs, gGs101UfsPhyPrePwrHsCfg,
                    ARRAY_SIZE (gGs101UfsPhyPrePwrHsCfg));

  Gs101UfsDmeSet (Ufs, 0x2041, 0, 8064);
  Gs101UfsDmeSet (Ufs, 0x2042, 0, 28224);
  Gs101UfsDmeSet (Ufs, 0x2043, 0, 20160);
  Gs101UfsDmeSet (Ufs, 0x15B0, 0, 12000);
  Gs101UfsDmeSet (Ufs, 0x15B1, 0, 32000);
  Gs101UfsDmeSet (Ufs, 0x15B2, 0, 16000);

  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x7888, 8064);
  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x788C, 28224);
  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x7890, 20160);
  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x78B8, 12000);
  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x78BC, 32000);
  MmioWrite32 ((UINTN)Ufs->UniProAddr + 0x78C0, 16000);

  DEBUG ((DEBUG_INFO, "UFS: pre-PMC HS PHY config done\n"));
  return UFS_CAL_NO_ERROR;
}

UfsCalError
UfsCalPostPmc (
  struct UfsCalParam  *Param
  )
{
  struct UfsHost  *Ufs;
  UINT32           Lane;
  UINT32           Lanes;
  UINT32           Base;
  UINT32           Try;
  BOOLEAN          Locked;

  if ((Param == NULL) || (Param->Host == NULL) || (Param->Pmd == NULL)) {
    return UFS_CAL_INV_ARG;
  }

  if ((Param->Pmd->Mode != GS101_UFS_FAST_MODE) ||
      (Param->Pmd->HsSeries != GS101_UFS_HS_MODE_B))
  {
    return UFS_CAL_NO_ERROR;
  }

  Ufs   = Param->Host;
  Lanes = Param->Pmd->Lane;
  if ((Lanes == 0) || (Lanes > 2)) {
    Lanes = Param->AvailableLane;
  }

  Gs101UfsPhyWrite (Ufs, gGs101UfsPhyPostPwrHsCfg,
                    ARRAY_SIZE (gGs101UfsPhyPostPwrHsCfg));

  //
  // CDR lock. The retry writes 0x10 then 0x18 to the AFC control register,
  // exactly as the GS101 Linux PHY driver does when the first check misses.
  //
  for (Lane = 0; Lane < Lanes; Lane++) {
    Base   = Lane * 0x200U;
    Locked = FALSE;
    for (Try = 0; Try < 100 && !Locked; Try++) {
      MicroSecondDelay (40);
      if ((ufs_lld_pma_read (Ufs, (0x339U + Base) << 2) & 0x8) == 0x8) {
        Locked = TRUE;
      } else {
        ufs_lld_pma_write (Ufs, 0x10, (0x222U + Base) << 2);
        ufs_lld_pma_write (Ufs, 0x18, (0x222U + Base) << 2);
      }
    }

    if (!Locked) {
      DEBUG ((DEBUG_ERROR, "UFS: CDR lock timeout on lane %u\n", Lane));
      return UFS_CAL_TIMEOUT;
    }
  }

  DEBUG ((DEBUG_INFO, "UFS: HS-G4 Series B up, %u lanes\n", Lanes));
  return UFS_CAL_NO_ERROR;
}

UINT8
UfsCalGetTargetBoard (
  VOID
  )
{
  //
  // Only the embedded (soldered) UFS device exists on bluejay.
  //
  return 0;
}
