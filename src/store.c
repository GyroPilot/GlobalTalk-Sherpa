/* store.c: everything Sherpa keeps between runs, and the files it saves.
 *
 * "GlobalTalk Sherpa Prefs" (in the Preferences folder on System 7, the System Folder on 6):
 *   resource fork  'Shrp' 128 = Prefs, 'STR#' 128 = lab zones,
 *                  'ICN#' 1000+ = custom icons, named "M:<model>" or "D:<zone>:<device>",
 *                  'SScn' 128 = the dashboard's scan history (ScanSum records, newest first)
 *   data fork      the reliability history, one tab-separated line per device
 * Files it writes: map pictures (PICT), scan lists (TEXT; Open Scan reads them back),
 * icon packs (type 'GTSi': just the custom ICN# resources, to share). */
#include "sherpa.h"
#include <Multiverse.h>
#include <OSUtils.h>
#include <stdlib.h>
#include <TextUtils.h>

Hist gHist[kMaxHist];
short gNumHist;
ScanSum gScans[kMaxScans];
short gNumScans;
DevRef gAdded[kMaxChanges], gGone[kMaxChanges];
short gNumAdded = -1, gNumGone = -1;
static short sPrefsRes = -1, sPrefsVol;
static const unsigned char kPrefsName[] = "\pGlobalTalk Sherpa Prefs";

/* ---------- finding the prefs file ---------- */

static Boolean TrapAvailable(short trap)
{
    TrapType t = (trap & 0x0800) ? kToolboxTrapType : kOSTrapType;
    if (t == kToolboxTrapType && (trap & 0x03FF) >= 0x0200) return false;
    return NGetTrapAddress(trap, t) != NGetTrapAddress(0xA89F, kToolboxTrapType);
}

static short PrefsVol(void)
{
    short vref;
    long dir;
    WDPBRec wd;
    SysEnvRec env;
    if (sPrefsVol) return sPrefsVol;
    if (TrapAvailable(0xA823) &&                 /* FindFolder: System 7 */
        FindFolder((short)0x8000, kPreferencesFolderType, true, &vref, &dir) == noErr) {
        memset(&wd, 0, sizeof wd);
        wd.ioVRefNum = vref; wd.ioWDDirID = dir; wd.ioWDProcID = 'GTSh';
        if (PBOpenWDSync(&wd) == noErr) return sPrefsVol = wd.ioVRefNum;
    }
    SysEnvirons(1, &env);                        /* System 6: the System Folder */
    return sPrefsVol = env.sysVRefNum;
}

static void OpenPrefsRes(void)
{
    short vol = PrefsVol(), old;
    if (sPrefsRes != -1) return;
    sPrefsRes = OpenRFPerm(kPrefsName, vol, fsRdWrPerm);
    if (sPrefsRes == -1) {
        Create(kPrefsName, vol, 'GTSh', 'pref');
        GetVol(NULL, &old);
        SetVol(NULL, vol);
        CreateResFile(kPrefsName);
        SetVol(NULL, old);
        sPrefsRes = OpenRFPerm(kPrefsName, vol, fsRdWrPerm);
    }
}

/* ---------- prefs ---------- */

void PrefsLoad(void)
{
    Handle h;
    short i, saved = CurResFile();
    gPrefs.version = 1;
    gPrefs.rescanMins = 0;
    gPrefs.nbpInterval = 4;                      /* 4 x 8 ticks, about half a second */
    gPrefs.nbpCount = 4;
    gPrefs.labCount = 0;
    OpenPrefsRes();
    if (sPrefsRes != -1) {
        UseResFile(sPrefsRes);
        h = Get1Resource('Shrp', 128);
        if (h && GetHandleSize(h) == sizeof(Prefs)) BlockMoveData(*h, &gPrefs, sizeof(Prefs));
        if (gPrefs.labCount > kMaxLab) gPrefs.labCount = 0;
        for (i = 0; i < gPrefs.labCount; i++) {
            Str255 s;
            GetIndString(s, 128, i + 1);
            PCopy(gLab[i], s, 32);
        }
        h = Get1Resource('SScn', 128);
        gNumScans = 0;
        if (h && GetHandleSize(h) % sizeof(ScanSum) == 0) {
            gNumScans = GetHandleSize(h) / sizeof(ScanSum);
            if (gNumScans > kMaxScans) gNumScans = kMaxScans;
            BlockMoveData(*h, gScans, gNumScans * sizeof(ScanSum));
        }
        UseResFile(saved);
    }
    /* history */
    {
        short ref;
        long len;
        Handle t;
        gNumHist = 0;
        if (FSOpen(kPrefsName, PrefsVol(), &ref) != noErr) return;
        GetEOF(ref, &len);
        t = NewHandle(len + 1);
        if (t) {
            char *p, *e, *f[7];
            short k;
            HLock(t);
            FSRead(ref, &len, *t);
            (*t)[len] = '\r';
            for (p = *t, e = *t + len; p < e && gNumHist < kMaxHist; ) {
                Hist *hh = &gHist[gNumHist];
                for (k = 0; k < 8; k++) {         /* split one line at tabs: 6 fields before 1.0.1, 7 since */
                    f[k] = p;
                    while (*p != '\t' && *p != '\r') p++;
                    if (*p == '\r') { if (k < 5) break; k++; break; }
                    *p++ = 0;
                }
                if (k >= 6) {
                    CtoP(f[0], hh->zone); CtoP(f[1], hh->label);
                    hh->seen = atoi(f[2]); hh->chances = atoi(f[3]);
                    hh->first = strtoul(f[4], NULL, 10); hh->last = strtoul(f[5], NULL, 10);
                    hh->misses = (k >= 7 && f[6][0] >= '0' && f[6][0] <= '9') ? atoi(f[6]) : 0;
                    gNumHist++;
                }
                while (p < e && *p != '\r') p++;
                p++;
            }
            DisposeHandle(t);
        }
        FSClose(ref);
    }
}

static void SaveHist(void)
{
    short ref, i;
    Handle t = NewHandle(0);
    long len;
    if (!t || FSOpen(kPrefsName, PrefsVol(), &ref) != noErr) return;
    for (i = 0; i < gNumHist; i++) {
        Hist *h = &gHist[i];
        unsigned char n[16];
        HAppendP(t, h->zone); HAppendC(t, "\t");
        HAppendP(t, h->label); HAppendC(t, "\t");
        CtoP("", n); PCatNum(n, h->seen); HAppendP(t, n); HAppendC(t, "\t");
        CtoP("", n); PCatNum(n, h->chances); HAppendP(t, n); HAppendC(t, "\t");
        CtoP("", n); PCatU(n, h->first); HAppendP(t, n); HAppendC(t, "\t");
        CtoP("", n); PCatU(n, h->last); HAppendP(t, n); HAppendC(t, "\t");
        CtoP("", n); PCatNum(n, h->misses); HAppendP(t, n); HAppendC(t, "\t\r");
    }
    len = GetHandleSize(t);
    HLock(t);
    SetFPos(ref, fsFromStart, 0);
    FSWrite(ref, &len, *t);
    SetEOF(ref, len);
    FSClose(ref);
    DisposeHandle(t);
}

void PrefsSave(void)
{
    Handle h;
    short i, saved = CurResFile();
    OpenPrefsRes();
    if (sPrefsRes == -1) return;
    UseResFile(sPrefsRes);
    h = Get1Resource('Shrp', 128);
    if (h) { RemoveResource(h); DisposeHandle(h); }
    if (PtrToHand(&gPrefs, &h, sizeof(Prefs)) == noErr) { AddResource(h, 'Shrp', 128, "\p"); WriteResource(h); }
    h = Get1Resource('STR#', 128);
    if (h) { RemoveResource(h); DisposeHandle(h); }
    h = NewHandle(2);
    **(short **)h = gPrefs.labCount;
    for (i = 0; i < gPrefs.labCount; i++) PtrAndHand(gLab[i], h, gLab[i][0] + 1);
    AddResource(h, 'STR#', 128, "\p");
    WriteResource(h);
    UpdateResFile(sPrefsRes);
    UseResFile(saved);
}

/* ---------- reliability history ---------- */

static short FindHist(ConstStringPtr zone, ConstStringPtr label)
{
    short i;
    for (i = 0; i < gNumHist; i++)
        if (PEqualNoCase(gHist[i].label, label) && PEqualNoCase(gHist[i].zone, zone)) return i;
    return -1;
}

void HistLink(void)
{
    short i;
    for (i = 0; i < gNumNodes; i++)
        gNodes[i].hist = FindHist(gZones[gNodes[i].zone].name, gNames[gNodes[i].label].obj);
}

static void SaveScans(void)
{
    Handle h;
    short saved = CurResFile();
    OpenPrefsRes();
    if (sPrefsRes == -1) return;
    UseResFile(sPrefsRes);
    h = Get1Resource('SScn', 128);
    if (h) { RemoveResource(h); DisposeHandle(h); }
    if (PtrToHand(gScans, &h, gNumScans * sizeof(ScanSum)) == noErr) { AddResource(h, 'SScn', 128, "\p"); WriteResource(h); }
    UpdateResFile(sPrefsRes);
    UseResFile(saved);
}

static void AddChange(DevRef *list, short *n, ConstStringPtr zone, ConstStringPtr label)
{
    if (*n < kMaxChanges) { PCopy(list[*n].zone, zone, 32); PCopy(list[*n].label, label, 32); }
    (*n)++;
}

/* The last time anything was seen in each zone: when that zone was last scanned, as far as
 * the history knows (0 = never). Devices seen then and not now are "gone"; devices not in the
 * history but in a zone with one are "new". */
static unsigned long sPrevSeen[kMaxZones];

void HistRecordScan(void)
{
    short i, z, h;
    unsigned long now;
    ScanSum *ss;
    now = NowFixed();
    for (z = 0; z < gNumZones; z++) sPrevSeen[z] = 0;
    for (i = 0; i < gNumHist; i++)
        for (z = 0; z < gNumZones; z++)
            if (gZones[z].scanned && PEqualNoCase(gZones[z].name, gHist[i].zone)) {
                if (gHist[i].last > sPrevSeen[z]) sPrevSeen[z] = gHist[i].last;
                break;
            }
    gNumAdded = gNumGone = 0;
    /* every device already known in a scanned zone had a chance to be seen */
    for (i = 0; i < gNumHist; i++)
        for (z = 0; z < gNumZones; z++)
            if (gZones[z].scanned && PEqualNoCase(gZones[z].name, gHist[i].zone)) {
                Hist *hh = &gHist[i];
                if (++hh->chances > 100) { hh->chances /= 2; hh->seen /= 2; }  /* favour recent scans */
                break;
            }
    for (i = 0; i < gNumNodes; i++) {
        Node *n = &gNodes[i];
        h = FindHist(gZones[n->zone].name, gNames[n->label].obj);
        if (h < 0) {
            if (gNumHist == kMaxHist) {          /* full: forget the longest-unseen device */
                short k, old = 0;
                for (k = 1; k < gNumHist; k++) if (gHist[k].last < gHist[old].last) old = k;
                gHist[old] = gHist[--gNumHist];
            }
            h = gNumHist++;
            PCopy(gHist[h].zone, gZones[n->zone].name, 32);
            PCopy(gHist[h].label, gNames[n->label].obj, 32);
            gHist[h].seen = 0; gHist[h].chances = 1; gHist[h].first = now;
            if (sPrevSeen[n->zone]) AddChange(gAdded, &gNumAdded, gZones[n->zone].name, gNames[n->label].obj);
        }
        gHist[h].misses = 0;
        gHist[h].seen++;
        if (gHist[h].seen > gHist[h].chances) gHist[h].seen = gHist[h].chances;
        gHist[h].last = now;
    }
    for (i = 0; i < gNumHist; i++)               /* in a scanned zone and not seen this time */
        for (z = 0; z < gNumZones; z++)
            if (gZones[z].scanned && PEqualNoCase(gZones[z].name, gHist[i].zone)) {
                /* 1.0.1: one NBP miss is not "gone" (two names flapped every scan on 29 Oct);
                 * a device is reported gone at the second miss in a row, and only once. */
                if (gHist[i].last != now && ++gHist[i].misses == 2)
                    AddChange(gGone, &gNumGone, gHist[i].zone, gHist[i].label);
                break;
            }
    HistLink();
    SaveHist();

    /* the dashboard's line for this scan */
    if (gNumScans == kMaxScans) gNumScans--;
    BlockMoveData(gScans, gScans + 1, gNumScans * sizeof(ScanSum));
    gNumScans++;
    ss = &gScans[0];
    memset(ss, 0, sizeof *ss);
    ss->when = now;
    for (z = 0; z < gNumZones; z++) if (gZones[z].scanned) ss->zones++;
    ss->devices = gNumNodes;
    ss->names = gNumNames;
    ss->added = gNumAdded;
    ss->gone = gNumGone;
    ss->all = gScanAll;
    for (i = 0; i < gNumNodes; i++) ss->kinds[gNodes[i].kind]++;
    SaveScans();
}

short HistRating(Node *n)
{
    Hist *h;
    short r;
    if (n->hist < 0) return 0;
    h = &gHist[n->hist];
    if (h->chances < 3) return 0;               /* "new": too few scans to judge */
    r = (h->seen * 5 + h->chances / 2) / h->chances;
    return r < 1 ? 1 : r > 5 ? 5 : r;
}

/* ---------- icons ---------- */

static void IconName(Node *n, Boolean wholeModel, unsigned char *s)
{
    if (wholeModel && n->model >= 0) { CtoP("M:", s); PCat(s, gNames[n->model].type); }
    else if (wholeModel) { CtoP("K:", s); PCatNum(s, n->kind); }
    else { CtoP("D:", s); PCat(s, gZones[n->zone].name); PCatC(s, ":"); PCat(s, gNames[n->label].obj); }
}

static Handle Custom(ConstStringPtr name)
{
    Handle h;
    short saved = CurResFile();
    if (sPrefsRes == -1) return NULL;
    UseResFile(sPrefsRes);
    h = Get1NamedResource('ICN#', name);
    UseResFile(saved);
    return h;
}

/* This Mac's own icon, the one About This Macintosh shows: Gestalt 'micn' names an ICN# in
 * the System file or the ROM. It comes from the Mac Sherpa runs on, so nothing of Apple's
 * ships with Sherpa (Apple's icons are Apple's, like the ROMs). */
static Handle ThisMacIcon(void)
{
    static Boolean tried;
    static Handle h;
    long id;
    if (tried) return h;
    tried = true;
    if (NGetTrapAddress(_Gestalt, kOSTrapType) == NGetTrapAddress(0xA89F, kToolboxTrapType)) return NULL;
    if (Gestalt('micn', &id) != noErr) return NULL;
    asm volatile("move.w #-1, 0x0B9E");         /* RomMapInsert, TmpResLoad: search the ROM too */
    h = GetResource('ICN#', (short)id);
    if (h && GetHandleSize(h) < 256) h = NULL;
    return h;
}

static Boolean IsThisMac(Node *n)
{
    unsigned short net;
    unsigned char node;
    SplitAddr(gOurAddr, gNetLo, gNetHi, &net, &node);
    return gScanIsLive && n->net == net && n->node == node;
}

/* A kind's icon: one from an icon pack ("K:<kind>"), else Sherpa's own. */
Handle KindIcon(short kind)
{
    unsigned char s[16];
    Handle h;
    CtoP("K:", s); PCatNum(s, kind);
    return (h = Custom(s)) != NULL ? h : GetResource('ICN#', 200 + kind);
}

/* A device's icon, first match wins: its own pasted icon ("D:"), one for its model ("M:"),
 * this Mac's real icon, one for its whole kind ("K:", from an icon pack), Sherpa's own. */
Handle IconFor(Node *n, short *kind)
{
    unsigned char s[80];
    Handle h;
    *kind = n->kind;
    IconName(n, false, s);
    if ((h = Custom(s)) != NULL) return h;
    IconName(n, true, s);
    if ((h = Custom(s)) != NULL) return h;
    /* not a Lisa/XL: a Mac running the XL Name extension should keep looking like an XL */
    if (n->kind != kLisa && n->kind != kRouter && IsThisMac(n) && (h = ThisMacIcon()) != NULL) return h;
    return KindIcon(n->kind);
}

OSErr SaveCustomIcon(Node *n, Boolean wholeModel, Handle icn)
{
    unsigned char s[80];
    Handle old;
    short saved = CurResFile(), id;
    OpenPrefsRes();
    if (sPrefsRes == -1) return fnfErr;
    IconName(n, wholeModel, s);
    UseResFile(sPrefsRes);
    old = Get1NamedResource('ICN#', s);
    if (old) { RemoveResource(old); DisposeHandle(old); }
    do id = Unique1ID('ICN#'); while (id < 1000);
    AddResource(icn, 'ICN#', id, s);
    WriteResource(icn);
    UpdateResFile(sPrefsRes);
    UseResFile(saved);
    return ResError();
}

void RemoveCustomIcon(Node *n)
{
    unsigned char s[80];
    Handle h;
    short saved = CurResFile(), pass;
    if (sPrefsRes == -1) return;
    UseResFile(sPrefsRes);
    for (pass = 0; pass < 2; pass++) {          /* the device's own first, then its model's */
        IconName(n, pass == 1, s);
        if ((h = Get1NamedResource('ICN#', s)) != NULL) { RemoveResource(h); DisposeHandle(h); break; }
    }
    UpdateResFile(sPrefsRes);
    UseResFile(saved);
}

/* Fill the mask: every pixel not reachable from the edge through white pixels. */
static void MakeMask(unsigned char *icon, unsigned char *mask)
{
    static unsigned char out[32][32];
    static short stack[2048];
    short sp = 0, x, y, i;
#define PIX(x, y) ((icon[(y) * 4 + (x) / 8] >> (7 - (x) % 8)) & 1)
    memset(out, 0, sizeof out);
    for (i = 0; i < 32; i++) {
        stack[sp++] = i; stack[sp++] = 0;
        stack[sp++] = i; stack[sp++] = 31;
        stack[sp++] = 0; stack[sp++] = i;
        stack[sp++] = 31; stack[sp++] = i;
    }
    while (sp > 0) {
        y = stack[--sp]; x = stack[--sp];
        if (x < 0 || y < 0 || x > 31 || y > 31 || out[y][x] || PIX(x, y)) continue;
        out[y][x] = 1;
        if (sp < 2040) {
            stack[sp++] = x + 1; stack[sp++] = y;
            stack[sp++] = x - 1; stack[sp++] = y;
            stack[sp++] = x; stack[sp++] = y + 1;
            stack[sp++] = x; stack[sp++] = y - 1;
        }
    }
    memset(mask, 0, 128);
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++)
            if (!out[y][x]) mask[y * 4 + x / 8] |= 0x80 >> (x % 8);
}

/* An ICN# from the clipboard: an ICN# as is, or a picture drawn into 32x32. */
Handle IconFromScrap(void)
{
    Handle h = NewHandle(0), icn;
    long off, len;
    GrafPort port;
    GrafPtr old;
    Rect r, f;
    short i, w, ht;
    Boolean any = false;
    if (!h) return NULL;
    if (GetScrap(h, 'ICN#', &off) >= 256) { SetHandleSize(h, 256); return h; }
    len = GetScrap(h, 'PICT', &off);
    if (len <= 0) { DisposeHandle(h); return NULL; }
    icn = NewHandleClear(256);
    if (!icn) { DisposeHandle(h); return NULL; }
    HLock(icn);
    GetPort(&old);
    OpenPort(&port);
    port.portBits.baseAddr = *icn; port.portBits.rowBytes = 4;
    SetRect(&port.portBits.bounds, 0, 0, 32, 32);
    SetRect(&r, 0, 0, 32, 32);
    port.portRect = r;
    RectRgn(port.visRgn, &r); RectRgn(port.clipRgn, &r);
    EraseRect(&r);
    f = (**(PicHandle)h).picFrame;              /* keep the picture's shape */
    w = f.right - f.left; ht = f.bottom - f.top;
    if (w <= 32 && ht <= 32) OffsetRect(&f, (32 - w) / 2 - f.left, (32 - ht) / 2 - f.top);
    else if (w >= ht) SetRect(&f, 0, (32 - 32L * ht / w) / 2, 32, (32 + 32L * ht / w) / 2);
    else SetRect(&f, (32 - 32L * w / ht) / 2, 0, (32 + 32L * w / ht) / 2, 32);
    DrawPicture((PicHandle)h, &f);
    ClosePort(&port);
    SetPort(old);
    for (i = 0; i < 128; i++) if ((*icn)[i]) any = true;
    MakeMask((unsigned char *)*icn, (unsigned char *)*icn + 128);
    HUnlock(icn);
    DisposeHandle(h);
    if (!any) { DisposeHandle(icn); return NULL; }
    return icn;
}

void CopyIconToScrap(Node *n)
{
    short kind;
    Handle icn = IconFor(n, &kind);
    PicHandle pic;
    BitMap bm;
    Rect r = { 0, 0, 32, 32 };
    GrafPtr port;
    if (!icn) return;
    GetPort(&port);
    HLock(icn);
    ZeroScrap();
    PutScrap(256, 'ICN#', (Ptr)*icn);
    bm.baseAddr = *icn; bm.rowBytes = 4; bm.bounds = r;
    pic = OpenPicture(&r);
    ClipRect(&r);
    CopyBits(&bm, &port->portBits, &r, &r, srcCopy, NULL);
    ClosePicture();
    HUnlock(icn);
    HLock((Handle)pic);
    PutScrap(GetHandleSize((Handle)pic), 'PICT', (Ptr)*pic);
    KillPicture(pic);
    InvalRect(&port->portRect);
}

/* ---------- files ---------- */

static OSErr WriteNamed(ConstStringPtr name, short vref, OSType type, Handle data, long headerZeros)
{
    short ref;
    long len;
    OSErr err;
    static char zeros[512];
    FSDelete(name, vref);
    err = Create(name, vref, 'ttxt', type);
    if (!err) err = FSOpen(name, vref, &ref);
    if (err) return err;
    len = headerZeros;
    if (len) err = FSWrite(ref, &len, zeros);
    len = GetHandleSize(data);
    HLock(data);
    if (!err) err = FSWrite(ref, &len, *data);
    HUnlock(data);
    FSClose(ref);
    FlushVol(NULL, vref);
    return err;
}


static short PictWidth(void)
{
    short w = gMapWin->portRect.right - gMapWin->portRect.left - 15;
    return w < 480 ? 480 : w;
}

OSErr SavePictNamed(ConstStringPtr name, short vref)
{
    PicHandle pic = MapPicture(PictWidth());
    OSErr err;
    if (!pic || GetHandleSize((Handle)pic) <= 10) return memFullErr;
    err = WriteNamed(name, vref, 'PICT', (Handle)pic, 512);
    KillPicture(pic);
    return err;
}

OSErr SaveTextNamed(ConstStringPtr name, short vref)
{
    Handle t = MapText();
    OSErr err;
    if (!t) return memFullErr;
    err = WriteNamed(name, vref, 'TEXT', t, 0);
    DisposeHandle(t);
    return err;
}

static Point Where(void) { Point p; p.h = 80; p.v = 60; return p; }

void SavePictFile(void)
{
    SFReply rep;
    SFPutFile(Where(), "\pSave this map as a picture:", gLevel == 0 ? (ConstStringPtr)"\pGlobalTalk Map" : gLevel == 1 ? (ConstStringPtr)gZones[gCurZone].name : (ConstStringPtr)gNames[gNodes[gCurNode].label].obj, NULL, &rep);
    if (rep.good && SavePictNamed(rep.fName, rep.vRefNum)) Alert1("The picture could not be saved (out of memory or disk space?).");
}

void SaveTextFile(void)
{
    SFReply rep;
    SFPutFile(Where(), "\pSave the scan as a text list:", "\pGlobalTalk Scan", NULL, &rep);
    if (rep.good && SaveTextNamed(rep.fName, rep.vRefNum)) Alert1("The list could not be saved.");
}

Handle MapText(void)
{
    Handle t = NewHandle(0);
    unsigned char s[256];
    short z, i, k;
    if (!t) return NULL;
    HAppendC(t, "GlobalTalk Sherpa scan\r");
    CtoP("Scanned:\t", s); PCatU(s, gScanTime); HAppendP(t, s);
    IUDateString(FixYear(gScanTime), abbrevDate, s); HAppendC(t, "\t"); HAppendP(t, s);
    HAppendC(t, "\r# Lines starting with # are for reading; Zone: and Name: lines are the data.\r");
    for (z = 0; z < gNumZones; z++) {
        if (!gZones[z].scanned) continue;
        HAppendC(t, "\rZone:\t"); HAppendP(t, gZones[z].name); HAppendC(t, "\r");
        if (gScanIsLive) {
            CtoP("# lookup result ", s); PCatNum(s, gZones[z].err); PCatC(s, ", replies "); PCatNum(s, gZones[z].got);
            if (gZones[z].retried) PCatC(s, " (asked twice)");
            if (z == 0) {
                PCatC(s, ", zone list result "); PCatNum(s, gZipErr);
                PCatC(s, ", info result "); PCatNum(s, gInfoErr);
                { unsigned short an; unsigned char ad;
                SplitAddr(gOurAddr, gNetLo, gNetHi, &an, &ad);
                PCatC(s, ", this Mac "); PCatNum(s, an); PCatC(s, "."); PCatNum(s, ad);
                SplitAddr(gRouterAddr, gNetLo, gNetHi, &an, &ad);
                PCatC(s, ", router "); PCatNum(s, an); PCatC(s, "."); PCatNum(s, ad);
                PCatC(s, ", cable "); PCatNum(s, gNetLo); PCatC(s, "-"); PCatNum(s, gNetHi);
                PCatC(s, ", raw "); PCatU(s, gOurAddr); PCatC(s, " "); PCatU(s, gRouterAddr); }
            }
            HAppendP(t, s); HAppendC(t, "\r");
        }
        for (i = 0; i < gNumNodes; i++) {
            Node *n = &gNodes[i];
            if (n->zone != z) continue;
            CtoP("# ", s); PCatNum(s, n->net); PCatC(s, "."); PCatNum(s, n->node); PCatC(s, "  ");
            PCat(s, gNames[n->label].obj);
            if (n->model >= 0) { PCatC(s, "  ("); PCat(s, gNames[n->model].type); PCatC(s, ")"); }
            k = HistRating(n);
            if (k) { PCatC(s, "  reliability "); PCatNum(s, k); PCatC(s, "/5"); }
            HAppendP(t, s); HAppendC(t, "\r");
            for (k = n->first; k < n->first + n->count; k++) {
                Name *nm = &gNames[k];
                CtoP("Name:\t", s); PCatNum(s, nm->net); PCatC(s, "."); PCatNum(s, nm->node);
                PCatC(s, ":"); PCatNum(s, nm->socket); PCatC(s, "\t");
                PCat(s, nm->type); PCatC(s, "\t"); PCat(s, nm->obj);
                HAppendP(t, s); HAppendC(t, "\r");
            }
            if (n->info) { HAppendC(t, "# "); HandAndHand(n->info, t); }
        }
    }
    return t;
}

/* Read a scan list back (Sherpa's own, or one made from the globaltalk-scraper JSON). */
void OpenScanFile(void)
{
    SFReply rep;
    SFTypeList types = { 'TEXT' };
    SFGetFile(Where(), "\p", NULL, 1, types, NULL, &rep);
    if (rep.good) OpenScanNamed(rep.fName, rep.vRefNum);
}

void OpenScanNamed(ConstStringPtr name, short vref)
{
    short ref, z = -1, k;
    long len;
    Handle t;
    char *p, *e, *line;
    if (gScanning) { Alert1("Stop the scan before opening a saved one."); return; }
    if (FSOpen(name, vref, &ref)) { Alert1("The file could not be opened."); return; }
    GetEOF(ref, &len);
    t = NewHandle(len + 1);
    if (!t) { FSClose(ref); Alert1("Not enough memory for that file."); return; }
    HLock(t);
    FSRead(ref, &len, *t);
    FSClose(ref);
    (*t)[len] = '\r';
    gNumZones = 0; gNumNames = 0; gNumNodes = 0; gScanTime = 0;
    for (p = *t, e = *t + len; p < e; p++) {
        line = p;
        while (*p != '\r' && *p != '\n') p++;
        *p = 0;
        if (!strncmp(line, "Scanned:\t", 9)) gScanTime = strtoul(line + 9, NULL, 10);
        else if (!strncmp(line, "Zone:\t", 6) && gNumZones < kMaxZones) {
            ATZone *zz = &gZones[gNumZones];
            memset(zz, 0, sizeof *zz);
            CtoP(line + 6, zz->name);
            if (zz->name[0] > 32) zz->name[0] = 32;
            zz->scanned = true;
            zz->lab = IsLabZone(zz->name);
            z = gNumZones++;
        } else if (!strncmp(line, "Name:\t", 6) && z >= 0 && gNumNames < kMaxNames) {
            Name *nm = &gNames[gNumNames];
            char *q = line + 6, *f[2];
            nm->net = strtoul(q, &q, 10);
            if (*q++ != '.') continue;
            nm->node = strtoul(q, &q, 10);
            if (*q++ != ':') continue;
            nm->socket = strtoul(q, &q, 10);
            if (*q++ != '\t') continue;
            for (k = 0; k < 2; k++) { f[k] = q; while (*q && *q != '\t') q++; if (*q) *q++ = 0; }
            CtoP(f[0], nm->type); CtoP(f[1], nm->obj);
            if (nm->type[0] > 32) nm->type[0] = 32;
            if (nm->obj[0] > 32) nm->obj[0] = 32;
            nm->zone = z;
            gNumNames++;
        }
    }
    DisposeHandle(t);
    gScanIsLive = false;
    gLevel = 0;
    BuildNodes();
    MapChanged();
    SetStatus(gNumZones ? "Showing a saved scan. Scan again for live results." : "That file had no Sherpa scan in it.");
}

void ExportIconPack(void)
{
    SFReply rep;
    short pack, i, n, id, saved = CurResFile(), old, copied = 0;
    Handle h, c;
    ResType rt;
    Str255 name;
    if (sPrefsRes == -1) return;
    UseResFile(sPrefsRes);
    n = Count1Resources('ICN#');
    UseResFile(saved);
    if (n == 0) { Alert1("There are no custom icons yet. Paste one onto a device first."); return; }
    SFPutFile(Where(), "\pSave your custom icons as an icon pack:", "\pSherpa Icon Pack", NULL, &rep);
    if (!rep.good) return;
    FSDelete(rep.fName, rep.vRefNum);
    Create(rep.fName, rep.vRefNum, 'GTSh', 'GTSi');
    GetVol(NULL, &old); SetVol(NULL, rep.vRefNum);
    CreateResFile(rep.fName);
    SetVol(NULL, old);
    pack = OpenRFPerm(rep.fName, rep.vRefNum, fsRdWrPerm);
    if (pack == -1) { Alert1("The icon pack could not be created."); return; }
    for (i = 1; i <= n; i++) {
        UseResFile(sPrefsRes);
        h = Get1IndResource('ICN#', i);
        if (!h) continue;
        GetResInfo(h, &id, &rt, name);
        if (name[0] > 2 && name[1] == 'D') continue;     /* one device's own icon stays private */
        c = h; HandToHand(&c);
        UseResFile(pack);
        AddResource(c, 'ICN#', id, name);
        WriteResource(c);
        copied++;
    }
    CloseResFile(pack);
    UseResFile(saved);
    if (!copied) Alert1("Only single-device icons exist; icon packs carry model icons.");
}

void ImportIconPack(void)
{
    SFReply rep;
    SFTypeList types = { 'GTSi' };
    short pack, i, n, id, saved = CurResFile();
    Handle h, c, old;
    ResType rt;
    Str255 name;
    SFGetFile(Where(), "\p", NULL, 1, types, NULL, &rep);
    if (!rep.good) return;
    OpenPrefsRes();
    pack = OpenRFPerm(rep.fName, rep.vRefNum, fsRdPerm);
    if (pack == -1 || sPrefsRes == -1) { Alert1("The icon pack could not be opened."); return; }
    UseResFile(pack);
    n = Count1Resources('ICN#');
    for (i = 1; i <= n; i++) {
        UseResFile(pack);
        h = Get1IndResource('ICN#', i);
        if (!h || GetHandleSize(h) != 256) continue;
        GetResInfo(h, &id, &rt, name);
        c = h; HandToHand(&c);
        UseResFile(sPrefsRes);
        old = Get1NamedResource('ICN#', name);
        if (old) { RemoveResource(old); DisposeHandle(old); }
        do id = Unique1ID('ICN#'); while (id < 1000);
        AddResource(c, 'ICN#', id, name);
        WriteResource(c);
    }
    UpdateResFile(sPrefsRes);
    CloseResFile(pack);
    UseResFile(saved);
    MapChanged();
}
