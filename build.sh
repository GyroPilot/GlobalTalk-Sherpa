#!/bin/bash
# build.sh: build GlobalTalk Sherpa with Retro68 on moof.
#   bash build.sh        -> build/GlobalTalk Sherpa.bin (MacBinary), .dsk (800K disk), .APPL
set -eu
cd "$(dirname "$0")"
T=/Volumes/RetroDev/src/Retro68-build/toolchain
export PATH="$T/bin:$PATH"
python3 tools/mkicons.py src/icons.r
mkdir -p build
cd build
[ -f CMakeCache.txt ] || cmake .. -DCMAKE_TOOLCHAIN_FILE="$T/m68k-apple-macos/cmake/retro68.toolchain.cmake" >/dev/null
make 2>&1
ls -l Sherpa.bin Sherpa.dsk
