# uefi-bluejay

Mu-Silicium (Project Mu) platform for the **Google Pixel 6a** — `bluejay`,
Google Tensor G1 (`gs101`).

This repository contains only the additions Mu-Silicium does not ship:

```
Platforms/Google/bluejayPkg/    platform package: DSC, FDF, DEC, DeviceBuild.py,
                                MemoryMapLib, Include/Extra.fdf.inc
Silicon/Google/GS101Pkg/        SoC package: PlatformSecLib (watchdogs, DECON),
                                PlatformUfsLib (GS101 UFS/PHY calibration),
                                ChipInfoDxe
Resources/Configs/bluejay.toml  FD base/size and boot-image settings
scripts/apply.sh                install both packages into a Mu-Silicium tree
```

`Include/Extra.fdf.inc` carries the Mu-Silicium FV extras (boot menu, logo
blobs) without the Samsung UFP firmware-update applications; the upstream
`SiliciumPkg/Extra.fdf.inc` lists `ufpdevicefw.inf`, which is not declared in
`SiliciumPkg.dsc.inc`.

`Silicon/Samsung/SamsungPkg`'s `UfsDxe` host controller driver is reused as-is;
GS101 only supplies the board addresses and the PHY/UniPro calibration hooks.

## Status

| Area | State |
|---|---|
| Platform package, memory map, FDF/DSC | written, not built here |
| UFS host bring-up (HCI reset, HCE, link start) | reused Samsung `UfsDxe` |
| GS101 PHY pre-init, post-link, HS-G4 Series B | transcribed from pixelios, not run on hardware |
| Watchdog stop, DECON trigger re-arm | transcribed from pixelios, not run on hardware |
| Framebuffer console (1080x2400) | expected to work (ABL display left running) |
| UniXNU modules (UniXNU, UniXNUDxe, UniXNURamDiskDxe, DarwinBoot) | wired behind `UNIXNU_PAYLOAD=1`, not built |
| Buttons / keypad | not implemented |
| USB device/host, PMIC, touch | not implemented |
| Build verified against a Mu-Silicium tree | no (no EDK2 toolchain in this environment) |

Read `docs/PLAN.md` before the first flash: the FD load address and the DXE
heap are *planned* addresses in measured free holes, not values reported by
ABL, and must be confirmed by a probe run.

## Build

The firmware is built inside a Mu-Silicium checkout (the `uefi-sheng` fork is
the one UniXNU's other ports use; any recent Mu-Silicium with
`Silicon/Samsung/SamsungPkg` works):

```sh
git clone --recursive https://github.com/Project-Silicium/Mu-Silicium.git
cd Mu-Silicium
git submodule update --init --recursive

/path/to/uefi-bluejay/scripts/apply.sh "$PWD"

python3 build_uefi.py -d bluejay --edk2 Mu_Basecore
```

Outputs:

- `Build/bluejayPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd`
- `Mu-bluejay-0.img` (boot shim + FD, Android boot header v4)

`build_uefi.py` also needs its `Binaries`, `Common/Mu`, `Common/Mu_OEM_Sample`,
`Mu_Basecore`, `Silicium-ACPI` and OpenSSL submodules initialised.

## UniXNU firmware

With `-D UNIXNU_PAYLOAD=1` (the UniXNU device repository sets it in
`board.toml [firmware.defines]`) the DSC and FDF also add `UniXNU.inf`,
`UniXNUDxe.inf`, `UniXNURamDiskDxe.inf` and `DarwinBoot.inf`, and
`PACKAGES_PATH` gains `{tree}/package` and `{tree}/drivers/DarwinBoot`. The
default `UNIXNU_PAYLOAD=0` keeps the tree buildable on its own. UniXNU builds
it with:

```sh
./unixnu build firmware --uefi /path/to/Mu-Silicium --edk2 /path/to/Mu_Basecore
```

`HobLib` (`DxeHobLib`) and `FdtLib` (`BaseFdtLib`) come from
`SiliciumPkg.dsc.inc`.

## Flash

```sh
fastboot flash boot Mu-bluejay-0.img
```

The Pixel 6a must be unlocked. The boot shim relocates the FD to
`0xF9800000`; the FD is 2 MiB (`uefi_fd` size in `Resources/Configs/bluejay.toml`).

## Evidence

Every register and address used here has a source; see `docs/EVIDENCE.md`.
Values that are decisions rather than measurements (FD base, DXE heap) are
marked as such in `docs/PLAN.md` and in the code.

## License

BSD-2-Clause-Patent, matching Mu-Silicium. The UFS PHY tables originate from
Linux (phy-gs101-ufs.c) via pixelios and keep their values verbatim.
