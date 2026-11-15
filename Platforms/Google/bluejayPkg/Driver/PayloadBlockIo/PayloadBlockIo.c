/** @file
  Publish the UniXNU payload volume as a read-only block device.

  Tensor ABL refuses a boot image whose ramdisk is larger than 32 MiB, and the
  prelinked kernel alone is 49.5 MiB, so the payload cannot be the ramdisk.
  BootShim copies it instead from the boot image kernel field into the 64 MiB
  payload window at 0x880000000 (see MemoryMapLib). This driver validates the
  FAT boot sector in that window and installs BlockIo, so FatDxe mounts it and
  UniXNULib finds EFI/UniXNU/unixnu.toml on it.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

//
// Must match BootShim.S (apply.sh, payload copy) and the "Payload Window"
// MemoryMapLib entry.
//
#define PAYLOAD_BASE  0x880000000ULL
#define PAYLOAD_LIMIT 0x04000000ULL

#define BLOCK_SIZE 512

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} PAYLOAD_BLOCK_DEVICE_PATH;
#pragma pack()

STATIC PAYLOAD_BLOCK_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } },
    { 0x8C4D2E51, 0x3A6B, 0x4F27, { 0x9E, 0x15, 0x2D, 0x7B, 0x8C, 0x1A, 0x6F, 0x40 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC UINT8               *mPayloadBase;
STATIC EFI_BLOCK_IO_MEDIA  mMedia;

STATIC
EFI_STATUS
EFIAPI
PayloadBlockReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PayloadBlockRead (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (MediaId != mMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if ((BufferSize % BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Lba > mMedia.LastBlock) || ((BufferSize / BLOCK_SIZE) > (mMedia.LastBlock + 1 - Lba))) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mPayloadBase + (UINTN)Lba * BLOCK_SIZE, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
PayloadBlockWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
PayloadBlockFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mMedia,
  PayloadBlockReset,
  PayloadBlockRead,
  PayloadBlockWrite,
  PayloadBlockFlush
};

EFI_STATUS
EFIAPI
PayloadBlockIoEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT16      BytesPerSector;
  UINT32      TotalSectors;
  EFI_HANDLE  Handle;

  mPayloadBase = (UINT8 *)(UINTN)PAYLOAD_BASE;

  if ((*(UINT16 *)(mPayloadBase + 510) != 0xAA55) ||
      (*(UINT16 *)(mPayloadBase + 11) != BLOCK_SIZE))
  {
    DEBUG ((DEBUG_ERROR, "PAYLOADBLOCK: no FAT volume at 0x%lx\n", (UINT64)PAYLOAD_BASE));
    return EFI_SUCCESS;
  }

  BytesPerSector = *(UINT16 *)(mPayloadBase + 11);
  TotalSectors   = *(UINT32 *)(mPayloadBase + 0x20);
  if ((TotalSectors == 0) || ((UINT64)TotalSectors * BytesPerSector > PAYLOAD_LIMIT)) {
    DEBUG ((DEBUG_ERROR, "PAYLOADBLOCK: invalid volume size %u sectors\n", TotalSectors));
    return EFI_SUCCESS;
  }

  mMedia.MediaId          = 1;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = BLOCK_SIZE;
  mMedia.IoAlign          = 1;
  mMedia.LastBlock        = TotalSectors - 1;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "PAYLOADBLOCK: volume 0x%lx size 0x%lx %r\n",
          (UINT64)PAYLOAD_BASE, (UINT64)TotalSectors * BytesPerSector, Status));
  return EFI_SUCCESS;
}
