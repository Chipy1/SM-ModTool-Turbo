#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p dist
x86_64-w64-mingw32-g++ \
  -std=c++17 -O2 -Wall -Wextra \
  -shared -static -static-libgcc -static-libstdc++ \
  -o dist/d3d11.dll \
  src/util.cpp src/config.cpp src/log.cpp src/compile_cache.cpp \
  src/hooks.cpp src/d3d11_exports.cpp src/dllmain.cpp \
  -lkernel32 -luser32
cp ModToolTurbo.ini dist/ModToolTurbo.ini
echo "built dist/d3d11.dll"
