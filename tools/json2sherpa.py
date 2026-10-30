#!/usr/bin/env python3
"""json2sherpa.py: turn a globaltalk-scraper JSON snapshot into a Sherpa scan list.

    python3 json2sherpa.py globaltalk_latest.json "GlobalTalk Scan"

The output is Mac Roman text with CR line ends, the same format as Sherpa's own
File > Save List as Text, so File > Open Scan reads it: a way to map GlobalTalk from the
netatalk Pi's daily scrape, and test data for the emulators.
"""
import json, sys, time
from datetime import datetime, timezone

d = json.load(open(sys.argv[1]))
when = d.get('generated_at', '')
try:
    t = datetime.fromisoformat(when.replace('Z', '+00:00')).timestamp()
except ValueError:
    t = time.time()
macsecs = int(t) + 2082844800          # Mac epoch: 1 Jan 1904
out = ['GlobalTalk Sherpa scan', 'Scanned:\t%d\t%s' % (macsecs, when[:10]),
       '# Made by json2sherpa.py from the globaltalk-scraper snapshot %s' % when]
by = {}
for n in d['nodes']:
    by.setdefault(n['zone'], []).append(n)
for z in d['zones']:
    out.append('')
    out.append('Zone:\t' + z)
    for n in sorted(by.get(z, []), key=lambda n: (tuple(int(p) for p in n['address'].split('.')), int(n['socket']))):
        out.append('Name:\t%s:%s\t%s\t%s' % (n['address'], n['socket'], n['type'][:32], n['object'][:32]))
open(sys.argv[2], 'wb').write(('\r'.join(out) + '\r').encode('mac_roman', 'replace'))
print('%d zones, %d names -> %s' % (len(d['zones']), len(d['nodes']), sys.argv[2]))
