#!/usr/bin/env bash
#
# Install the bluejay platform + GS101 silicon packages into a Mu-Silicium
# checkout. Mu-Silicium's build scripts discover platforms by globbing
# Platforms/*/*Pkg, so no source file outside these directories is modified.
#
# Usage:
#   scripts/apply.sh /path/to/Mu-Silicium [link|copy]
#
# link (default) creates symlinks, which keeps this repository the single
# source of truth. copy makes real copies for a throwaway build tree.
#
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: $0 /path/to/Mu-Silicium [link|copy]" >&2
  exit 2
fi

MU=$(cd "$1" && pwd)
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODE=${2:-link}

if [ ! -d "$MU/Silicon/Silicium/SiliciumPkg" ] || [ ! -d "$MU/Silicon/Samsung/SamsungPkg" ]; then
  echo "error: $MU does not look like a Mu-Silicium checkout" >&2
  echo "       (Silicon/Silicium/SiliciumPkg or Silicon/Samsung/SamsungPkg missing)" >&2
  exit 1
fi

case "$MODE" in
  link|copy) ;;
  *) echo "error: mode must be 'link' or 'copy'" >&2; exit 2 ;;
esac

install_path () {
  local src=$1 dst=$2
  rm -rf "$dst"
  if [ "$MODE" = "copy" ]; then
    cp -a "$src" "$dst"
  else
    ln -s "$src" "$dst"
  fi
}

mkdir -p "$MU/Platforms/Google" "$MU/Silicon/Google" "$MU/Resources/Configs"

install_path "$HERE/Platforms/Google/bluejayPkg"    "$MU/Platforms/Google/bluejayPkg"
install_path "$HERE/Silicon/Google/GS101Pkg"        "$MU/Silicon/Google/GS101Pkg"
install_path "$HERE/Resources/Configs/bluejay.toml" "$MU/Resources/Configs/bluejay.toml"

# Upstream BootShim writes its arm64 Image header with text_offset = FD_BASE,
# i.e. it declares a fixed load address. Tensor ABL does not accept that
# (the payload never runs and the device drops back to fastboot). BootShim is
# position independent and copies the FD to FD_BASE itself, so build it as a
# PIE image: text_offset 0, no fixed placement. Measured on bluejay, see
# docs/EVIDENCE.md.
#
# The header is also emitted when REQUIRES_KERNEL_HEADER=0. UniXNU compiles the
# shim that way because its own check expects text_offset to equal FD_BASE,
# which bluejay cannot satisfy; keeping the header gives ABL the layout it
# already accepted on this phone.
SHIM="$MU/BootShim/BootShim.S"
if [ -f "$SHIM" ]; then
  # GNU-only sed syntax would fail here: the CI runner's sed is BSD sed.
  python3 - "$SHIM" <<'PY_SHIM'
import sys
from pathlib import Path

PAYLOAD = """
#if REQUIRES_KERNEL_HEADER == 0
  //
  // The Kernel field of a UniXNU image continues with the payload volume:
  // one 4 KiB FDT envelope (the pack stage writes it as [bootimg] dtb) and
  // then the FAT volume. The shim copies the volume to its reserved window so
  // the firmware can publish it as a block device. The size comes from the
  // BPB: bytes/sector at 0x0B and total sectors at 0x20 (FAT32), no new
  // defines, so the firmware-only build (REQUIRES_KERNEL_HEADER == 1) is
  // byte-identical to before.
  //
  add   x4, x4, #0x1000
  ldr   x6, [x4, #8]
  lsr   x6, x6, #24
  and   x6, x6, #0xFFFF
  cmp   x6, #512
  b.ne  _PayloadDone
  ldr   w7, [x4, #0x20]
  cbz   w7, _PayloadDone
  mov   w8, #0x20000
  cmp   w7, w8
  b.hi  _PayloadDone
  mov   w9, #512
  mul   w6, w9, w7
  ldr   x5, =0x880000000
_PayloadCopy:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  w6, w6, #16
  b.ne  _PayloadCopy
_PayloadDone:
#endif
  ldr   x5, =FD_BASE
"""

path = Path(sys.argv[1])
text = path.read_text()
original = text
text = text.replace('#if REQUIRES_KERNEL_HEADER == 1', '#if 1', 1)
text = text.replace('.quad FD_BASE', '.quad 0', 1)
anchor = '  b.ne  _CopyLoop\n  ldr   x5, =FD_BASE\n'
if anchor not in text:
    sys.exit('apply.sh: BootShim.S copy loop changed; payload copy not installed')
text = text.replace(anchor, '  b.ne  _CopyLoop\n' + PAYLOAD, 1)
if text != original:
    path.write_text(text)
    print('patched BootShim.S: arm64 header always present, text_offset = 0, payload copy installed')
PY_SHIM
fi

echo "installed bluejayPkg and GS101Pkg into $MU ($MODE)"
echo
echo "Build (from $MU):"
echo "  python3 build_uefi.py -d bluejay -r RELEASE"
echo
echo "Output:"
echo "  Build/bluejayPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd"
echo "  Mu-bluejay.img"
