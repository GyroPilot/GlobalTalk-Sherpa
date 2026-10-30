# GlobalTalk Sherpa

An AppleTalk scanner and mapper for 68k Macs, for Marchintosh 2027 (the Trawl replacement). The spec is `../SCANNER_SPEC_DRAFT.md`. Tim said "build" on 26 Oct 2026; version 0.1d1 was built the same day.

## Build

```
bash build.sh                      # build/Sherpa.bin (MacBinary), .dsk, .APPL
python3 tools/package.py 0.1d1     # dist/: .dc42 (Disk Copy 4.2, 800K) and .hqx (BinHex)
```

Retro68 on moof (`/Volumes/RetroDev/src/Retro68-build`), 68000 code, about 45 KB. Retro68 has no AppleTalk headers: `src/appletalk.h` is written from the Inside Macintosh: Networking record layouts, with each offset noted.

| File | What it does |
|---|---|
| `src/scan.c` | ZIP zone list and my zone (.XPP xCall), NBP lookups one zone at a time (async), grouping names into devices, model and kind detection |
| `src/afp.c` | FPGetSrvrInfo (ASPGetStatus): server name, machine, AFP versions, icon, and whether it takes guests. It never logs in. |
| `src/map.c` | The three levels (all zones, one zone, one device) and the PICT export |
| `src/store.c` | Prefs file (resource fork: settings, lab zones, custom ICN#; data fork: reliability history), saving and opening scans, icon packs, paste-an-icon |
| `src/main.c` | Menus, window, scrolling, the Lab Zones dialog, automatic rescan |
| `src/autotest.c` | Unattended test runs (see below) |
| `tools/mkicons.py` | Draws the built-in black-and-white icons into `src/icons.r` |
| `tools/json2sherpa.py` | Converts a globaltalk-scraper JSON snapshot into a Sherpa scan list |
| `tools/package.py` | Makes the .dc42 and .hqx (both checked: the DC42 checksum method matches Tim's own images, and the BinHex decodes with Python's stock `binhex`) |

## Unattended tests

If a file named `Sherpa Autotest` sits next to the app, Sherpa runs its commands at startup (`open`, `zone`, `node`, `world`, `scan site|all`, `waitscan`, `pict`, `text`, `wait`, `quit`).

- `test/run_qemu.sh test/autotest_views.txt 80`: System 7.6.1, Quadra 800. Sherpa goes in Startup Items; the test files are marked invisible (`hattrib +i`) so the Finder doesn't open them. The QEMU monitor presses Return for the "not shut down properly" warning and takes screenshots.
- `test/run_snow.sh test/autotest_views.txt 90`: System 6.0.8, Mac Plus, in Snow. Patches the floppy's boot blocks (0x1A shell and 0x5A startup program) so the System launches Sherpa. Snow's `settings.json` has `"convert_to_moof_mode": "Never"` so it doesn't stop to ask.
- `test/run_basilisk.sh`: System 7.5.5. Stops at the shutdown warning, because Basilisk can't be sent a key, so use QEMU instead.

Screenshots go to `test/out/<emulator>/`.

## 0.1d6 (27 Oct 2026, built on Tim's "build")

- **Scan Local Network fixed (probably).** Tim's Performa test of 0.1d5 still showed only LisaGaming. `ask_router.py` on the Pi 3 (moof-setup) sent the same RTMP Route Data Request and ZIP Query to the IIsi and got nets 38, 37, 35 = LisaGaming, Lisa_Net_E, Lisa_Net_LT, so the router was fine. The bug: the socket listener called ReadRest with a 590-byte buffer and dropped the packet unless D3 came back 0, but D3 > 0 only means the buffer was bigger than the packet (Inside Macintosh: Networking, "DDP socket listener"). Now it reads exactly D1 bytes.
- **Router report.** Local Network Zones shows what the router said after Ask the Router ("Router 38.1: 3 networks, 3 zones." or where it stopped: no router heard, socket/send error, no answer), and the Dashboard shows it after a Scan Local Network. Typed zones are named as the source when set.
- **Icons.** Apple's icons are Apple's (like the ROMs), so none ship in Sherpa. What it does instead: on System 7, this Mac is drawn with its own Apple icon from Gestalt 'micn' (System file or ROM), except when it is a Lisa/XL (the XL Name extension). Icon packs can now carry kind icons named "K:<kind number>", which replace Sherpa's drawing for every device of that kind (dashboard too), so a lab can use Trawl's or Apple's icons locally. Looked into: System 7.6.1's System file and the IIci, IIsi, Quadra 800 and LC 475 ROMs carry no per-model Mac icons, so there is nothing to borrow for other machines.
- Tested: QEMU 7.6.1 (no router there: the report reads "No router answer: no router heard on this network. Using this cable's zones."), Snow Plus 6.0.8 (saved scan, dashboard). Not yet on real hardware: router detection and this Mac's icon.

## 0.1d5 (26 Oct 2026, built on Tim's "build")

- **Icons follow the model.** New black-and-white families: Macintosh II (II, IIx, IIcx, IIci, IIsi, Quadra 700/650, Centris 650), LC "pizza box" (LC, Performa 4xx, Quadra 605/610/630/660), tower (Quadra 800/840/900/950, Workgroup Server), all-in-one (Color Classic, LC/Performa 5xx, Macintosh TV) and Power Macintosh, matched by model name (`ModelKind` in scan.c). A Mac that also routes keeps its model's icon with a small router badge, and its card says so; only non-Macs get the plain router icon (Tim: the icons didn't match the Macs).
- **Scan Local Network** (Cmd-L) replaces Scan This Site, and **Local Network Zones…** replaces Lab Zones. With no zones typed in, Sherpa asks the router (local.c): an RTMP Route Data Request to its socket 1 gives its routing table, the networks at distance 0 are the ones plugged into it, and a ZIP Query to its socket 6 names their zones. For Tim that should be LisaGaming, Lisa_Net_LT and Lisa_Net_E. If the router doesn't answer, it falls back to this cable's zones. The replies come to a DDP socket of Sherpa's own, read by an assembly socket listener (registers per Inside Macintosh: Networking). **Not yet tested on real hardware**: no emulator here has a router.
- **Map > Dashboard** (Cmd-3): the latest scan, devices on the map by kind (bar chart), the last 12 scans (when, local or all, zones, devices, new, gone; kept in the prefs as 'SScn' 128), and which devices were new or gone in the latest scan (compared with the last time each zone was scanned).
- Tested in QEMU 7.6.1 and Snow Plus 6.0.8 (`test/autotest_d5.txt`, `test/autotest_d5plus.txt`).

## 0.1d4 (26 Oct 2026, built on Tim's "build")

- About box: real line breaks (Rez "\n" is a linefeed, drawn as a box on the Mac; now \0x0D) and the full version, "GlobalTalk Sherpa 0.1d4" (Tim asked for the sub-version there).
- Each disk image is named by version ("Sherpa 0.1d4"), so old and new disks can't be mixed up in the Finder.
- The About box is taller (its last line was cut off) and shows the app icon (a dialog Icon item needs a plain ICON, so mkicons now writes ICON 128 too). Checked in QEMU with the new `about` autotest command.

## 0.1d3 (26 Oct 2026, evening)

- **Confirmed on the Performa (Tim's photos, 06:27):** Scan This Site gives 1 zone (LisaGaming, 5 devices, 22 names) and the Performa shows itself, as a Lisa / Macintosh XL from its XL Name. Scan All: 31 zones, 61 devices, 227 names; the Network Diagram looks right.

- **Scan This Site shows only the site's zones** (the lab zones); Scan All of GlobalTalk shows them all. Before, a site scan listed every GlobalTalk zone as "not scanned" (Tim: not intuitive).
- **With no lab zones set, the site is the zones on this Mac's own cable** (ZIP GetLocalZones), so it stops at the router: for Tim, LisaGaming. Zones behind the IIsi (Lisa_Net_LT, Lisa_Net_E) are added in Scan > Lab Zones. Automatic one-hop detection from the IIsi's RTMP broadcasts was offered and not built.
- **This Mac's model:** 0.1d2 showed the Performa as "Device" (its System has no Gestalt 'mnam'). Now it falls back to Gestalt 'mach' and the System's model names (STR# -16395).
- Tested: builds clean; QEMU's live site scan shows only the one zone. QEMU's AppleTalk has no address, so the this-Mac path can only be checked on the Performa.

## 0.1d2 (26 Oct 2026, evening)

- **Map > Network Diagram** (Cmd-2): a backbone across each row, a drop line per zone, and the zone's devices tapped off it with icon, name and model, like the old AppleTalk mappers (Tim's photo). Rows wrap to the window: two zones across on a Plus. Click a device for its card, a zone name for the zone. The choice is remembered in the prefs (the old unused volume flag).
- **The scanning Mac now appears in its own zone.** Its zone is looked up as "*", and if NBP still doesn't return its names, Sherpa adds it from its Sharing Setup name and (System 7) its Gestalt model. Not yet confirmed on the Performa.
- **Models with leading spaces** ("  Macintosh", Emulated Mac at 38.207) are recognised and drawn without the spaces.
- Tested: QEMU 7.6.1 and Snow Plus 6.0.8 with the saved scan (`test/autotest_diagram.txt`; the sample scan now has lisa-mon1 and the leading-space model).

## Status, 0.1d1 (26 Oct 2026)

**Verified in emulators (with the Pi's GlobalTalk scrape loaded as a saved scan):**
- System 7.6.1 (QEMU Quadra 800) and System 6.0.8 (Snow, Mac Plus, 512x342) both run.
- The all-zones map (39 zones, 57 devices) works.
- The zone view (network strips, icons, models, reliability) works.
- The device card works. The Performa 475 shows as a Lisa / Macintosh XL from its socket-253 name.
- "AppleTalk is off" is handled.
- Save Picture and Save List write files.

**Verified on real hardware (26 Oct, Tim's photos):**
- A live Scan All of GlobalTalk: "60 devices, 225 names in 36 zones". The all-zones map and the LisaGaming zone view drew correctly, with every device marked "new" (first scan).

**Found in that test (not fixed yet):**
- The Mac running Sherpa doesn't appear in its own zone (the Performa 475 was missing, though lisa-mon1 sees it). Probably NBP lookups don't return the asking Mac's own names. Fix: add "this Mac" from local info (Sharing Setup name, Gestalt model).
- "Emulated Mac" shows as an unknown Device: its model type is "  Macintosh" with leading spaces, which IsModelType doesn't skip.

**Not verified yet (needs the real network or a person):**
- The guest flag (FPGetSrvrInfo) and the server icon.
- Paste Icon, Copy Icon, and icon pack export and import.
- Reliability over repeated live scans, and automatic rescan.
- The 512Ke, and MacWorks on the Mac XL.
- Opening the saved PICT in TeachText and SimpleText.

**Not built yet:**
- Printing (planned for later; saved pictures print from SimpleText today).

## 0.1d7 (built 28 Oct 2026, 19:56, on Tim's "build")
- Approved model icons from tools/models.py (0, 1, 3, 6, 9, 10, 13, 14) replace mkicons.py's stand-ins in icons.r; the rest keep their stand-ins until approved.
- Socket listener: branches on ReadRest's condition codes, as Apple's sample listener does, instead of testing D3.
- Printers are two classes (Tim, 27 Oct: icons are class-centric, the name underneath does the rest; icon packs cover the rest): kPrinter (box printers: LaserWriter, StyleWriter, DeskWriter, …) and kDotMatrix (ImageWriter), ICN# 216 = the approved ImageWriter icon.

- Colour icons.
- The community icon drop folder on the netatalk Pi.

## Design choices

- **One zone at a time:** parallel NBP lookups dropped LocalTalk replies on 25 Oct.
- **Model detection by name, not socket:** GlobalTalk Macs announce their model on sockets 251, 252 and 253.
- **Guest access is reported from the server's own info:** Sherpa never logs in, and lists no volumes (Tim, 26 Oct).
- **Lab zones:** set in Scan > Lab Zones. If none are set, the lab is your own zone.

**Hardware, 28 Oct 2026 (Tim's photo, test/Performa 28 Oct 2026 - 0.1d6 bomb after Scan Local Network.jpg):**
0.1d6 over MiniVNC on the Performa 475. Scan Local Network reported "1 zones, 5 devices": LisaGaming with the
Performa (shown with the Lisa/XL icon, so the this-Mac fix works), IIsiRouterMarkII, netatalk, lisa-mon1 and
Emulated Mac (as "Macintosh": the leading-space fix works). So the router query still produced only this cable's
zone, which 0.1d7's listener change is meant to fix. Some time later (Tim was away) a system bomb: "Finder" stack
collision with heap, which took MiniVNC down. Not yet known: how long after the scan, whether automatic rescan
was on, Sherpa's partition (SIZE: 448 KB preferred, 320 KB minimum), and whether the Performa bombs the same way
without Sherpa. The listener itself uses no stack and the socket is closed at the end of AskRouter; the first
suspects are memory (the Finder's heap next to Sherpa's partition) and the rescan path.
Tim, later: automatic rescan was OFF, so Sherpa was idle in WaitNextEvent when the bomb came. With its socket
closed and no timer, Sherpa has nothing running at that point; the Finder's stack was overrun by something at
interrupt or completion time, and MiniVNC (whose TCP completion routines run on whatever stack is current, the
Finder's when it is frontmost) is the likelier cause, especially after Sherpa's map gave it a lot of screen to
encode. To separate them: an hour of MiniVNC with Sherpa never launched, and an hour of Sherpa with no VNC.

**Hardware, 29 Oct 2026, 7:13 AM (Tim's photo, test/Performa 29 Oct 2026 - 0.1d6 dashboard after a night of
15-min rescans.jpg):** 0.1d6 ran all night on the Performa with automatic rescan every 15 minutes, no VNC, no bomb:
Local scans at 9:24 PM and 5:35 AM, Scan All every 15 minutes from 5:24 AM to 7:07 AM, 83 devices, 290 names,
36 routers (17 Macs). So the 28 Oct "Finder: stack collision with heap" was not Sherpa; MiniVNC is the suspect.
Seen in the history (to fix): the zone count wanders between 43 and 49 from one scan to the next (GetZoneList
comes back short sometimes: keep the union of the retries, or of the last scans, and say how many answered);
two names flap "gone" by one each scan (LC III+ BluSCSI, LW 16/600: NBP misses; report gone only after two
consecutive misses). Local scans still show 1 zone, which 0.1d7's listener change targets. The Dashboard's
history table, the by-kind chart with the Lisa/XL icon, and the new/gone lists all drew correctly.
- Built and packaged (dist/GlobalTalk Sherpa 0.1d7.dc42 and .hqx); on the Pi 3 share "GlobalTalk Sherpa" (0.1d6
  removed there) and in Dropbox From moof/GlobalTalk Sherpa. The test that matters: Scan Local Network on the
  Performa should say "Router 38.1: 3 networks, 3 zones" and list LisaGaming, Lisa_Net_LT and Lisa_Net_E.
  Not in this build: the wandering zone count and the flapping "gone" names seen on 29 Oct (next).

## 0.1d8 (source only)

- **Scans starve other programs.** scan.c waited for each NBP lookup with `while (ioResult > 0) SystemTask();`
  and local.c's Collect did the same. SystemTask does not give time to other applications under System 7, so
  for the seconds of every lookup, times 49 zones, MiniVNC on the Performa could not answer and RealVNC from
  tmobile timed out ("Timed out waiting for a response from the computer", 29 Oct, with Sherpa rescanning
  every 15 minutes). Both waits now call `Yield()` (main.c): WaitNextEvent with mask 0 and a 5-tick sleep,
  which lets other programs run and takes no event off the queue; SystemTask on a System 6 Mac without WNE.

## 1.0 (built 29 Oct 2026, 10:57, on Tim's "Let's build a 1.0")

Tim, 29 Oct: "Sherpa has been running for a long time now and performing well... I'm good with the icons as they are
for now. Let's build a 1.0 please." So 1.0 is 0.1d8's source as it stood, with the version strings changed:
- About box says "GlobalTalk Sherpa 1.0"; 'vers' (1) is 1.0 final (`0x01, 0x00, 0x80, 0x00`: Retro68's Rez does not
  define the word `final`, so the stage is the number; `development` worked before because Types.r defines it).
- The disk's Read Me says "Version 1.0, 29 October 2026, from Tim O'Connor's lab" instead of "This is a test
  version" (README_MAC.txt, VERSION substituted by package.py). Backups: src/sherpa.r.bak_0.1d7, README_MAC.txt.bak_0.1d7.
- In it from 0.1d8: the Yield() waits in scan.c and local.c (other programs, MiniVNC included, get time during scans).
- Icons: the approved model icons plus the stand-ins, as Tim accepted them on 29 Oct ("as they are for now").
- Not in 1.0 (noted for 1.0.1): the zone count wandering 43-49 between scans, names flapping "gone" after one NBP
  miss, and the empty Lisa_Net_LT / Lisa_Net_E zones in Scan All (see Tim's 29 Oct scan on the Pi 3 share).
- Built and packaged by the banana over ssh: dist/GlobalTalk Sherpa 1.0.dc42 (sha256 148fdba4...) and .hqx
  (ded7d94d...); dc42_check OK; on both Pi shares ("GlobalTalk Sherpa") and in Dropbox From moof/GlobalTalk Sherpa.
- QEMU 7.6.1 autotest run on the 1.0 binary the same morning (test/out/qemu); result in MOOF_NOTES / BANANA_TO_MOOF.

## 1.0.1 (built 29 Oct 2026, 14:41, on Tim's "Go ahead and build it")

- **Years.** A System 7 Mac whose Date & Time panel got "26" sits in 1926, and every date Sherpa printed said
  1926 (the Performa's scan file and Dashboard). util.c `FixYear` adds a century to any year before 1984;
  `NowFixed` is used where scans and history are stamped (scan.c, store.c), and FixYear where dates are drawn or
  written (map.c DateText and the history table, the scan file's "Scanned:" line), so old 1926 stamps display right.
- **Zone list.** GetZoneList came back short at times (43 to 49 zones between scans). ScanStart now asks three
  times and keeps the union, and drops a zone a router lists twice. The map header says "49 zones (46 answered),
  83 devices" after a live scan: answered = zones whose lookup returned at least one name. (The 46-vs-45
  mismatch in the 29 Oct scan: the header counted listed zones, the file only scanned ones; a duplicate name
  from the router is the likely one-off, and is now removed.)
- **Gone.** A device is reported gone at its second consecutive miss, not the first. Hist gains `misses`
  (saved as a seventh field in the history lines; six-field files from 1.0 still load).
- **Empty zones.** A lookup that returns no names is asked once more with double the interval and count before
  the zone counts as empty (Lisa_Net_LT and Lisa_Net_E gave 0 replies in Scan All on 29 Oct; LocalTalk behind the
  IIsi is slower than the first wait). The scan file marks such zones "(asked twice)".
- All six C files compile with Retro68; not built, not run. Test plan on "build": QEMU `test/run_qemu.sh
  test/autotest_map.txt` for the saved-scan paths, then the Performa for the zone list and Lisa_Net_LT.
- **Scan document icon** (Tim, 29 Oct: "a version of the application icon for the document"): ICN# 129 "Sherpa
  scan", a page with the folded corner and the mountain, flag and route on it; the BNDL now maps the 'GTSi' scan
  file to it (it borrowed the file-server icon before). tools/mkicons.py `doc()`. Check: dist/app_icon_check.png.
  The Finder shows new icons only after its desktop file learns the application (rebuild the desktop, or copy the
  application to the hard disk); on the Performa Tim saw the app icon in the window's top corner, not yet on the desktop.
- Built and packaged: dist/GlobalTalk Sherpa 1.0.1.dc42 (sha256 9e2ac307...) and .hqx; on both Pi shares through
  put_share.sh (1.0 removed there; 0.1d6 stays on the Pi 3 until the Performa unmounts it) and in Dropbox
  From moof/GlobalTalk Sherpa. QEMU 7.6.1 autotest_map ran through (open the saved scan, world, zone, node,
  Save Picture, Save List, quit); the scan file's date reads "Mon, Oct 26, 2026". Screenshots of the run and the
  rendered Shot World PICT: dist/Screenshots 1.0.1 (QEMU System 7.6.1)/. Tim's Performa photos, cleaned up with
  src/moof-setup/crt_clean.py (perspective, moire, lighting): dist/Performa photos cleaned/. Hardware test of
  the zone list, the two-miss "gone" and Lisa_Net_LT still to come (Performa).
