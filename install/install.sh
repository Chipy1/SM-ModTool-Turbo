#!/bin/sh
# Install SM ModTool Turbo into a Mod Tool Release folder.
# Usage: ./install.sh "/path/to/Scrap Mechanic Mod Tool/Release"
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
target=${1:-}
if [ -z "$target" ]; then
    echo "usage: $0 <folder containing ModTool.exe>" >&2
    exit 2
fi
if [ ! -f "$target/ModTool.exe" ]; then
    echo "error: $target/ModTool.exe not found" >&2
    exit 1
fi
if [ ! -f "$here/dist/d3d11.dll" ]; then
    echo "error: run ./build.sh first (dist/d3d11.dll missing)" >&2
    exit 1
fi
stamp=$(date +%Y%m%d-%H%M%S)
if [ -f "$target/d3d11.dll" ]; then
    cp "$target/d3d11.dll" "$target/d3d11.dll.bak.$stamp"
    echo "backed up existing d3d11.dll -> d3d11.dll.bak.$stamp"
fi
if [ -f "$target/version.dll" ]; then
    echo "note: an old version.dll is present; v1.0.0 of this mod used that name and it is bypassed by Proton. Remove it if it is the old ModTool Turbo proxy."
fi
if [ -f "$target/ModToolTurbo.ini" ]; then
    cp "$target/ModToolTurbo.ini" "$target/ModToolTurbo.ini.bak.$stamp"
    echo "backed up existing ModToolTurbo.ini -> ModToolTurbo.ini.bak.$stamp"
fi
cp "$here/dist/d3d11.dll" "$target/d3d11.dll"
cp "$here/dist/ModToolTurbo.ini" "$target/ModToolTurbo.ini"
echo "installed into $target"
