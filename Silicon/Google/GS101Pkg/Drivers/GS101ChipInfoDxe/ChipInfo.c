/** @file
  Minimal gEfiChipInfoProtocolGuid producer for GS101.

  SamsungPkg's UfsDxe locates this protocol and uses GetRevision to pick a
  PHY table. The GS101 calibration in this tree does not use the revision, so
  "unknown" (0.0) is the honest answer until the chip-id/revision register is
  measured on hardware.

  GetId returns 0x8015, the chip id UniXNU/pixelios publishes in the Apple
  Device Tree for bluejay (tools/mkadt.py). It is a stable identifier, not a
  hardware register read.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <GS101.h>
#include <Protocol/EFIChipInfo.h>

#define GS101_CHIP_ID  0x8015ULL

STATIC
EFI_STATUS
EFIAPI
Gs101GetChipId (
  OUT UINT64  *Id
  )
{
  if (Id == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Id = GS101_CHIP_ID;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
Gs101GetChipRevision (
  OUT UINT8  *MajorRev,
  OUT UINT8  *MinorRev
  )
{
  if ((MajorRev == NULL) || (MinorRev == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Not measured. The only consumer in this tree (UfsDxe) passes the value
  // to PlatformUfsLib, which ignores it on GS101.
  //
  *MajorRev = 0;
  *MinorRev = 0;
  return EFI_SUCCESS;
}

STATIC EFI_CHIP_INFO_PROTOCOL mChipInfo = {
  Gs101GetChipId,
  Gs101GetChipRevision
};

EFI_STATUS
EFIAPI
RegisterChipInfo (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  //
  // DXE progress marker: SEC paints red, green and blue; this turns the
  // screen magenta once driver dispatch reached this platform driver.
  //
  for (Index = 0; Index < (GS101_FRAMEBUFFER_SIZE / sizeof (UINT32)); Index++) {
    ((volatile UINT32 *)(UINTN)GS101_FRAMEBUFFER_BASE)[Index] = 0x00FF00FF;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiChipInfoProtocolGuid,
                  &mChipInfo,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ChipInfo: install failed: %r\n", Status));
  }

  return Status;
}
