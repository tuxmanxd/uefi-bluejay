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

echo "installed bluejayPkg and GS101Pkg into $MU ($MODE)"
echo
echo "Build (from $MU):"
echo "  python3 build_uefi.py -d bluejay -r RELEASE"
echo
echo "Output:"
echo "  Build/bluejayPkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd"
echo "  Mu-bluejay.img"
