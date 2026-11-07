GlobalTalk Sherpa VERSION

Finds the AppleTalk zones and devices on your network and on GlobalTalk, and draws them as a map.

How to use it
- Scan > Scan Local Network looks at your side of the router: Sherpa asks your router which networks are plugged straight into it, and scans their zones. To choose the zones yourself, use Scan > Local Network Zones.
- Scan > Scan All of GlobalTalk looks at every zone, one at a time (a few minutes).
- Click a zone to see its devices; click a device to see its card. Click the bar at the top, or press Delete, to go back out.
- Map > All Zones shows each zone as a block. Map > Network Diagram draws the network instead: a backbone with each zone's devices hanging off it. Click a device for its card, or a zone's name for the zone.
- Map > Dashboard shows the numbers: devices by kind, your recent scans, and what's new or gone since the last scan.
- File > Save Map as Picture saves what you're looking at (the whole view, not just the window) as a PICT that TeachText, SimpleText and paint programs open and print.
- File > Save List as Text saves the scan. File > Open Scan reads one back.
- Edit > Paste Icon puts the picture on the clipboard onto a device (or every device of its model). Export Icon Pack shares your icons; Import Icon Pack adds someone else's.
- Scan > Local Network Zones > Ask the Router shows what your router said. After a Scan Local Network, the Dashboard says it too.
- On System 7, your own Mac is drawn with its real Apple icon (the one About This Macintosh shows), taken from your Mac itself.

The bar at the bottom says, on the right, when the scan you are looking at was made ("Last scan" or "Saved scan from"). A zone where nothing replied says "no answer": it is empty, or its devices could not be reached.

Reliability: each device's bar is how often it answered your scans. It says "new" until it has been scanned three times.

File servers: the card shows whether a server takes guests. Sherpa asks the same question the Chooser does; it never logs in.

Runs on System 6.0.8 and System 7 (68000 code: Plus and up). AppleTalk must be on.

Version VERSION, 7 November 2026, from Tim O'Connor's lab (Marchintosh 2027). The sample scan is GlobalTalk as the netatalk Pi saw it on 25 October 2026.
