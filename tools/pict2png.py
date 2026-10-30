"""pict2png.py IN.pict OUT.png [scale] -- render a QuickDraw PICT version 1 (what Sherpa's Save Map as Picture
and other System 6/7 programs write) to PNG: lines, rects, round rects, ovals, text, 1-bit bitmaps (BitsRect,
PackBitsRect). macOS 15's sips no longer renders these opcodes. Pillow draws; text uses Pillow's bitmap font."""
import struct, sys
from PIL import Image, ImageDraw, ImageFont

def unpack(src, n):
    out = bytearray(); i = 0
    while i < len(src) and len(out) < n:
        c = src[i]; i += 1
        if c < 128: out += src[i:i + c + 1]; i += c + 1
        elif c > 128: out += bytes([src[i]]) * (257 - c); i += 1
    return bytes(out[:n])

def main():
    src, dst = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    b = open(src, 'rb').read()
    if len(b) > 512 and b[:512] == b'\0' * 512: b = b[512:]
    size, top, left, bot, right = struct.unpack('>HhhhhH'[:6], b[:10])
    w, h = right - left, bot - top
    img = Image.new('1', (w, h), 1); d = ImageDraw.Draw(img)
    font = ImageFont.load_default()
    p = 10; pen = (0, 0); loc = (0, 0); penw = 1; ops = {}
    fill_black = False
    def rect(off): return struct.unpack('>hhhh', b[off:off + 8])
    def R(r): return [r[1] - left, r[0] - top, r[3] - left - 1, r[2] - top - 1]
    def draw_rect(kind, r, shape):
        x0, y0, x1, y1 = R(r)
        if x1 < x0 or y1 < y0: return
        # verbs: 0 frame, 1 paint, 2 erase, 3 invert, 4 fill (the pen/fill pattern is taken as black)
        fill = {1: 0, 2: 1, 3: 0, 4: 0}.get(kind)
        outline = 0 if kind == 0 else None
        if shape == 'rect': d.rectangle([x0, y0, x1, y1], fill=fill, outline=outline, width=penw)
        elif shape == 'rrect': d.rounded_rectangle([x0, y0, x1, y1], radius=4, fill=fill, outline=outline, width=penw)
        elif shape == 'oval': d.ellipse([x0, y0, x1, y1], fill=fill, outline=outline, width=penw)
    lastrect = (0, 0, 0, 0)
    while p < len(b):
        op = b[p]; p += 1; ops[op] = ops.get(op, 0) + 1
        if op == 0xFF: break
        if op == 0x00 or op == 0x11 and False: continue
        if op == 0x11: p += 1; continue
        if op == 0x01: n = struct.unpack('>H', b[p:p + 2])[0]; p += n; continue
        if op in (0x02, 0x09, 0x0A, 0x10): p += 8; continue
        if op in (0x03, 0x05, 0x08, 0x0D, 0xA0): p += 2; continue
        if op == 0x04: p += 1; continue
        if op in (0x06, 0x0B, 0x0C, 0x0E, 0x0F): p += 4; continue
        if op == 0x07: penw = max(1, struct.unpack('>hh', b[p:p + 4])[1]); p += 4; continue
        if op == 0x20:
            y0, x0, y1, x1 = struct.unpack('>hhhh', b[p:p + 8]); p += 8
            d.line([x0 - left, y0 - top, x1 - left, y1 - top], fill=0, width=penw); pen = (x1, y1); continue
        if op == 0x21:
            y1, x1 = struct.unpack('>hh', b[p:p + 4]); p += 4
            d.line([pen[0] - left, pen[1] - top, x1 - left, y1 - top], fill=0, width=penw); pen = (x1, y1); continue
        if op == 0x22:
            y0, x0 = struct.unpack('>hh', b[p:p + 4]); dx, dy = struct.unpack('>bb', b[p + 4:p + 6]); p += 6
            d.line([x0 - left, y0 - top, x0 + dx - left, y0 + dy - top], fill=0, width=penw); pen = (x0 + dx, y0 + dy); continue
        if op == 0x23:
            dx, dy = struct.unpack('>bb', b[p:p + 2]); p += 2
            d.line([pen[0] - left, pen[1] - top, pen[0] + dx - left, pen[1] + dy - top], fill=0, width=penw); pen = (pen[0] + dx, pen[1] + dy); continue
        if op in (0x28, 0x29, 0x2A, 0x2B):
            if op == 0x28: y, x = struct.unpack('>hh', b[p:p + 4]); p += 4; loc = (x, y)
            elif op == 0x29: loc = (loc[0] + b[p], loc[1]); p += 1
            elif op == 0x2A: loc = (loc[0], loc[1] + b[p]); p += 1
            else: loc = (loc[0] + b[p], loc[1] + b[p + 1]); p += 2
            n = b[p]; s = b[p + 1:p + 1 + n].decode('mac_roman', 'replace'); p += 1 + n
            d.text((loc[0] - left, loc[1] - top - 9), s, fill=0, font=font); continue
        if 0x30 <= op <= 0x5F:
            kind = op & 7; shape = ('rect', 'rrect', 'rrect', 'oval')[(op - 0x30) >> 4 & 3] if op < 0x50 else 'oval'
            if op < 0x40: shape = 'rect'
            elif op < 0x50: shape = 'rrect'
            if not (op & 8): lastrect = rect(p); p += 8
            draw_rect(kind, lastrect, shape); continue
        if 0x60 <= op <= 0x67: p += 12; continue
        if 0x68 <= op <= 0x6F: p += 4; continue
        if 0x70 <= op <= 0x77 or 0x80 <= op <= 0x87:
            n = struct.unpack('>H', b[p:p + 2])[0]
            if 0x70 <= op <= 0x77 and op & 7 == 0:
                pts = [struct.unpack('>hh', b[p + 10 + i * 4:p + 14 + i * 4]) for i in range((n - 10) // 4)]
                d.line([(x - left, y - top) for y, x in pts], fill=0, width=penw)
            p += n; continue
        if op in (0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F): continue
        if op in (0x90, 0x91, 0x98, 0x99):
            rb = struct.unpack('>H', b[p:p + 2])[0]; bounds = rect(p + 2); srect = rect(p + 10); drect = rect(p + 18); p += 28
            if op in (0x91, 0x99): n = struct.unpack('>H', b[p:p + 2])[0]; p += n
            bh = bounds[2] - bounds[0]; rows = []
            for _ in range(bh):
                if op in (0x90, 0x91): rows.append(b[p:p + rb]); p += rb
                else:
                    if rb > 250: cnt = struct.unpack('>H', b[p:p + 2])[0]; p += 2
                    else: cnt = b[p]; p += 1
                    rows.append(unpack(b[p:p + cnt], rb)); p += cnt
            bw = rb * 8
            bm = Image.frombytes('1', (bw, bh), b''.join(rows))
            bm = bm.point(lambda v: 255 - v) if False else Image.eval(bm, lambda v: 255 - v)  # QuickDraw 1 = black
            # copy srcRect of the bitmap (in bounds coordinates) to dstRect
            sx0, sy0 = srect[1] - bounds[1], srect[0] - bounds[0]
            sx1, sy1 = srect[3] - bounds[1], srect[2] - bounds[0]
            part = bm.crop((sx0, sy0, sx1, sy1))
            img.paste(part, (drect[1] - left, drect[0] - top))
            continue
        if op == 0xA1: n = struct.unpack('>H', b[p + 2:p + 4])[0]; p += 4 + n; continue
        if op in (0x2C, 0x2D, 0x2E): n = struct.unpack('>H', b[p:p + 2])[0]; p += 2 + n; continue   # fontName, lineJustify, glyphState (System 7 writes 2C into v1 pictures)
        print('unknown opcode 0x%02X at %d; stopping' % (op, p - 1)); break
    if scale > 1: img = img.resize((w * scale, h * scale), Image.NEAREST)
    img.save(dst)
    print('%s: %dx%d, opcodes:' % (dst, w, h), ' '.join('%02X:%d' % kv for kv in sorted(ops.items())))

main()
