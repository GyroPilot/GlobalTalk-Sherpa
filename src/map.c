/* map.c: the three map levels.
 *   0  World: every zone as an island, the local network's zones with a heavy border;
 *      or (Map > Network Diagram) a bus diagram with every device hanging off its zone
 *   1  Zone:  one zone's networks as strips of device icons
 *   2  Node:  one device's card (model, services, printers, server info, reliability)
 * Click to go in; click the bar at the top (or Cmd-U) to come back out.
 * One routine lays out, draws and hit-tests, so the three never disagree. */
#include "sherpa.h"
#include <Multiverse.h>
#include <Fonts.h>
#include <TextUtils.h>

short gLevel, gCurZone, gCurNode;

#define kHeader 22
#define kZoneW 136
#define kZoneH 58
#define kNodeW 92
#define kNodeH 70
#define kGap 8

enum { kDraw, kMeasure, kHit };

static const char *kKindName[kNumKinds] = {
    "Device", "Compact Mac", "Macintosh", "Lisa / Macintosh XL", "Printer", "File server",
    "Router", "Apple II", "PowerBook", "Zone", "Netatalk server",
    "Macintosh II", "LC", "Tower", "All-in-one", "Power Macintosh", "Dot-matrix printer"
};

static void Small(void) { TextFont(kFontIDGeneva); TextSize(9); TextFace(0); }
static void Big(void) { TextFont(systemFont); TextSize(12); TextFace(0); }

/* Draw p, cut with an ellipsis to fit width. */
/* Copy p without leading spaces (GlobalTalk has models like "  Macintosh"). */
static void Trimmed(unsigned char *s, ConstStringPtr p)
{
    short sp = 0;
    while (sp < p[0] && p[1 + sp] == ' ') sp++;
    s[0] = p[0] - sp > 60 ? 60 : p[0] - sp;
    BlockMoveData(p + 1 + sp, s + 1, s[0]);
}

static void DrawFit(ConstStringPtr p, short width)
{
    unsigned char s[64];
    Trimmed(s, p);
    if (StringWidth(s) <= width) { DrawString(s); return; }
    while (s[0] > 1 && StringWidth(s) + CharWidth('\311') > width) s[0]--;
    s[++s[0]] = '\311';
    DrawString(s);
}

static void DrawFitCentered(ConstStringPtr p, short cx, short y, short width)
{
    unsigned char s[64];
    Trimmed(s, p);
    while (s[0] > 1 && StringWidth(s) > width) { s[0]--; s[s[0]] = '\311'; }
    MoveTo(cx - StringWidth(s) / 2, y);
    DrawString(s);
}

static void DrawC(const char *c) { unsigned char p[256]; CtoP(c, p); DrawString(p); }

/* Plot a 32x32 ICN# (icon + mask) into r, which may be bigger (scaled). */
static void PlotICN(Handle icn, Rect *r)
{
    BitMap bm;
    Rect src = { 0, 0, 32, 32 };
    GrafPtr port;
    char state;
    if (!icn) { FrameRect(r); return; }
    GetPort(&port);
    state = HGetState(icn);
    HLock(icn);
    bm.rowBytes = 4;
    bm.bounds = src;
    bm.baseAddr = *icn + 128;
    CopyBits(&bm, &port->portBits, &src, r, srcBic, NULL);
    bm.baseAddr = *icn;
    CopyBits(&bm, &port->portBits, &src, r, srcOr, NULL);
    HSetState(icn, state);
}

/* A device's icon; a Mac that is also a router gets a small router badge in its corner. */
static void PlotNode(Node *n, Rect *r)
{
    short kind;
    PlotICN(IconFor(n, &kind), r);
    if (n->isRouter && n->kind != kRouter) {
        Rect b;
        short x, y;
        SetRect(&b, r->right - 13, r->bottom - 9, r->right + 1, r->bottom + 1);
        EraseRect(&b);
        FrameRect(&b);
        x = b.left + 3; y = b.top + 3;
        MoveTo(x, y); LineTo(x + 7, y); MoveTo(x + 5, y - 1); LineTo(x + 5, y + 1);   /* -> */
        y += 3;
        MoveTo(x, y); LineTo(x + 7, y); MoveTo(x + 2, y - 1); LineTo(x + 2, y + 1);   /* <- */
    }
}

void DrawRelBar(short x, short y, Node *n)
{
    short r = HistRating(n), i;
    Rect b;
    if (r == 0) { MoveTo(x, y + 6); Small(); DrawC("new"); return; }
    for (i = 0; i < 5; i++) {
        SetRect(&b, x + i * 6, y, x + i * 6 + 5, y + 6);
        if (i < r) PaintRect(&b); else FrameRect(&b);
    }
}

static void Header(Rect *area, Boolean draw)
{
    Rect h = *area;
    unsigned char s[100];
    if (!draw) return;
    h.bottom = h.top + kHeader;
    EraseRect(&h);
    MoveTo(h.left, h.bottom - 1); LineTo(h.right, h.bottom - 1);
    Big();
    MoveTo(h.left + 6, h.top + 15);
    if (gLevel == 0) {
        CtoP(gScanIsLive || gNumZones == 0 ? "GlobalTalk" : "GlobalTalk (saved scan)", s);
        DrawString(s);
        {
            short x = h.left + 6 + StringWidth(s) + 16;    /* measured in the title's font */
            Small(); MoveTo(x, h.top + 14);
        }
        if (gNumZones == 0) DrawC("Choose Scan > Scan Local Network or Scan All of GlobalTalk");
        else {
            short z, answered = 0, scanned = 0;
            for (z = 0; z < gNumZones; z++) if (gZones[z].scanned) { scanned++; if (gZones[z].got > 0) answered++; }
            CtoP("", s); PCatNum(s, gNumZones); PCatC(s, " zones");
            if (gScanIsLive && scanned) { PCatC(s, " ("); PCatNum(s, answered); PCatC(s, " answered)"); }   /* 1.0.1 */
            PCatC(s, ", "); PCatNum(s, gNumNodes); PCatC(s, " devices"); DrawString(s);
        }
    } else {
        DrawC("< ");
        if (gLevel == 1) { DrawC("All zones   "); TextFace(bold); DrawString(gZones[gCurZone].name); }
        else { DrawString(gZones[gCurZone].name); DrawC("   "); TextFace(bold); DrawString(gNames[gNodes[gCurNode].label].obj); }
        TextFace(0);
    }
}

/* ---- level 0 ---- */
static short World(short mode, Rect *a, short scroll, Point pt)
{
    short cols = (a->right - a->left - kGap) / (kZoneW + kGap), i, x, y;
    Rect r, ir;
    unsigned char s[40];
    if (cols < 1) cols = 1;
    for (i = 0; i < gNumZones; i++) {
        x = a->left + kGap + (i % cols) * (kZoneW + kGap);
        y = a->top + kHeader + kGap + (i / cols) * (kZoneH + kGap) - scroll;
        SetRect(&r, x, y, x + kZoneW, y + kZoneH);
        if (mode == kHit) { if (PtInRect(pt, &r)) return i; continue; }
        if (mode != kDraw || r.bottom < a->top + kHeader || r.top > a->bottom) continue;
        EraseRoundRect(&r, 14, 14);
        if (!gZones[i].scanned) PenPat(&qd.gray);
        if (gZones[i].lab) PenSize(3, 3);
        FrameRoundRect(&r, 14, 14);
        PenNormal();
        SetRect(&ir, x + 6, y + 13, x + 38, y + 45);
        PlotICN(GetResource('ICN#', 200 + kZone), &ir);
        Small(); TextFace(bold);
        MoveTo(x + 42, y + 20); DrawFit(gZones[i].name, kZoneW - 48);
        TextFace(0);
        MoveTo(x + 42, y + 33);
        if (!gZones[i].scanned) DrawC("not scanned");
        else { CtoP("", s); PCatNum(s, gZones[i].nodes); PCatC(s, gZones[i].nodes == 1 ? " device" : " devices"); DrawString(s); }
        MoveTo(x + 42, y + 45);
        if (gZones[i].lab) DrawC("local");
    }
    if (mode == kHit) return -1;
    return kHeader + kGap + ((gNumZones + cols - 1) / cols) * (kZoneH + kGap);
}

/* ---- level 0, as a network diagram ----
 * Like the old AppleTalk mappers: a backbone across each row, a drop line down from it
 * for each zone, and that zone's devices tapped off the drop. Rows wrap to the window,
 * so a Plus gets two zones across. Hit: a device gives its index, a zone's name -2 - zone. */
#define kColW 168
#define kDevH 40
#define kRowTop 30

static short Diagram(short mode, Rect *a, short scroll, Point pt)
{
    short cols = (a->right - a->left - kGap) / kColW, z, c, i, k, x, y, rowH, most, inRow;
    short y0 = a->top + kHeader + 6 - scroll;
    Rect r, ir;
    unsigned char s[40];
    if (cols < 1) cols = 1;
    for (z = 0; z < gNumZones; z += cols) {
        inRow = gNumZones - z < cols ? gNumZones - z : cols;
        most = 0;
        for (c = 0; c < inRow; c++) if (gZones[z + c].nodes > most) most = gZones[z + c].nodes;
        rowH = kRowTop + (most ? most * kDevH : 14) + 14;
        if (mode == kDraw && y0 < a->bottom && y0 + rowH > a->top + kHeader) {
            short right = a->left + kGap + inRow * kColW - 8;
            MoveTo(a->left + kGap, y0 + 18); LineTo(right, y0 + 18);     /* the backbone */
            MoveTo(a->left + kGap, y0 + 21); LineTo(right, y0 + 21);
        }
        for (c = 0; c < inRow; c++) {
            ATZone *zn = &gZones[z + c];
            x = a->left + kGap + c * kColW;
            SetRect(&r, x, y0, x + kColW - 8, y0 + 16);
            if (mode == kHit && PtInRect(pt, &r)) return -2 - (z + c);
            k = 0;
            for (i = 0; i < gNumNodes; i++) {
                Node *n = &gNodes[i];
                if (n->zone != z + c) continue;
                y = y0 + kRowTop + k * kDevH;
                k++;
                SetRect(&r, x + 14, y, x + kColW - 8, y + kDevH - 4);
                if (mode == kHit) { if (PtInRect(pt, &r)) return i; continue; }
                if (mode != kDraw || r.bottom < a->top + kHeader || r.top > a->bottom) continue;
                MoveTo(x + 4, y + 16); LineTo(x + 18, y + 16);           /* the tap */
                SetRect(&ir, x + 18, y, x + 50, y + 32);
                PlotNode(n, &ir);
                Small();
                MoveTo(x + 54, y + 13);
                TextFace(italic); DrawFit(gNames[n->label].obj, kColW - 64); TextFace(0);
                MoveTo(x + 54, y + 25);
                if (n->model >= 0) DrawFit(gNames[n->model].type, kColW - 64);
                else { CtoP(kKindName[n->kind], s); DrawFit(s, kColW - 64); }
            }
            if (mode != kDraw || y0 + rowH < a->top + kHeader || y0 > a->bottom) continue;
            Small(); TextFace(bold);
            MoveTo(x + 2, y0 + 12); DrawFit(zn->name, kColW - 12);
            TextFace(0);
            if (zn->lab) PenSize(2, 2);
            if (!zn->scanned) PenPat(&qd.gray);
            MoveTo(x + 4, y0 + 21);
            LineTo(x + 4, k ? y0 + kRowTop + (k - 1) * kDevH + 16 : y0 + 34);  /* the zone's drop */
            PenNormal();
            if (!k) { MoveTo(x + 12, y0 + 36); DrawC(zn->scanned ? "no devices" : "not scanned"); }
        }
        y0 += rowH;
    }
    if (mode == kHit) return -1;
    return y0 + scroll - a->top;
}

/* ---- level 1 ---- */
static short ZoneView(short mode, Rect *a, short scroll, Point pt)
{
    short cols = (a->right - a->left - kGap) / (kNodeW + kGap), i, k = 0, x, y;
    short top = a->top + kHeader + 4 - scroll, lastNet = -1, row = 0;
    Rect r, ir;
    unsigned char s[40];
    if (cols < 1) cols = 1;
    y = top;
    for (i = 0; i < gNumNodes; i++) {
        Node *n = &gNodes[i];
        if (n->zone != gCurZone) continue;
        if ((short)n->net != lastNet) {          /* a new network strip */
            if (lastNet != -1) y += kNodeH + kGap;
            lastNet = n->net; k = 0;
            if (mode == kDraw) {
                Small(); TextFace(bold);
                MoveTo(a->left + kGap, y + 12);
                CtoP("Network ", s); PCatNum(s, n->net); DrawString(s);
                TextFace(0);
                MoveTo(a->left + kGap + 80, y + 8); LineTo(a->right - kGap, y + 8);
            }
            y += 18;
        } else if (k % cols == 0) y += kNodeH + kGap;
        x = a->left + kGap + (k % cols) * (kNodeW + kGap);
        k++;
        SetRect(&r, x, y, x + kNodeW, y + kNodeH);
        if (mode == kHit) { if (PtInRect(pt, &r)) return i; continue; }
        if (mode != kDraw || r.bottom < a->top + kHeader || r.top > a->bottom) continue;
        SetRect(&ir, x + (kNodeW - 32) / 2, y + 2, x + (kNodeW + 32) / 2, y + 34);
        PlotNode(n, &ir);
        Small();
        DrawFitCentered(gNames[n->label].obj, x + kNodeW / 2, y + 45, kNodeW - 4);
        if (n->model >= 0) DrawFitCentered(gNames[n->model].type, x + kNodeW / 2, y + 56, kNodeW - 4);
        else { CtoP(kKindName[n->kind], s); DrawFitCentered(s, x + kNodeW / 2, y + 56, kNodeW - 4); }
        DrawRelBar(x + kNodeW / 2 - 15, y + 60, n);
        (void)row;
    }
    if (mode == kHit) return -1;
    if (lastNet == -1) {
        if (mode == kDraw) { Small(); MoveTo(a->left + kGap, top + 20); DrawC(gZones[gCurZone].scanned ? "No devices answered in this zone." : "This zone has not been scanned yet."); }
        return kHeader + 40;
    }
    return y + kNodeH + kGap + scroll - a->top;
}

/* ---- level 2 ---- */
static void CardLine(short *y, short x, const char *label, ConstStringPtr val)
{
    MoveTo(x, *y);
    TextFace(bold); DrawC(label); TextFace(0);
    if (val) DrawString(val);
    *y += 12;
}

static void DateText(unsigned long secs, unsigned char *s)
{
    Str255 t;
    secs = FixYear(secs);
    IUDateString(secs, abbrevDate, s);
    IUTimeString(secs, false, t);
    PCatC(s, " "); PCat(s, t);
}

static short NodeView(short mode, Rect *a, short scroll)
{
    Node *n = &gNodes[gCurNode];
    short y = a->top + kHeader + 16 - scroll, x = a->left + 92, i;
    Rect ir;
    unsigned char s[256];
    Hist *h = n->hist >= 0 ? &gHist[n->hist] : NULL;
    Boolean draw = mode == kDraw;

    if (draw) {
        SetRect(&ir, a->left + 12, y - 4, a->left + 76, y + 60);
        PlotNode(n, &ir);
        if (n->srvIcon) {
            Handle t = n->srvIcon;
            SetRect(&ir, a->right - 48, y - 4, a->right - 16, y + 28);
            PlotICN(t, &ir);
        }
        Big(); TextFace(bold);
        MoveTo(x, y + 4); DrawString(gNames[n->label].obj);
        Small();
    }
    y += 22;
    if (draw) {
        if (n->model >= 0) CardLine(&y, x, "Model: ", gNames[n->model].type);
        else { CtoP(kKindName[n->kind], s); CardLine(&y, x, "Kind: ", s); }
        if (n->isRouter && n->kind != kRouter) { CtoP("yes, an AppleTalk router too", s); CardLine(&y, x, "Router: ", s); }
        CardLine(&y, x, "Zone: ", gZones[n->zone].name);
        CtoP("", s); PCatNum(s, n->net); PCatC(s, "."); PCatNum(s, n->node);
        CardLine(&y, x, "AppleTalk address: ", s);
        MoveTo(x, y); TextFace(bold); DrawC("Reliability: "); TextFace(0);
        DrawRelBar(x + 72, y - 6, n);
        MoveTo(x + 108, y);
        if (h) { CtoP("seen in ", s); PCatNum(s, h->seen); PCatC(s, " of "); PCatNum(s, h->chances); PCatC(s, " scans"); DrawString(s); }
        y += 12;
        if (h) { DateText(h->first, s); CardLine(&y, x, "First seen: ", s); DateText(h->last, s); CardLine(&y, x, "Last seen: ", s); }
    } else y += h ? 72 : 48;
    y += 8;
    if (draw) { MoveTo(a->left + 12, y); TextFace(bold); DrawC("Services on the network"); TextFace(0); }
    y += 13;
    for (i = n->first; i < n->first + n->count; i++) {
        if (draw) {
            Name *nm = &gNames[i];
            MoveTo(a->left + 20, y);
            DrawFit(nm->type, 130);
            MoveTo(a->left + 156, y); DrawFit(nm->obj, a->right - a->left - 230);
            MoveTo(a->right - 64, y); CtoP("socket ", s); PCatNum(s, nm->socket); DrawString(s);
        }
        y += 12;
    }
    if (n->isServer) {
        y += 8;
        if (draw) { MoveTo(a->left + 12, y); TextFace(bold); DrawC("File server"); TextFace(0); }
        y += 13;
        if (!n->infoDone && draw) { MoveTo(a->left + 20, y); DrawC(gScanIsLive ? "Checking\311" : "(saved scan: not checked)"); }
        if (n->info) {
            char st = HGetState(n->info);
            long len = GetHandleSize(n->info), p0 = 0, p;
            HLock(n->info);
            for (p = 0; p < len; p++) {
                if ((*n->info)[p] != '\r') continue;
                if (draw) {
                    short L = p - p0 > 200 ? 200 : p - p0;
                    s[0] = L; BlockMoveData(*n->info + p0, s + 1, L);
                    MoveTo(a->left + 20, y); DrawString(s);
                }
                y += 12; p0 = p + 1;
            }
            HSetState(n->info, st);
        } else y += 12;
    }
    y += 10;
    if (draw) { MoveTo(a->left + 12, y); DrawC("To change this picture: copy an icon or picture, then Edit > Paste Icon."); }
    return y + 16 + scroll - a->top;
}

/* ---- level 0, as a dashboard ----
 * The latest scan, the devices on the map by kind (bars), the recent scans (the 'SScn'
 * history: zones, devices, new and gone), and which devices were new or gone in this
 * session's latest scan. Fits a 512-pixel screen. Nothing to click. */
static void Section(short *y, short x, short right, const char *title, Boolean draw)
{
    *y += 10;
    if (draw) {
        Small(); TextFace(bold);
        MoveTo(x, *y); DrawC(title);
        TextFace(0);
        MoveTo(x, *y + 3); LineTo(right, *y + 3);
    }
    *y += 15;
}

static void NumAt(short x, short y, long v)
{
    unsigned char s[16];
    CtoP("", s); PCatNum(s, v);
    MoveTo(x - StringWidth(s), y);              /* right-aligned at x */
    DrawString(s);
}

static void ChangeList(short *y, short x, short right, DevRef *list, short n, Boolean draw)
{
    unsigned char s[80];
    short i;
    for (i = 0; i < n && i < kMaxChanges && i < 12; i++) {
        if (draw) {
            MoveTo(x + 8, *y);
            TextFace(italic); DrawFit(list[i].label, (right - x) / 2); TextFace(0);
            MoveTo(x + 8 + (right - x) / 2 + 8, *y); DrawFit(list[i].zone, (right - x) / 2 - 24);
        }
        *y += 12;
    }
    if (n > i) {
        if (draw) { MoveTo(x + 8, *y); CtoP("and ", s); PCatNum(s, n - i); PCatC(s, " more"); DrawString(s); }
        *y += 12;
    }
}

/* After a live Scan Local Network, the dashboard says where its zones came from. */
static Boolean RouterLine(void)
{
    return gScanIsLive && gNumScans > 0 && !gScans[0].all && (gPrefs.labCount || gRouterNote[0]);
}

static short Dashboard(short mode, Rect *a, short scroll)
{
    Boolean draw = mode == kDraw;
    short x = a->left + 12, right = a->right - 12, y = a->top + kHeader + 4 - scroll, i, k, most = 0, routers = 0, macRouters = 0;
    short count[kNumKinds], order[kNumKinds], nk = 0;
    unsigned char s[100];
    Rect r;

    /* the latest scan */
    Section(&y, x, right, "Latest scan", draw);
    if (draw) {
        Small(); MoveTo(x, y);
        if (gNumScans == 0) DrawC("No live scans yet. Choose Scan > Scan Local Network or Scan All of GlobalTalk.");
        else {
            ScanSum *ss = &gScans[0];
            DateText(ss->when, s); DrawString(s);
            DrawC(ss->all ? "   All of GlobalTalk" : "   Local network");
            MoveTo(x, y + 12);
            CtoP("", s); PCatNum(s, ss->zones); PCatC(s, ss->zones == 1 ? " zone, " : " zones, ");
            PCatNum(s, ss->devices); PCatC(s, " devices, "); PCatNum(s, ss->names); PCatC(s, " names.");
            DrawString(s);
            MoveTo(x, y + 24);
            CtoP("", s); PCatNum(s, ss->added); PCatC(s, " new, "); PCatNum(s, ss->gone); PCatC(s, " gone since those zones were last scanned.");
            DrawString(s);
        }
        if (!gScanIsLive && gNumZones > 0) {
            MoveTo(x, y + (gNumScans ? 36 : 12));
            DrawC("The map shows a saved scan; the history below is from live scans.");
        }
        if (RouterLine()) {                     /* where the local network's zones came from */
            MoveTo(x, y + 36);
            if (gPrefs.labCount) DrawC("Local network: the zones typed in Scan > Local Network Zones\311");
            else { DrawC("Local network: "); DrawString(gRouterNote); }
        }
    }
    y += (gNumScans ? 36 : 12) + (!gScanIsLive && gNumZones > 0 ? 12 : 0) + (RouterLine() ? 12 : 0);

    /* devices by kind, on the map now */
    for (k = 0; k < kNumKinds; k++) count[k] = 0;
    for (i = 0; i < gNumNodes; i++) {
        count[gNodes[i].kind]++;
        if (gNodes[i].isRouter) { routers++; if (gNodes[i].kind != kRouter) macRouters++; }
    }
    for (k = 0; k < kNumKinds; k++) if (count[k]) {     /* biggest first */
        for (i = nk; i > 0 && count[order[i - 1]] < count[k]; i--) order[i] = order[i - 1];
        order[i] = k; nk++;
        if (count[k] > most) most = count[k];
    }
    Section(&y, x, right, "Devices on the map, by kind", draw);
    if (nk == 0) { if (draw) { Small(); MoveTo(x, y); DrawC("Nothing on the map yet."); } y += 14; }
    for (i = 0; i < nk; i++) {
        k = order[i];
        if (draw && y + 4 > a->top + kHeader && y - 14 < a->bottom) {
            short barMax = right - x - 170, w;
            SetRect(&r, x, y - 12, x + 16, y + 4);
            PlotICN(KindIcon(k), &r);
            Small();
            MoveTo(x + 22, y); DrawC(kKindName[k]);
            NumAt(x + 140, y, count[k]);
            w = most ? (long)count[k] * barMax / most : 0;
            SetRect(&r, x + 150, y - 8, x + 150 + (w < 2 ? 2 : w), y);
            PaintRect(&r);
        }
        y += 18;
    }
    if (draw && routers) {
        Small(); MoveTo(x, y);
        CtoP("", s); PCatNum(s, routers); PCatC(s, routers == 1 ? " router" : " routers");
        if (macRouters) { PCatC(s, ", "); PCatNum(s, macRouters); PCatC(s, macRouters == 1 ? " of them a Mac" : " of them Macs"); }
        PCatC(s, ".");
        DrawString(s);
    }
    if (routers) y += 14;

    /* recent scans */
    Section(&y, x, right, "Recent scans", draw);
    if (draw) {
        Small(); TextFace(underline);
        MoveTo(x, y); DrawC("When");
        MoveTo(x + 112, y); DrawC("Scan");
        MoveTo(x + 178, y); DrawC("Zones");
        MoveTo(x + 222, y); DrawC("Devices");
        MoveTo(x + 280, y); DrawC("New");
        MoveTo(x + 318, y); DrawC("Gone");
        TextFace(0);
    }
    y += 13;
    if (gNumScans == 0) { if (draw) { MoveTo(x, y); DrawC("None yet."); } y += 12; }
    for (i = 0; i < gNumScans; i++) {
        ScanSum *ss = &gScans[i];
        if (draw && y > a->top + kHeader && y - 12 < a->bottom) {
            Str255 t;
            IUDateString(FixYear(ss->when), shortDate, s);
            IUTimeString(FixYear(ss->when), false, t);
            PCatC(s, " "); PCat(s, t);
            MoveTo(x, y); DrawFit(s, 108);
            MoveTo(x + 112, y); DrawC(ss->all ? "All" : "Local");
            NumAt(x + 208, y, ss->zones);
            NumAt(x + 258, y, ss->devices);
            NumAt(x + 300, y, ss->added);
            NumAt(x + 342, y, ss->gone);
            if (right - x > 420 && ss->devices) {   /* a small bar of the device count */
                short w = (long)ss->devices * (right - x - 360) / 100;
                if (w > right - x - 360) w = right - x - 360;
                SetRect(&r, x + 356, y - 7, x + 356 + (w < 1 ? 1 : w), y - 1);
                FillRect(&r, &qd.gray);
            }
        }
        y += 12;
    }

    /* what changed */
    Section(&y, x, right, "New since the previous scan", draw);
    if (gNumAdded < 0) { if (draw) { Small(); MoveTo(x, y); DrawC("Scan to see what changed."); } y += 12; }
    else if (gNumAdded == 0) { if (draw) { Small(); MoveTo(x, y); DrawC("Nothing new."); } y += 12; }
    else { if (draw) Small(); ChangeList(&y, x, right, gAdded, gNumAdded, draw); }
    Section(&y, x, right, "Gone since the previous scan", draw);
    if (gNumGone < 0) { if (draw) { Small(); MoveTo(x, y); DrawC("Scan to see what changed."); } y += 12; }
    else if (gNumGone == 0) { if (draw) { Small(); MoveTo(x, y); DrawC("Nothing gone."); } y += 12; }
    else { if (draw) Small(); ChangeList(&y, x, right, gGone, gNumGone, draw); }
    return y + 12 + scroll - a->top;
}

static short Walk(short mode, Rect *a, short scroll, Point pt)
{
    if (gLevel == 0) return gPrefs.view == kViewDiagram ? Diagram(mode, a, scroll, pt) :
                           gPrefs.view == kViewDashboard ? Dashboard(mode, a, scroll) : World(mode, a, scroll, pt);
    if (gLevel == 1) return ZoneView(mode, a, scroll, pt);
    return NodeView(mode, a, scroll);
}

void MapDraw(Rect *area, short scroll)
{
    Point p = { 0, 0 };
    Rect body = *area;
    RgnHandle old = NewRgn();
    GetClip(old);
    body.top += kHeader;
    EraseRect(&body);
    ClipRect(&body);
    Walk(kDraw, area, scroll, p);
    SetClip(old);
    DisposeRgn(old);
    Header(area, true);
}

short MapHeight(short width)
{
    Rect a;
    Point p = { 0, 0 };
    SetRect(&a, 0, 0, width, 30000);
    return Walk(kMeasure, &a, 0, p);
}

void MapClick(Point p, Rect *area, short scroll)
{
    short hit;
    if (p.v < area->top + kHeader) { if (gLevel > 0) MapZoomOut(); return; }
    hit = Walk(kHit, area, scroll, p);
    if (gLevel == 0 && gPrefs.view == kViewDiagram && hit <= -2) { gCurZone = -2 - hit; gLevel = 1; hit = -1; MapChanged(); }
    if (hit < 0 || gLevel == 2) return;
    if (gLevel == 0 && gPrefs.view == kViewDashboard) return;
    if (gLevel == 0 && gPrefs.view == kViewDiagram) { gCurNode = hit; gCurZone = gNodes[hit].zone; gLevel = 2; }
    else if (gLevel == 0) { gCurZone = hit; gLevel = 1; }
    else { gCurNode = hit; gLevel = 2; }
    MapChanged();
    if (gLevel == 2 && gNodes[gCurNode].isServer && !gNodes[gCurNode].infoDone) {
        NodeServerInfo(&gNodes[gCurNode]);
        MapChanged();
    }
}

void MapZoomOut(void)
{
    if (gLevel > 0) gLevel--;
    MapChanged();
}

/* The whole current view (not just what's on screen) as a picture, for Save as Picture. */
PicHandle MapPicture(short width)
{
    GrafPort port;
    GrafPtr old;
    PicHandle pic;
    Rect r;
    short h = MapHeight(width);
    static char bits[4];
    GetPort(&old);
    OpenPort(&port);
    /* a tiny bitmap: nothing really draws, but every call is recorded into the picture */
    port.portBits.baseAddr = bits; port.portBits.rowBytes = 2;
    SetRect(&port.portBits.bounds, 0, 0, 16, 1);
    SetRect(&r, 0, 0, width, h);
    port.portRect = r;
    RectRgn(port.visRgn, &r);
    RectRgn(port.clipRgn, &r);
    pic = OpenPicture(&r);
    ClipRect(&r);
    MapDraw(&r, 0);
    ClosePicture();
    ClosePort(&port);
    SetPort(old);
    return pic;
}
