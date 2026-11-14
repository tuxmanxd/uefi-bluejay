/** @file
  GS101 PlatformPeiLib.

  Replaces SiliciumNullPkg's null implementation so the SEC stage can report
  progress on the framebuffer: yellow means InitializeMemory() returned and
  the FV HOB was built, i.e. anything that hangs afterwards is in
  DecompressFirstFv() or the DxeCore load.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/PcdLib.h>
#include <Library/HobLib.h>

#include <GS101.h>

EFI_STATUS
EFIAPI
PlatformPeim (
  VOID
  )
{
  volatile UINT32  *Framebuffer;
  UINTN            Index;

  Framebuffer = (volatile UINT32 *)(UINTN)GS101_FRAMEBUFFER_BASE;

  for (Index = 0; Index < (GS101_FRAMEBUFFER_SIZE / sizeof (UINT32)); Index++) {
    Framebuffer[Index] = 0x00FFFF00;
  }

  // Build FV HOB
  BuildFvHob (PcdGet64 (PcdFvBaseAddress), PcdGet32 (PcdFvSize));

  return EFI_SUCCESS;
}
