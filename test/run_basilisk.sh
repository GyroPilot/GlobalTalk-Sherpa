#!/bin/bash
# run_basilisk.sh: test Sherpa unattended on System 7.5.5 in Basilisk II.
#   bash test/run_basilisk.sh [autotest-file] [seconds]
# Clones the Basilisk boot disk (APFS clone, instant), puts Sherpa, the test scan and the
# autotest script in its Startup Items, boots, screenshots the window every 10 s, then
# copies the saved pictures and list back out to test/out/basilisk/.
set -u
cd "$(dirname "$0")/.."
T=/Volumes/RetroDev/src/Retro68-build/toolchain/bin
EMU=/Volumes/RetroDev/emulators
AUTO=${1:-test/autotest_map.txt}; SECS=${2:-90}
W=test/work; OUT=test/out/basilisk
mkdir -p $W $OUT; rm -f $OUT/*
export HOME_SAVE=$HOME
cp -c $EMU/basilisk2/disks/system.dsk $W/b2.dsk
SI=":System Folder:Startup Items"
( export HOME=$PWD/$W
  $T/hmount $W/b2.dsk >/dev/null
  $T/hcopy -m build/Sherpa.bin "$SI:GlobalTalk Sherpa"
  tr '\n' '\r' < "$AUTO" > $W/auto.txt
  $T/hcopy -r $W/auto.txt "$SI:Sherpa Autotest"
  $T/hcopy -r "test/GlobalTalk Scan" "$SI:GlobalTalk Scan"
  for f in "Sherpa Autotest" "GlobalTalk Scan"; do $T/hattrib -t TEXT -c ttxt +i "$SI:$f"; done
  $T/humount )
cat > $W/b2.prefs <<P
rom $EMU/basilisk2/roms/Quadra800.ROM
disk $PWD/$W/b2.dsk
screen win/640/480
ramsize 33554432
modelid 14
cpu 4
fpu true
nocdrom true
nosound true
nogui true
P
# the emulator's own window is the one with a title; its menu-bar strips have none
winid() { $EMU/bin/winlist basilisk | awk '/^BasiliskII [0-9]+ \{/ {id=$2} /^\} Basilisk II/ {print id; exit}'; }
/opt/local/bin/BasiliskII --config $W/b2.prefs >$W/b2.log 2>&1 &
pid=$!
for ((t=10; t<=SECS; t+=10)); do
  sleep 10
  id=$(winid)
  [ -n "$id" ] && screencapture -x -o -l "$id" $OUT/screen_${t}s.png
done
kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
( export HOME=$PWD/$W
  $T/hmount $W/b2.dsk >/dev/null
  $T/hls -l "$SI"
  $T/hls -1 "$SI" | grep -E "^(Shot|Sherpa List)" | while IFS= read -r f; do $T/hcopy -r "$SI:$f" "$OUT/$f.raw"; done
  $T/humount )
ls -l $OUT
