#!/bin/bash
# run_qemu.sh: test Sherpa unattended on System 7.6.1 in the QEMU Quadra 800.
#   bash test/run_qemu.sh [autotest-file] [seconds]
# Clones qemu/hd.img (the GlobalTalk 7.6.1 disk), puts Sherpa, the test scan and the autotest
# script in Startup Items, boots headless, presses Return every 15 s for the first minute
# (the "not shut down properly" warning, since every test ends by killing QEMU), takes a
# screenshot every 10 s through the QEMU monitor, then copies Sherpa's saved files out to
# test/out/qemu/. QEMU's NIC is user-mode NAT, so AppleTalk sees no other machine: a live
# scan there finds only this Mac and its Apple Internet Router.
set -u
cd "$(dirname "$0")/.."
T=/Volumes/RetroDev/src/Retro68-build/toolchain/bin
Q=/Volumes/RetroDev/emulators/qemu
AUTO=${1:-test/autotest_map.txt}; SECS=${2:-120}
W=test/work; OUT=test/out/qemu; PORT=45810
mkdir -p $W $OUT; rm -f $OUT/*
cp -c $Q/hd.img $W/q.img
cp $Q/pram.img $W/q_pram.img
SI=":System Folder:Startup Items"
( export HOME=$PWD/$W
  $T/hmount $W/q.img >/dev/null
  $T/hcopy -m build/Sherpa.bin "$SI:GlobalTalk Sherpa"
  tr '\n' '\r' < "$AUTO" > $W/auto.txt
  $T/hcopy -r $W/auto.txt "$SI:Sherpa Autotest"
  $T/hcopy -r "test/GlobalTalk Scan" "$SI:GlobalTalk Scan"
  for f in "Sherpa Autotest" "GlobalTalk Scan"; do $T/hattrib -t TEXT -c ttxt +i "$SI:$f"; done
  $T/humount )
/opt/local/bin/qemu-system-m68k -M q800 -m 128 -bios $Q/Q800.ROM -display none -g 1152x870x8 \
    -drive file=$W/q_pram.img,format=raw,if=mtd \
    -device scsi-hd,scsi-id=0,drive=hd0 \
    -drive file=$W/q.img,format=raw,media=disk,if=none,id=hd0 \
    -nic user,model=dp83932 \
    -monitor tcp:127.0.0.1:$PORT,server,nowait > $W/qemu.log 2>&1 &
pid=$!
mon() { printf '%s\n' "$1" | nc -w 2 127.0.0.1 $PORT >/dev/null 2>&1; }
for ((t=5; t<=SECS; t+=5)); do
  sleep 5
  (( t >= 25 && t <= 70 && t % 15 == 10 )) && mon "sendkey ret"
  if (( t % 10 == 0 )); then
    mon "screendump $PWD/$OUT/screen_${t}s.ppm"
    sleep 1
    sips -s format png $OUT/screen_${t}s.ppm --out $OUT/screen_${t}s.png >/dev/null 2>&1 && rm -f $OUT/screen_${t}s.ppm
  fi
done
mon quit; sleep 2; kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
( export HOME=$PWD/$W
  $T/hmount $W/q.img >/dev/null
  $T/hls -1 "$SI" | while IFS= read -r f; do
    case "$f" in Shot*|*List*|*Prefs*) $T/hcopy -r "$SI:$f" "$OUT/$f.raw";; esac; done
  $T/hcopy -r ":System Folder:Preferences:GlobalTalk Sherpa Prefs" "$OUT/Prefs history.raw" 2>/dev/null
  $T/humount )
ls $OUT
