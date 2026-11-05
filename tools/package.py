#!/usr/bin/env python3
"""package.py: make the files a real Mac can take GlobalTalk Sherpa from.

    python3 tools/package.py VERSION

Writes to dist/:
  GlobalTalk Sherpa VERSION.dc42  Disk Copy 4.2 image of an 800K disk: the application,
                                   a Read Me and a sample GlobalTalk scan. Mount it with
                                   Disk Copy, or write it to a floppy for a Plus or the XL.
  GlobalTalk Sherpa VERSION.hqx   BinHex 4.0 of the application (StuffIt Expander opens it).
Both are plain data files, so they survive netatalk, Dropbox and email without resource forks.
"""
import binascii, os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
T = '/Volumes/RetroDev/src/Retro68-build/toolchain/bin'
VER = sys.argv[1] if len(sys.argv) > 1 else '0.1d1'
DIST = os.path.join(ROOT, 'dist')
WORK = os.path.join(ROOT, 'test', 'work')

def macbinary(path):
    b = open(path, 'rb').read()
    n = b[1]
    name = b[2:2 + n]
    ftype, creator = b[65:69], b[69:73]
    flags = (b[73] << 8) | b[101]
    dlen, rlen = struct.unpack('>II', b[83:91])
    d0 = 128
    r0 = d0 + ((dlen + 127) // 128) * 128
    return name, ftype, creator, flags, b[d0:d0 + dlen], b[r0:r0 + rlen]

# ---- BinHex 4.0 ----
def crc16(data, crc=0):
    """BinHex's CRC: CCITT/XMODEM, the same as binascii.crc_hqx."""
    return binascii.crc_hqx(data, crc)

def with_crc(data):
    return data + struct.pack('>H', crc16(data))

def rle90(data):
    """0x90 n repeats the previous byte to make n in all; 0x90 0x00 is a literal 0x90."""
    out, i = bytearray(), 0
    while i < len(data):
        c, run = data[i], 1
        while i + run < len(data) and data[i + run] == c and run < 255:
            run += 1
        lit = b'\x90\x00' if c == 0x90 else bytes([c])
        if run >= 3:
            out += lit + bytes([0x90, run])
        else:
            out += lit * run
        i += run
    return bytes(out)

ALPHA = b'!"#$%&\'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr'

def binhex(name, ftype, creator, flags, data, rsrc):
    hdr = bytes([len(name)]) + name + b'\0' + ftype + creator + struct.pack('>HII', flags & 0xF800 & ~0x0100, len(data), len(rsrc))
    body = with_crc(hdr) + with_crc(data) + with_crc(rsrc)
    raw = rle90(body)
    bits, nbits, out = 0, 0, bytearray()
    for byte in raw:
        bits = (bits << 8) | byte; nbits += 8
        while nbits >= 6:
            nbits -= 6
            out.append(ALPHA[(bits >> nbits) & 63])
    if nbits:
        out.append(ALPHA[(bits << (6 - nbits)) & 63])
    text = b':' + bytes(out) + b':'
    lines = [text[i:i + 64] for i in range(0, len(text), 64)]
    return b'(This file must be converted with BinHex 4.0)\r\r' + b'\r'.join(lines) + b'\r'

def unbinhex(text):
    """Decoder, used only to check the encoder round-trips."""
    s = text.split(b':', 1)[1].rsplit(b':', 1)[0].replace(b'\r', b'').replace(b'\n', b'')
    bits, nbits, raw = 0, 0, bytearray()
    for ch in s:
        bits = (bits << 6) | ALPHA.index(ch); nbits += 6
        if nbits >= 8:
            nbits -= 8
            raw.append((bits >> nbits) & 255)
    out, i, last = bytearray(), 0, None
    while i < len(raw):
        c = raw[i]
        if c == 0x90:
            n = raw[i + 1]; i += 2
            if n == 0:
                out.append(0x90); last = 0x90
            else:
                out += bytes([last]) * (n - 1)
            continue
        out.append(c); last = c; i += 1
    n = out[0]
    p = 1 + n + 1 + 4 + 4 + 2
    dlen, rlen = struct.unpack('>II', out[p:p + 8]); p += 8
    assert crc16(bytes(out[:p])) == struct.unpack('>H', out[p:p + 2])[0], 'header CRC'
    p += 2
    data = bytes(out[p:p + dlen]); p += dlen
    assert crc16(data) == struct.unpack('>H', out[p:p + 2])[0], 'data CRC'
    p += 2
    rsrc = bytes(out[p:p + rlen]); p += rlen
    assert crc16(rsrc) == struct.unpack('>H', out[p:p + 2])[0], 'rsrc CRC'
    return bytes(out[1:1 + n]), data, rsrc

# ---- Disk Copy 4.2 ----
def dc42(image, name):
    assert len(image) == 819200
    c = 0
    for (w,) in struct.iter_unpack('>H', image):
        c = (c + w) & 0xFFFFFFFF
        c = ((c >> 1) | (c << 31)) & 0xFFFFFFFF
    n = name.encode('mac_roman')[:63]
    hdr = bytes([len(n)]) + n + b'\0' * (63 - len(n))
    hdr += struct.pack('>IIIIBBH', len(image), 0, c, 0, 1, 0x22, 0x0100)
    assert len(hdr) == 84
    return hdr + image

def h(*args):
    subprocess.run([os.path.join(T, args[0])] + list(args[1:]), check=True,
                   env=dict(os.environ, HOME=WORK), stdout=subprocess.DEVNULL)

def main():
    os.makedirs(DIST, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)
    # Retro68's MacBinary leaves the Finder flags at 0. Without hasBundle (0x2000) the Finder never reads Sherpa's
    # BNDL, so it showed the generic application icon (Tim, 2 Nov 2026). Set it, and redo the MacBinary II CRC.
    b = bytearray(open(os.path.join(ROOT, 'build', 'Sherpa.bin'), 'rb').read())
    b[73] |= 0x20
    b[124:126] = struct.pack('>H', binascii.crc_hqx(bytes(b[:124]), 0))
    app = os.path.join(WORK, 'Sherpa_bundle.bin')
    os.makedirs(WORK, exist_ok=True)
    open(app, 'wb').write(bytes(b))
    name, ftype, creator, flags, data, rsrc = macbinary(app)
    assert flags & 0x2000, 'hasBundle not set'
    appname = b'GlobalTalk Sherpa'

    hqx = binhex(appname, ftype, creator, flags, data, rsrc)
    n2, d2, r2 = unbinhex(hqx)
    assert (n2, d2, r2) == (appname, data, rsrc), 'BinHex round trip failed'
    open(os.path.join(DIST, 'GlobalTalk Sherpa %s.hqx' % VER), 'wb').write(hqx)

    img = os.path.join(WORK, 'dist.dsk')
    open(img, 'wb').write(b'\0' * 819200)
    h('hformat', '-l', 'Sherpa ' + VER, img)   # a different disk name per version, so old ones can't be mixed up
    h('hmount', img)
    h('hcopy', '-m', app, ':GlobalTalk Sherpa')
    readme = os.path.join(WORK, 'readme.txt')
    txt = open(os.path.join(ROOT, 'README_MAC.txt'), encoding='utf-8').read().replace('VERSION', VER)
    open(readme, 'wb').write(txt.replace('\n', '\r').encode('mac_roman', 'replace'))
    h('hcopy', '-r', readme, ':Read Me')
    h('hattrib', '-t', 'TEXT', '-c', 'ttxt', ':Read Me')
    h('hcopy', '-r', os.path.join(ROOT, 'test', 'GlobalTalk Scan'), ':GlobalTalk Scan (sample)')
    h('hattrib', '-t', 'TEXT', '-c', 'ttxt', ':GlobalTalk Scan (sample)')
    h('humount')
    out = os.path.join(DIST, 'GlobalTalk Sherpa %s.dc42' % VER)
    open(out, 'wb').write(dc42(open(img, 'rb').read(), 'Sherpa ' + VER))
    for f in sorted(os.listdir(DIST)):
        print('%8d  dist/%s' % (os.path.getsize(os.path.join(DIST, f)), f))

if __name__ == '__main__':
    main()
