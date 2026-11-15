/** @file
  Bluejay (Pixel 6a) physical memory map for the Mu-Silicium SEC/DXE core.

  Only regions the firmware actually touches are listed. Everything else in
  the low 2 GiB bank is bootloader/modem/TZ/TPU/AoC/GSA RAM with no-map
  semantics, so leaving it out of this map is intentional: DXE must never
  create a cacheable mapping for it.

  The low DDR regions come from the measured bluejay FDT:
    * FD_BASE 0xF9800000, the 20 MiB hole between abl@f8800000 (+16 MiB)
      and the framebuffer at 0xFAC00000.
    * DXE heap 0xFB600000, the hole between the framebuffer end
      (0xFB5E3400) and dss_log_reserved@fd3f0000.
  These are planned load addresses, not values reported by ABL; they are
  documented in docs/PLAN.md and must be confirmed on the first hardware run.

  The high bank is the phone's upper 4 GiB of DRAM, measured from the
  bootloader FDT (docs/EVIDENCE.md): two contiguous 2 GiB banks at
  0x880000000 and 0x900000000 with no reserved range inside. The first
  64 MiB, 0x880000000..0x884000000, is the payload window: BootShim copies
  the UniXNU payload volume there before the MMU is on, so it is mapped but
  reserved from DXE allocation. XNU's memory node starts after it.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/MemoryMapLib.h>

STATIC
EFI_MEMORY_REGION_DESCRIPTOR
gMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute

  // DDR Regions
  {"UEFI FD",            0xF9800000, 0x00200000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"UEFI Stack",         0xF9A00000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"DXE Heap",           0xFB600000, 0x01DF0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK},
  // Payload window first, then the rest of the high DRAM: no overlapping
  // descriptors, so the resource HOBs stay disjoint.
  {"Payload Window",     0x880000000, 0x04000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  {"High RAM",           0x884000000, 0x0FC000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,  WRITE_BACK},
  // The panel is 1080x2400x32bpp = 0x009E3400 bytes, which is not a multiple
  // of the 4 KB translation granule. ArmConfigureMmu() rejects a region that
  // is not page aligned, so the reservation is rounded up to 0x009E4000; the
  // extra page is still inside the hole before the DXE heap at 0xFB600000.
  {"Display Reserved",   0xFAC00000, 0x009E4000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH},

  // Register Regions
  {"GICD",               0x10400000, 0x00010000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"GICR",               0x10440000, 0x00100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Watchdog CL0",       0x10060000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"Watchdog CL1",       0x10070000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"UART",               0x10A00000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"SYSREG HSI1",        0x14420000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"UFS",                0x14700000, 0x00100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"PMU",                0x17460000, 0x00010000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE},
  {"DRM Decon",          0x1C300000, 0x00004000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}
};

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount)
{
  // Pass Data
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = ARRAY_SIZE (gMemoryDescriptor);
}
