#!/bin/bash
# run_snow.sh: test Sherpa unattended on System 6.0.8, Mac Plus, in Snow.
#   bash test/run_snow.sh [autotest-file] [seconds] [plus|se]
# Copies the System 6.0.8 floppy, puts Sherpa in its System Folder as "Sherpa", and patches
# the boot blocks so the System launches Sherpa instead of the Finder (the shell name at
# offsets 0x1A and 0x5A, Str15s). Screenshots the Snow window every 10 s into test/out/snow/.
set -u
cd "$(dirname "$0")/.."
T=/Volumes/RetroDev/src/Retro68-build/toolchain/bin
SNOW=/Volumes/RetroDev/emulators/snow
AUTO=${1:-test/autotest_views.txt}; SECS=${2:-90}; MODEL=${3:-plus}
W=test/work; OUT=test/out/snow
mkdir -p $W $OUT; rm -f $OUT/*
cp $SNOW/disks/System608.dsk $W/s6.dsk
SF=":System Folder"
( export HOME=$PWD/$W
  $T/hmount $W/s6.dsk >/dev/null
  $T/hdel ":MacWrite"
  $T/hcopy -m build/Sherpa.bin "$SF:Sherpa"
  tr '\n' '\r' < "$AUTO" > $W/auto.txt
  $T/hcopy -r $W/auto.txt "$SF:Sherpa Autotest"
  $T/hcopy -r "test/GlobalTalk Scan" "$SF:GlobalTalk Scan"
  for f in "Sherpa Autotest" "GlobalTalk Scan"; do $T/hattrib -t TEXT -c ttxt "$SF:$f"; done
  $T/humount )
python3 - "$W/s6.dsk" <<'PY'
import sys
f = open(sys.argv[1], 'r+b')
for off in (0x1A, 0x5A):                      # shell name, and the startup program System 6 launches
    f.seek(off); f.write(bytes([6]) + b'Sherpa' + b' ' * 9)
f.close()
PY
ROM=$SNOW/roms/MacPlus.ROM; [ "$MODEL" = se ] && ROM=$SNOW/roms/MacSE-FDHD.ROM
/Applications/Snow.app/Contents/MacOS/Snow "$ROM" --floppy "$PWD/$W/s6.dsk" >$W/snow.log 2>&1 &
pid=$!
for ((t=10; t<=SECS; t+=10)); do
  sleep 10
  id=$(/Volumes/RetroDev/emulators/bin/winlist snow | awk '/^Snow [0-9]+ \{/ {id=$2} /^\} ./ {print id; exit}')
  [ -n "$id" ] && screencapture -x -o -l "$id" $OUT/screen_${t}s.png
done
kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
ls $OUT
