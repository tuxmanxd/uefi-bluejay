/** @file
  GS101 DeviceBootManagerLib: a text console for bring-up.

  bluejay has no reachable UART, so the framebuffer console is the only way to
  read state from the firmware. This override of the null implementation prints
  a short status block to ConOut, and replaces the "no bootable option"
  shutdown with a halt so the text stays on screen.

  It answers the questions the colour ladder cannot: did this overlay's code
  get linked at all, and did UFS publish a block device / filesystem.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <GS101.h>

/**
  Count the handles that publish a protocol.

  @param[in] Protocol  Protocol to count.
  @return Number of handles, or 0 when none (or on error).
**/
STATIC
UINTN
CountHandles (
  IN EFI_GUID  *Protocol
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       Count;

  Count  = 0;
  Status = gBS->LocateHandleBuffer (ByProtocol, Protocol, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  FreePool (Handles);
  return Count;
}

/**
  Print the firmware status block.

  @param[in] Tag  Short label identifying the point in the boot flow.
**/
STATIC
VOID
PrintStatus (
  IN CONST CHAR16  *Tag
  )
{
  UINTN  Columns;
  UINTN  Rows;

  Columns = 0;
  Rows    = 0;
  if ((gST->ConOut != NULL) && (gST->ConOut->Mode != NULL)) {
    gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows);
  }

  Print (L"\n[%s] GS101 UEFI: this overlay is running\n", Tag);
  Print (L" console %dx%d, block I/O handles %d, file system handles %d\n",
         Columns, Rows,
         CountHandles (&gEfiBlockIoProtocolGuid),
         CountHandles (&gEfiSimpleFileSystemProtocolGuid));

  //
  // Raw UFS host controller identification registers. Reading them proves the
  // register window is mapped; the values themselves are not interpreted here.
  //
  Print (L" UFS HCI CAP=0x%08x VER=0x%08x HCS=0x%08x\n",
         MmioRead32 (GS101_UFS_BASE + 0x00),
         MmioRead32 (GS101_UFS_BASE + 0x08),
         MmioRead32 (GS101_UFS_BASE + 0x30));
}

EFI_HANDLE
EFIAPI
DeviceBootManagerBeforeConsole (
  OUT EFI_DEVICE_PATH_PROTOCOL   **DevicePath,
  OUT BDS_CONSOLE_CONNECT_ENTRY **PlatformConsoles
  )
{
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerAfterConsole (
  VOID
  )
{
  PrintStatus (L"after console");
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerOnDemandConInConnect (
  VOID
  )
{
  return NULL;
}

VOID
EFIAPI
DeviceBootManagerProcessBootCompletion (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  return;
}

EFI_STATUS
EFIAPI
DeviceBootManagerPriorityBoot (
  IN OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  return EFI_SUCCESS;
}

VOID
EFIAPI
DeviceBootManagerUnableToBoot (
  VOID
  )
{
  //
  // BootManagerLib draws the "no operating system" graphic next and then
  // shuts the system down, which erases any text. Keep the diagnostics on
  // screen instead: this is a bring-up build, not a usable firmware yet.
  //
  PrintStatus (L"no boot option");
  Print (L" halting so the diagnostics stay readable\n");

  CpuDeadLoop ();
}

VOID
EFIAPI
DeviceBootManagerBdsEntry (
  VOID
  )
{
  return;
}
