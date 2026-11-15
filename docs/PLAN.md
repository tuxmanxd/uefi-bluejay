# Bring-up plan and open risks

## Memory layout

Measured low-bank map (bluejay FDT) leaves these holes around the framebuffer:

```
0xF8800000  abl                        16 MiB
0xF9800000  <-- planned UEFI FD          2 MiB
0xF9A00000  <-- planned UEFI stack     256 KiB
0xFAC00000  framebuffer               ~9.9 MiB (ends 0xFB5E3400)
0xFB600000  <-- planned DXE heap      ~31 MiB (ends 0xFD3F0000, dss_log)
```

The FD/stack hole is between ABL and the framebuffer; the heap hole is between
the framebuffer and the logging reservations. Both are **planned**, not
observed allocations. If ABL still owns either range on a real device, the
first boot will fail before the framebuffer appears.

The upper 4 GiB (measured, see EVIDENCE.md) is mapped as conventional memory
from `0x884000000` to `0x980000000`. Its first 64 MiB, `0x880000000` +
`0x04000000`, is the **payload window**: `BootShim` copies the UniXNU payload
volume there from the boot image kernel field (Tensor ABL caps the ramdisk at
32 MiB, the kernel is 49.5 MiB), `PayloadBlockIo` publishes it as a read-only
`BlockIo`, and `FatDxe` mounts it. The window is reserved in the UEFI memory
map and excluded from XNU's memory node, which starts at `0x884000000`.

Fallback if `0xF9800000` is not writable: move the FD and stack into the
0xFB600000 hole and shrink the DXE heap, or grow the FD into a larger hole and
adjust `Resources/Configs/bluejay.toml` and `MemoryMapLib.c` together.

## Boot sequence

1. ABL verifies and loads `Mu-bluejay-0.img` (Android boot header v4).
2. `BootShim` copies the appended FD to `0xF9800000` and jumps to `_ModuleEntryPoint`.
3. SEC `PlatformInitialize` (GS101 `PlatformSecLib`) stops both watchdogs and
   re-arms the DECON trigger.
4. DXE loads `SimpleFbDxe` (writes go to the bootloader framebuffer), then
   `UfsDxe` (GS101 `PlatformUfsLib`), then BDS.
5. BDS autoboots `\EFI\BOOT\BOOTAA64.EFI` from the ESP.

## UFS open questions

- The Samsung `UfsDxe` writes `VS_UTRL_NEXUS_TYPE`/`VS_UMTRL_NEXUS_TYPE` as
  `0xFFFFFFFF`. pixelios uses `0xFFFFFFFE` for the UTRL so that slot 0 is a
  device-management nexus. If the first UTP query (NOP OUT in `UfsEndBootMode`)
  times out, this is the first thing to change.
- Samsung `UfsDxe` does not write GS101's `VS+0x6C` DMA burst value from
  `UfsVendorSetup`; this tree writes it in `UfsCalPostLink` instead.
- HS-G4 is the only mode implemented. If CDR lock fails, the link stays in PWM
  only when the mode check rejects the change; a partially switched link is
  reported as an error, never silently accepted.

## Not implemented

- Keypad/buttons (Pixel 6a keys are PMIC/GPIO based and need the PMIC map).
- USB (DWC3 at `0x11110000`) and USB mass storage.
- PMIC regulators, charger, fuel gauge.
- Touch (FTM5) and panel backlight control.
- ACPI tables (`Silicium-ACPI` ships none for Google platforms).

These do not block an ESP boot; they block a phone-shaped experience.

## Validation done in this repository

- Package layout mirrors `Platforms/Samsung/e1sPkg` + `S5E9945Pkg`.
- `Resources/Configs/bluejay.toml` matches the `build_uefi.py` contract and
  agrees with `MemoryMapLib` (FD base/size) and the DSC (stack base).
- UFS tables diffed against the pixelios generated header (values unchanged).
- Textual preflight: every `!include`/`INF` whose package tree is present in
  the checkout resolves, every such INF is in a `[Components]` section, the
  memory regions do not overlap and the FD fits between `abl` and the
  framebuffer.
- `scripts/apply.sh` was run against a Mu-Silicium checkout in `copy` mode.
- No EDK2 compile was run: this environment has no Mu-Silicium submodules or
  EDK2 toolchain. The first build must happen in a full checkout.

## Known integration gaps

- `ufpdevicefw.inf` from `SiliciumPkg/Extra.fdf.inc` is not declared in
  `SiliciumPkg.dsc.inc`, so this platform uses its own
  `Include/Extra.fdf.inc` without the UFP applications.
- `GS101Pkg.dsc.inc` includes `SamsungPkg/SamsungPkg.dsc.inc`, which declares
  Samsung's generic `ChipInfoDxe` as a component. It is not placed in the FV;
  only `GS101ChipInfoDxe` runs.
