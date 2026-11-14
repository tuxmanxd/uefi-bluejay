##
#  Files and applications every Mu-Silicium platform needs, minus the Samsung
#  UFP firmware-update applications (SiliciumPkg/Applications/UFP), which the
#  Pixel 6a does not use and which are not declared in SiliciumPkg.dsc.inc.
#
#  Keep this in sync with Silicon/Silicium/SiliciumPkg/Extra.fdf.inc when the
#  Mu-Silicium submodule is updated.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

  #
  # Remaining UEFI Drivers
  #
  INF MdeModulePkg/Universal/Acpi/BootGraphicsResourceTableDxe/BootGraphicsResourceTableDxe.inf
  INF MdeModulePkg/Universal/SetupBrowserDxe/SetupBrowserDxe.inf

  #
  # UEFI Applications
  #
  INF MdeModulePkg/Application/BootManagerMenuApp/BootManagerMenuApp.inf
  INF PcBdsPkg/MsBootPolicy/MsBootPolicy.inf

  #
  # RAW Files
  #
!if $(ENABLE_SECUREBOOT) == 1
  FILE FREEFORM = PCD (gSiliciumPkgTokenSpaceGuid.PcdSiPolicyFile) {
    SECTION RAW = SiliciumPkg/Include/Resources/SecureBoot/SiPolicy.p7b
    SECTION UI = "SiPolicy.p7b"
  }
!endif

  FILE FREEFORM = PCD (gOemPkgTokenSpaceGuid.PcdLogoFile) {
    SECTION RAW = SiliciumPkg/Include/Resources/BootLogo.bmp
    SECTION UI = "BootLogo.bmp"
  }

  FILE FREEFORM = PCD (gSiliciumPkgTokenSpaceGuid.PcdNoBootOSFile) {
    SECTION RAW = SiliciumPkg/Include/Resources/NoBootOS.bmp
    SECTION UI = "NoBootOS.bmp"
  }

  FILE FREEFORM = PCD (gSiliciumPkgTokenSpaceGuid.PcdMassStorageBmpFile) {
    SECTION RAW = SiliciumPkg/Include/Resources/MassStorage.bmp
    SECTION UI = "MassStorage.bmp"
  }
