# AGENTS.md

## What this repository is

Mu-Silicium platform + silicon packages for the Google Pixel 6a (`bluejay`,
Tensor G1 / `gs101`). It is not a standalone EDK2 tree: `scripts/apply.sh`
installs it into a Mu-Silicium checkout.

## Rules

- Every hardware address or constant must have a source in `docs/EVIDENCE.md`.
  Values that are decisions (FD base, heap base) belong in `docs/PLAN.md`.
- Do not invent register values, PHY rows, GPIO numbers or partition bounds.
  Mark unknown work as not implemented.
- Keep the Samsung `UfsDxe`, `UfsHostBridge` and `UfsCalAdapterLib` interface
  intact. GS101 differences belong in `GS101Pkg/Library/PlatformUfsLib`.
- `UfsPhyTables.h` is generated data from pixelios
  (`tools/gen-ufsphy-tables.py`). Change it only by regenerating there and
  retyping the values; never hand-edit.
- Platform code follows the surrounding Mu-Silicium style: EDK2 types,
  `MmioRead32`/`MmioWrite32`, `DEBUG` macros, 2-space indent.
- Use `gh` for repository operations.

## Verification

A change is only "verified" after it builds in a Mu-Silicium checkout:

```sh
scripts/apply.sh /path/to/Mu-Silicium copy
cd /path/to/Mu-Silicium
python3 build_uefi.py -d bluejay --edk2 Mu_Basecore
```

The FD must fit in `uefi_fd.size` (2 MiB). If it does not, grow the size and
the memory-map region together.
