#!/usr/bin/env python3
"""mkicons.py: draw Sherpa's built-in 32x32 black-and-white icons and write them as Rez source.

    python3 mkicons.py ../src/icons.r [--show]

ICN# 200 + kind (the kinds in sherpa.h), and ICN# 128 for the application.
Each icon is drawn with a few primitives; the mask is the filled silhouette (everything the
outside can't reach), the same rule Sherpa uses for pasted icons.
These are stand-ins. Kinds listed in models.APPROVED are taken from models.py (Tim's approved
portraits) instead; the rest keep their stand-in until approved.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import models                                   # the approved portraits (models.APPROVED)

def canvas():
    return [[0] * 32 for _ in range(32)]

def px(c, x, y, v=1):
    if 0 <= x < 32 and 0 <= y < 32:
        c[y][x] = v

def hline(c, x0, x1, y):
    for x in range(x0, x1 + 1): px(c, x, y)

def vline(c, x, y0, y1):
    for y in range(y0, y1 + 1): px(c, x, y)

def box(c, x0, y0, x1, y1, round_=False):
    hline(c, x0, x1, y0); hline(c, x0, x1, y1); vline(c, x0, y0, y1); vline(c, x1, y0, y1)
    if round_:
        for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)): px(c, x, y, 0)
        px(c, x0 + 1, y0 + 1); px(c, x1 - 1, y0 + 1); px(c, x0 + 1, y1 - 1); px(c, x1 - 1, y1 - 1)

def fill(c, x0, y0, x1, y1, v=1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1): px(c, x, y, v)

def dots(c, x0, x1, y, step=2):
    for x in range(x0, x1 + 1, step): px(c, x, y)

def glyph(c, x, y, rows):
    for j, r in enumerate(rows):
        for i, ch in enumerate(r):
            if ch == '#': px(c, x + i, y + j)

def compact():
    c = canvas()
    box(c, 7, 1, 24, 25, True)
    box(c, 9, 3, 22, 15, True)
    hline(c, 15, 21, 19)
    px(c, 10, 22)
    box(c, 8, 26, 23, 29)
    fill(c, 9, 27, 22, 28)
    return c

def desktop():
    c = canvas()
    box(c, 4, 1, 27, 19, True)
    box(c, 7, 4, 24, 16)
    fill(c, 13, 20, 18, 21)
    box(c, 1, 22, 30, 29)
    hline(c, 19, 27, 25)
    px(c, 4, 25)
    return c

def lisa():
    c = canvas()
    box(c, 1, 4, 30, 27, True)
    box(c, 3, 6, 17, 18, True)          # the Lisa's screen sits to the left
    box(c, 20, 7, 28, 11)               # floppy drive
    hline(c, 22, 26, 9)
    glyph(c, 20, 15, ["#...#.#....",
                      ".#.#..#....",
                      "..#...#....",
                      ".#.#..#....",
                      "#...#.####."])   # "XL"
    hline(c, 3, 17, 22); hline(c, 3, 17, 24)
    fill(c, 3, 28, 6, 29); fill(c, 25, 28, 28, 29)
    return c

def printer():
    c = canvas()
    box(c, 9, 2, 22, 11)
    hline(c, 11, 20, 5); hline(c, 11, 18, 7); hline(c, 11, 19, 9)
    box(c, 2, 11, 29, 26, True)
    hline(c, 5, 26, 15)
    px(c, 25, 21); px(c, 23, 21)
    hline(c, 4, 27, 27)
    return c

def server():
    c = canvas()
    box(c, 9, 1, 22, 30, True)
    box(c, 11, 4, 20, 7); hline(c, 13, 18, 6)
    box(c, 11, 9, 20, 12); hline(c, 13, 18, 11)
    for y in range(16, 24, 2): hline(c, 12, 19, y)
    px(c, 12, 27); px(c, 14, 27)
    return c

def router():
    c = canvas()
    box(c, 1, 10, 30, 22, True)
    hline(c, 6, 25, 14)
    for i in range(3): vline(c, 22 + i, 12 + i, 16 - i)      # arrow heads
    hline(c, 6, 25, 18)
    for i in range(3): vline(c, 9 - i, 16 + i, 20 - i)
    vline(c, 6, 4, 9); vline(c, 25, 4, 9)
    px(c, 5, 3); px(c, 7, 3); px(c, 24, 3); px(c, 26, 3)
    return c

def apple2():
    c = canvas()
    box(c, 6, 1, 25, 14, True)
    box(c, 8, 3, 23, 12)
    box(c, 2, 16, 29, 23, True)
    hline(c, 5, 26, 19)
    box(c, 2, 24, 29, 29)
    for y in (25, 27): dots(c, 4, 27, y)
    return c

def portable():
    c = canvas()
    box(c, 5, 2, 26, 17, True)
    box(c, 7, 4, 24, 15)
    box(c, 2, 18, 29, 27, True)
    for y in (20, 22): dots(c, 4, 27, y)
    box(c, 13, 24, 18, 26)
    return c

def zone():
    c = canvas()
    import math
    for a in range(0, 360, 3):
        r = math.radians(a)
        px(c, round(15.5 + 13 * math.cos(r)), round(15.5 + 13 * math.sin(r)))
        px(c, round(15.5 + 6 * math.cos(r)), round(15.5 + 13 * math.sin(r)))
    vline(c, 15, 3, 28)
    hline(c, 3, 28, 15)
    hline(c, 6, 25, 9); hline(c, 6, 25, 22)
    return c

def unix():
    c = canvas()
    box(c, 2, 6, 29, 25, True)
    for x, y in ((4, 8), (27, 8), (4, 23), (27, 23)): px(c, x, y)
    box(c, 7, 11, 14, 18); fill(c, 9, 13, 12, 16)
    box(c, 17, 11, 22, 15)
    box(c, 25, 11, 29, 15); box(c, 25, 18, 29, 22)
    dots(c, 6, 22, 8)
    return c

def unknown():
    c = canvas()
    box(c, 6, 2, 25, 29, True)
    glyph(c, 12, 8, ["..####..",
                     ".##..##.",
                     ".....##.",
                     "....##..",
                     "...##...",
                     "...##...",
                     "........",
                     "...##...",
                     "...##..."])
    return c

def macii():
    """Mac II family: monitor on a squarish case with floppy slots on the right (II, IIx, IIcx, IIci, IIsi, Quadra 700)."""
    c = canvas()
    box(c, 3, 1, 22, 15, True)          # monitor
    box(c, 5, 3, 20, 13)
    fill(c, 10, 16, 15, 16)
    box(c, 1, 17, 30, 30)               # tall case
    hline(c, 21, 28, 21); hline(c, 21, 28, 25)   # two floppy slots
    px(c, 4, 27); px(c, 6, 27)          # power light, apple
    hline(c, 3, 17, 20)
    return c

def pizza():
    """LC "pizza box": a big monitor on a thin case (LC, LC II/III, Performa 4xx, Quadra 605/610)."""
    c = canvas()
    box(c, 4, 1, 27, 20, True)
    box(c, 6, 3, 25, 18)
    fill(c, 14, 21, 17, 22)
    box(c, 1, 23, 30, 28)
    hline(c, 12, 19, 25)                # centred floppy slot
    px(c, 4, 25)
    return c

def tower():
    """Tower: a tall case beside a small monitor (Quadra 650/800/840/900/950, Workgroup Server)."""
    c = canvas()
    box(c, 1, 2, 13, 30, True)          # tower
    hline(c, 3, 11, 6); hline(c, 3, 11, 9)       # drive bays
    hline(c, 5, 9, 13)                  # floppy
    px(c, 3, 27)
    box(c, 15, 9, 30, 23, True)         # monitor
    box(c, 17, 11, 28, 21)
    fill(c, 21, 24, 24, 25)
    hline(c, 18, 27, 26)
    return c

def allinone():
    """All-in-one with a colour screen: Color Classic, LC 520/575, Performa 5xx, Macintosh TV."""
    c = canvas()
    box(c, 4, 1, 27, 24, True)
    box(c, 7, 3, 24, 16, True)
    hline(c, 9, 15, 20)                 # floppy slot, under the screen on the left
    px(c, 22, 20)
    box(c, 2, 25, 29, 29)               # wide foot
    fill(c, 3, 26, 28, 28)
    return c

def powermac():
    """Power Macintosh: monitor on a flat case with a vent grille."""
    c = canvas()
    box(c, 4, 1, 27, 18, True)
    box(c, 6, 3, 25, 16)
    fill(c, 13, 19, 18, 20)
    box(c, 1, 21, 30, 29)
    for x in range(4, 14, 2): vline(c, x, 23, 27)    # grille
    hline(c, 19, 27, 25)
    return c

def line(c, x0, y0, x1, y1):
    n = max(abs(x1 - x0), abs(y1 - y0)) or 1
    for i in range(n + 1):
        px(c, round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n))

def app():
    c = canvas()
    for y in range(7, 29):                       # one mountain
        half = (y - 7) * 14 // 21
        px(c, 16 - half, y); px(c, 16 + half, y)
    for y in range(7, 12):                       # snow cap
        half = (y - 7) * 14 // 21
        hline(c, 16 - half, 16 + half, y)
    for x in range(12, 21, 2): px(c, x, 12)
    hline(c, 1, 30, 29)
    vline(c, 16, 1, 7); fill(c, 17, 1, 22, 4)     # flag on the summit
    pts = [(9, 26), (14, 22), (19, 24), (21, 18)]  # the route, as network nodes
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]): line(c, x0, y0, x1, y1)
    for x, y in pts: fill(c, x - 1, y - 1, x + 1, y + 1)
    return c

def doc():
    """A saved scan: a page with the folded corner, and the application's mountain on it (1.0.1, Tim 29 Oct)."""
    c = canvas()
    hline(c, 4, 21, 1); vline(c, 4, 1, 30); hline(c, 4, 27, 30); vline(c, 27, 7, 30)   # the page
    line(c, 21, 1, 27, 7)                                                              # the fold
    vline(c, 21, 1, 7); hline(c, 21, 27, 7)
    for y in range(12, 26):                      # the mountain, two thirds size
        half = (y - 12) * 9 // 13
        px(c, 16 - half, y); px(c, 16 + half, y)
    for y in range(12, 15):                      # snow cap
        half = (y - 12) * 9 // 13
        hline(c, 16 - half, 16 + half, y)
    hline(c, 6, 26, 26)
    vline(c, 16, 8, 12); fill(c, 17, 8, 20, 10)   # the flag
    pts = [(11, 24), (14, 21), (18, 23), (19, 18)]  # the route
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]): line(c, x0, y0, x1, y1)
    for x, y in pts: fill(c, x - 1, y - 1, x + 1, y + 1)
    return c

def mask(c):
    out = [[0] * 32 for _ in range(32)]
    st = [(x, y) for x in range(32) for y in (0, 31)] + [(x, y) for y in range(32) for x in (0, 31)]
    while st:
        x, y = st.pop()
        if not (0 <= x < 32 and 0 <= y < 32) or out[y][x] or c[y][x]:
            continue
        out[y][x] = 1
        st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return [[0 if out[y][x] else 1 for x in range(32)] for y in range(32)]

def hexrows(c):
    return ''.join(''.join('%02X' % int(''.join(str(b) for b in row[i:i + 8]), 2) for i in range(0, 32, 8)) for row in c)

# order = the kinds in sherpa.h
KINDS = [("Device", unknown), ("Compact Mac", compact), ("Macintosh", desktop), ("Lisa / Macintosh XL", lisa),
         ("Printer", printer), ("File server", server), ("Router", router), ("Apple II", apple2),
         ("PowerBook", portable), ("Zone", zone), ("Netatalk server", unix),
         ("Macintosh II", macii), ("LC", pizza), ("Tower", tower), ("All-in-one", allinone),
         ("Power Macintosh", powermac),
         ("Dot-matrix printer", None)]              # no stand-in: models.py's ImageWriter (approved)

def rez(rid, name, c):
    h = hexrows(c) + hexrows(mask(c))
    lines = '\n'.join('\t$"%s"' % h[i:i + 64] for i in range(0, len(h), 64))
    return 'data \'ICN#\' (%d, "%s") {\n%s\n};\n' % (rid, name, lines)

def show(c):
    for row in c: print(''.join('#' if b else '.' for b in row))

if __name__ == '__main__':
    out = ['/* icons.r: written by tools/mkicons.py; edit that, not this. */\n']
    out.append(rez(128, "GlobalTalk Sherpa", app()))
    out.append(rez(129, "Sherpa scan", doc()))     # the document icon (BNDL: FREF 129 'GTSi' -> ICN# 129)
    h = hexrows(app())                          # plain ICON 128 too: dialog Icon items (About) draw an ICON
    out.append('data \'ICON\' (128, "GlobalTalk Sherpa") {\n%s\n};\n' % '\n'.join('\t$"%s"' % h[i:i + 64] for i in range(0, len(h), 64)))
    for k, (name, f) in enumerate(KINDS):
        if k in models.APPROVED and k < len(models.MODELS):   # Tim's approved portrait replaces the stand-in
            out.append(models.rez(200 + k, name, models.MODELS[k][1]))
        elif f is None:
            raise SystemExit("kind %d (%s) has no stand-in and is not approved" % (k, name))
        else:
            out.append(rez(200 + k, name, f()))
    open(sys.argv[1], 'w').write('\n'.join(out))
    if '--show' in sys.argv:
        for name, f in [("app", app)] + KINDS:
            print(name); show(f()); print()
