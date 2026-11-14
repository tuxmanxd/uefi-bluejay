##
#  Google Tensor G1 (GS101) silicon package
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  #
  # Timer: the bootloader leaves CNTFRQ_EL0 = 24.576 MHz and the physical
  # counter running, so use the physical timer (same choice as Exynos).
  #
  USE_PHYSICAL_TIMER                         = 1

  #
  # CNTFRQ_EL0 is programmed by ABL; no empty-register errata workaround.
  #
  HAS_CNTFRQ_EL0_EMPTY_ERRATA                = 0

#
# SamsungPkg brings the SEC/DXE defaults and, through it, SiliciumPkg. Its
# null GPU/PMIC/USB libraries are unused on bluejay; the UFS library class is
# overridden below with the GS101 implementation.
#
!include SamsungPkg/SamsungPkg.dsc.inc

[BuildOptions]
  #
  # Cortex-A55/A76/X1 on GS101 are ARMv8.2-A implementations.
  #
  *_*_AARCH64_CC_FLAGS = -march=armv8.2-a+crypto

[PcdsFixedAtBuild]
  #
  # ACPI
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemId|"GOOGLE"

  #
  # CPU: 4x Cortex-A55 + 2x Cortex-A76 + 2x Cortex-X1, three clusters
  #
  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|3

  #
  # GIC: distributor @0x10400000 (+64 KiB), redistributors @0x10440000 (+1 MiB)
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x10400000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x10440000

[LibraryClasses]
  #
  # SEC
  #
  PlatformSecLib|GS101Pkg/Library/PlatformSecLib/PlatformSecLib.inf

  #
  # Storage (overrides the Samsung null implementation)
  #
  PlatformUfsLib|GS101Pkg/Library/PlatformUfsLib/PlatformUfsLib.inf

[Components]
  #
  # Boot progress marker: paints the framebuffer from the first DXE dispatch.
  #
  GS101Pkg/Drivers/GS101ProgressDxe/GS101ProgressDxe.inf

  #
  # Chip info: UfsDxe locates gEfiChipInfoProtocolGuid and reads GetRevision.
  # UfsDxe itself is declared by SamsungPkg.dsc.inc above.
  #
  GS101Pkg/Drivers/GS101ChipInfoDxe/GS101ChipInfoDxe.inf
