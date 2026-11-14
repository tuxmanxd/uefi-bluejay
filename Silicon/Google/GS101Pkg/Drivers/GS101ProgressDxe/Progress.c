/** @file
  DXE dispatch progress marker for GS101.

  The only boot-time feedback channel on bluejay is the framebuffer, so each
  stage paints a different colour:

    red    - SEC reached C code, framebuffer writable
    green  - DECON0 trigger re-armed
    blue   - cluster watchdogs cleared
    orange - DxeCore dispatched this driver (first entry in the APRIORI list)
    magenta- GS101ChipInfoDxe entry, i.e. dispatch reached the platform drivers

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <GS101.h>

EFI_STATUS
EFIAPI
Gs101ProgressEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  volatile UINT32  *Framebuffer;
  UINTN            Index;

  Framebuffer = (volatile UINT32 *)(UINTN)GS101_FRAMEBUFFER_BASE;

  for (Index = 0; Index < (GS101_FRAMEBUFFER_SIZE / sizeof (UINT32)); Index++) {
    Framebuffer[Index] = 0x00FF8000;
  }

  return EFI_SUCCESS;
}
