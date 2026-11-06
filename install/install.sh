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
if [ ! -f "$here/dist/version.dll" ]; then
    echo "error: run ./build.sh first (dist/version.dll missing)" >&2
    exit 1
fi
stamp=$(date +%Y%m%d-%H%M%S)
if [ -f "$target/version.dll" ]; then
    cp "$target/version.dll" "$target/version.dll.bak.$stamp"
    echo "backed up existing version.dll -> version.dll.bak.$stamp"
fi
if [ -f "$target/ModToolTurbo.ini" ]; then
    cp "$target/ModToolTurbo.ini" "$target/ModToolTurbo.ini.bak.$stamp"
    echo "backed up existing ModToolTurbo.ini -> ModToolTurbo.ini.bak.$stamp"
fi
cp "$here/dist/version.dll" "$target/version.dll"
cp "$here/dist/ModToolTurbo.ini" "$target/ModToolTurbo.ini"
echo "installed into $target"
