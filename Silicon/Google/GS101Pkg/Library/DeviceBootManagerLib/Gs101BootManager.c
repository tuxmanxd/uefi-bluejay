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
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>

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
  Print the partition table of the largest non-logical block device.

  A successful read proves the UFS driver, its DMA buffers and the partition
  parsing all work; the printed names come from the on-disk GPT, not from
  assumptions in this tree.
**/
STATIC
VOID
PrintPartitionTable (
  VOID
  )
{
  EFI_STATUS           Status;
  EFI_HANDLE           *Handles;
  UINTN                Count;
  UINTN                Index;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  EFI_BLOCK_IO_PROTOCOL *Disk;
  UINT64                DiskLastBlock;
  VOID                  *Buffer;
  UINT8                 *Bytes;

  Disk          = NULL;
  DiskLastBlock = 0;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    return;
  }

  for (Index = 0; Index < Count; Index++) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo))) {
      continue;
    }

    if ((BlockIo->Media == NULL) || BlockIo->Media->LogicalPartition) {
      continue;
    }

    if (BlockIo->Media->LastBlock > DiskLastBlock) {
      DiskLastBlock = BlockIo->Media->LastBlock;
      Disk          = BlockIo;
    }
  }

  FreePool (Handles);

  if (Disk == NULL) {
    Print (L" no whole-disk block device\n");
    return;
  }

  Print (L" disk block size %d, last LBA 0x%lx\n", Disk->Media->BlockSize, DiskLastBlock);

  //
  // Read LBA 1 (the GPT header) into page-aligned memory: the UFS driver hands
  // the buffer straight to the controller.
  //
  Buffer = AllocatePages (1);
  if (Buffer == NULL) {
    return;
  }

  Status = Disk->ReadBlocks (Disk, Disk->Media->MediaId, 1, Disk->Media->BlockSize, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L" reading LBA 1 failed: %r\n", Status);
    FreePages (Buffer, 1);
    return;
  }

  Bytes = (UINT8 *)Buffer;
  if ((CompareMem (Bytes, "EFI PART", 8)) != 0) {
    Print (L" LBA 1 has no GPT signature\n");
    FreePages (Buffer, 1);
    return;
  }

  //
  // GPT header: disk GUID at 0x38, first entry LBA at 0x48, entry count at
  // 0x50 (UEFI spec 2.10, 5.3.3).
  //
  Print (L" GPT present, disk GUID %g, entry count %d\n",
         (EFI_GUID *)(Bytes + 0x38),
         *(UINT32 *)(Bytes + 0x50));

  FreePages (Buffer, 1);
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

  PrintPartitionTable ();
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
