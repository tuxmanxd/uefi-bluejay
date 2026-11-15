##
#  Google Pixel 6a (bluejay / Tensor G1, GS101)
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  PLATFORM_NAME                  = bluejay
  PLATFORM_GUID                  = 2C0F7B1E-16A4-4C5B-9E3A-7D6B9A0C4F21
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/bluejayPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = bluejayPkg/bluejay.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # SiliciumPkg.dsc.inc passes -D ENABLE_SECUREBOOT=$(ENABLE_SECUREBOOT) to the
  # compiler, so the variable must exist even when the build is driven directly
  # (UniXNU calls BaseTools' build, not build_uefi.py, which sets it).
  #
  ENABLE_SECUREBOOT              = 0

  #
  # UniXNU firmware: 0 builds a plain Mu-Silicium UEFI tree on its own,
  # 1 adds UniXNU, UniXNUDxe, UniXNURamDiskDxe and DarwinBoot. The UniXNU
  # release sets it in board.toml [firmware.defines] and adds the UniXNU
  # package paths to PACKAGES_PATH.
  #
  UNIXNU_PAYLOAD                 = 0

!include GS101Pkg/GS101Pkg.dsc.inc

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack (must match the "UEFI Stack" MemoryMapLib entry)
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0xF9A00000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x00040000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Google"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Pixel 6a"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"bluejay"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"Pixel_6a_bluejay"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"Pixel 6a"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorModel|"Google Tensor (GS101)"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumber|"GS101"

  #
  # Simple Frame Buffer: measured panel mode 1080x2400, 32 bpp a8r8g8b8
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1080
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2400
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|bluejayPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # SEC progress: paints the framebuffer once InitializeMemory() has returned.
  #
  PlatformPeiLib|GS101Pkg/Library/PlatformPeiLib/PlatformPeiLib.inf

!if $(UNIXNU_PAYLOAD) == 1
  #
  # UniXNU Libraries
  #
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
!endif

[Components]
!if $(UNIXNU_PAYLOAD) == 1
  #
  # UniXNU: payload volume block device, boot menu, boot option registration,
  # FDT initrd block device and the XNU loader. HobLib and FdtLib come from
  # SiliciumPkg.dsc.inc.
  #
  bluejayPkg/Driver/PayloadBlockIo/PayloadBlockIo.inf
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf
!endif
