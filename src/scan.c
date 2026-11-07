/* scan.c: find the zones and the NBP names in them, one zone at a time, and group the names
 * into devices. One zone at a time on purpose: on 25 Oct 2026 parallel lookups dropped
 * replies from the lab's LocalTalk zone, and one at a time never did. */
#include "sherpa.h"
#include <Multiverse.h>
#include "appletalk.h"
#include <stdlib.h>
#include <OSUtils.h>

ATZone gZones[kMaxZones];
short gNumZones;
Name gNames[kMaxNames];
short gNumNames;
Node gNodes[kMaxNodes];
short gNumNodes;
unsigned long gScanTime;
Boolean gScanning, gScanIsLive;
Boolean gScanAll;                   /* the last live scan was Scan All (for the dashboard) */

static short sMPP, sXPP;
static Boolean sOpen;
static NBPLookupPB sPB;
static Ptr sReply;
#define kReplySize 14000
static unsigned char sEntity[110];
static short sQueue[kMaxZones], sQLen, sQPos;
static Boolean sStop;
short gZipErr;
long gOurAddr, gRouterAddr;
short gInfoErr;
unsigned short gNetLo, gNetHi;

/* GetATalkInfo's ourAdd and routerAddr.  Sherpa read them as an AddrBlock (net, node, socket), but real Macs
 * gave 0, net, node: the Performa on net 38 showed its router 38.1 as "0.38", the XL on net 37 itself as "0.37"
 * (4 Nov 2026).  Use the layout whose network lies in this cable's range, netLo..netHi, from the same call. */
void SplitAddr(long a, unsigned short lo, unsigned short hi, unsigned short *net, unsigned char *node)
{
    unsigned long u = (unsigned long)a;
    unsigned short n1 = u >> 16, n2 = (u >> 8) & 0xFFFF;
    if (!(n1 && n1 >= lo && n1 <= hi) && (u >> 24) == 0 && n2 && n2 >= lo && n2 <= hi) {
        *net = n2; *node = u & 0xFF;
    } else {
        *net = n1; *node = (u >> 8) & 0xFF;
    }
}

static void GetInfo(void)
{
    static ATInfoPB pb;
    memset(&pb, 0, sizeof pb);
    pb.ioRefNum = sMPP;
    pb.csCode = GetATalkInfo;
    pb.version = 1;
    gInfoErr = PBControlSync((ParmBlkPtr)&pb);
    gOurAddr = pb.ourAdd;
    gRouterAddr = pb.routerAddr;
    gNetLo = pb.netLo; gNetHi = pb.netHi;
}

OSErr ATOpen(void)
{
    OSErr err;
    if (sOpen) return noErr;
    err = OpenDriver("\p.MPP", &sMPP);
    if (err) return err;
    err = OpenDriver("\p.XPP", &sXPP);
    if (err) return err;
    sOpen = true;
    return noErr;
}

Boolean ATAvailable(void) { return ATOpen() == noErr; }

static OSErr ZipCall(short sub, Ptr buf, XCallPB *pb, Boolean first)
{
    if (first) memset(pb, 0, sizeof *pb);
    pb->ioRefNum = sXPP;
    pb->csCode = xCall;
    pb->xppSubCode = sub;
    pb->xppTimeout = 3;
    pb->xppRetry = 4;
    pb->zipBuffPtr = buf;
    return PBControlSync((ParmBlkPtr)pb);
}

/* Unpack zipNumZones packed Pascal strings from buf. */
static void AddZones(Ptr buf, short n, Str32a *out, short max, short *count)
{
    unsigned char *p = (unsigned char *)buf;
    short i;
    for (i = 0; i < n && *count < max; i++) {
        PCopy(out[*count], p, 32);
        (*count)++;
        p += p[0] + 1;
    }
}

static OSErr GetZoneNames(short sub, Str32a *out, short max, short *n)
{
    static XCallPB pb;
    Ptr buf = NewPtr(578);
    OSErr err;
    Boolean first = true;
    *n = 0;
    if (!buf) return memFullErr;
    do {
        err = ZipCall(sub, buf, &pb, first);
        first = false;
        if (err) break;
        AddZones(buf, pb.zipNumZones, out, max, n);
    } while (!pb.zipLastFlag && *n < max);
    DisposePtr(buf);
    return err;
}

OSErr GetLocalZoneNames(Str32a *out, short max, short *n)
{
    OSErr err = ATOpen();
    if (err) return err;
    return GetZoneNames(zipGetLocalZones, out, max, n);
}

static void GetMyZoneName(void)
{
    static XCallPB pb;
    Ptr buf = NewPtr(578);
    PCopy(gMyZone, "\p*", 32);
    if (!buf) return;
    if (ZipCall(zipGetMyZone, buf, &pb, true) == noErr && pb.zipNumZones > 0)
        PCopy(gMyZone, (unsigned char *)buf, 32);
    DisposePtr(buf);
}

/* The local network, found at each scan when no lab zones are set (FindLocalNetwork, local.c):
 * the zones of the networks plugged straight into the router, or failing that the zones on
 * this Mac's own cable. Typed-in lab zones (Scan > Local Network Zones) override it. */
static Str32a sLocal[kMaxLab];
static short sNumLocal;

Boolean IsLabZone(ConstStringPtr z)
{
    short i;
    if (gPrefs.labCount == 0) {
        if (PEqualNoCase(z, gMyZone) || (z[0] == 1 && z[1] == '*')) return true;
        for (i = 0; i < sNumLocal; i++)
            if (PEqualNoCase(z, sLocal[i])) return true;
        return false;
    }
    for (i = 0; i < gPrefs.labCount; i++)
        if (PEqualNoCase(z, gLab[i])) return true;
    return false;
}

static void StartLookup(short zi, Boolean slow)
{
    unsigned char *e = sEntity;
    /* Our own zone is looked up as "*", the way the Chooser does: by name, the Performa 475
     * didn't get its own names back (first live test, 26 Oct 2026). */
    ConstStringPtr zone = PEqualNoCase(gZones[zi].name, gMyZone) ? (ConstStringPtr)"\p*" : (ConstStringPtr)gZones[zi].name;
    *e++ = 1; *e++ = '=';
    *e++ = 1; *e++ = '=';
    BlockMoveData(zone, e, zone[0] + 1);
    memset(&sPB, 0, sizeof sPB);
    sPB.ioRefNum = sMPP;
    sPB.csCode = lookupName;
    sPB.interval = slow ? gPrefs.nbpInterval * 2 : gPrefs.nbpInterval;   /* 1.0.1: the retry waits twice as long */
    sPB.count = slow ? gPrefs.nbpCount * 2 : gPrefs.nbpCount;
    sPB.entityPtr = (Ptr)sEntity;
    sPB.retBuffPtr = sReply;
    sPB.retBuffSize = kReplySize;
    sPB.maxToGet = kReplySize / 40;
    PBControlAsync((ParmBlkPtr)&sPB);
}

static void TakeReplies(short zi)
{
    unsigned char *p = (unsigned char *)sReply;
    short i, got = 0;
    for (i = 0; i < sPB.numGotten && gNumNames < kMaxNames; i++) {
        Name *nm = &gNames[gNumNames];
        nm->net = (p[0] << 8) | p[1];
        nm->node = p[2];
        nm->socket = p[3];
        p += 5;                                  /* address + enumerator */
        PCopy(nm->obj, p, 32);  p += p[0] + 1;
        PCopy(nm->type, p, 32); p += p[0] + 1;
        p += p[0] + 1;                           /* zone comes back as "*" */
        nm->zone = zi;
        gNumNames++; got++;
    }
    gZones[zi].names = got;
}

static void AddName(short zi, unsigned short net, unsigned char node, unsigned char socket,
                    ConstStringPtr obj, ConstStringPtr type)
{
    Name *nm = &gNames[gNumNames];
    if (gNumNames >= kMaxNames) return;
    nm->net = net; nm->node = node; nm->socket = socket; nm->zone = zi;
    PCopy(nm->obj, obj, 32);
    PCopy(nm->type, type, 32);
    gNumNames++;
    gZones[zi].names++;
}

/* If NBP still didn't return this Mac's own names, add it from what it knows locally:
 * its Sharing Setup name and (System 7) its model from Gestalt. */
static void AddThisMac(short zi)
{
    unsigned short net;
    unsigned char node;
    StringHandle sh;
    Str32a obj;
    long resp;
    short i;
    SplitAddr(gOurAddr, gNetLo, gNetHi, &net, &node);
    if (node == 0) return;
    for (i = 0; i < gNumNames; i++)
        if (gNames[i].net == net && gNames[i].node == node) return;
    sh = GetString(-16413);                      /* Sharing Setup's computer name */
    if (sh && (*sh)[0]) PCopy(obj, *sh, 32); else PCopy(obj, "\pThis Mac", 32);
    AddName(zi, net, node, 4, obj, "\pWorkstation");
    if (NGetTrapAddress(_Gestalt, kOSTrapType) == NGetTrapAddress(0xA89F, kToolboxTrapType)) return;
    if (Gestalt('mnam', &resp) == noErr && resp && ((StringPtr)resp)[0])  /* 7.5 and later */
        AddName(zi, net, node, 253, obj, (ConstStringPtr)resp);
    else if (Gestalt('mach', &resp) == noErr) {   /* earlier: the System's own list of models */
        Str255 model;
        GetIndString(model, -16395, resp);
        if (model[0]) AddName(zi, net, node, 253, obj, model);
    }
}

void ScanStart(Boolean wholeNet)
{
    static Str32a zl[kMaxZones];
    short n = 0, i;
    OSErr err;
    unsigned char msg[64];

    if (gScanning) return;
    err = ATOpen();
    if (err) { Alert1("AppleTalk is not available. Turn it on in the Chooser (or Network control panel) and try again."); return; }
    if (!sReply) sReply = NewPtr(kReplySize);
    if (!sReply) { Alert1("Not enough memory to scan."); return; }

    SetStatus("Asking the router for zones\311");
    GetInfo();
    GetMyZoneName();
    sNumLocal = gPrefs.labCount ? 0 : FindLocalNetwork(sLocal, kMaxLab);
    /* 1.0.1: GetZoneList came back short now and then (43 to 49 zones from one scan to the next
     * on the Performa, 29 Oct), so the list is asked for three times and the union kept. */
    err = GetZoneNames(zipGetZoneList, zl, kMaxZones, &n);
    gZipErr = err;
    {
        static Str32a more[kMaxZones];
        short t, m, j, k;
        for (t = 0; t < 2; t++) {
            if (GetZoneNames(zipGetZoneList, more, kMaxZones, &m) != noErr) continue;
            if (err) { err = gZipErr = noErr; }
            for (j = 0; j < m && n < kMaxZones; j++) {
                for (k = 0; k < n && !PEqualNoCase(zl[k], more[j]); k++) ;
                if (k == n) PCopy(zl[n++], more[j], 32);
            }
        }
        for (j = 1; j < n; j++) {                   /* a router can list a zone twice: keep one */
            for (k = 0; k < j && !PEqualNoCase(zl[k], zl[j]); k++) ;
            if (k < j) { for (m = j + 1; m < n; m++) PCopy(zl[m - 1], zl[m], 32); n--; j--; }
        }
    }
    if (err || n == 0) {                         /* no router: just this network */
        n = 1;
        PCopy(zl[0], "\p*", 32);
    }
    /* Scan Local Network shows only its zones; Scan All shows every zone (Tim, 26 Oct) */
    gNumZones = 0;
    for (i = 0; i < n; i++) {
        ATZone *z = &gZones[gNumZones];
        if (!wholeNet && !IsLabZone(zl[i])) continue;
        memset(z, 0, sizeof *z);
        PCopy(z->name, zl[i], 32);
        z->lab = IsLabZone(z->name);
        gNumZones++;
    }
    /* home zones first, then alphabetical-ish as the router gave them */
    sQLen = 0;
    for (i = 0; i < gNumZones; i++) if (gZones[i].lab) sQueue[sQLen++] = i;
    if (wholeNet) for (i = 0; i < gNumZones; i++) if (!gZones[i].lab) sQueue[sQLen++] = i;
    if (sQLen == 0) { Alert1("None of the local network's zones were found. Check Scan > Local Network Zones."); return; }

    gNumNames = 0;
    gNumNodes = 0;
    sQPos = 0;
    sStop = false;
    gScanning = true;
    gScanIsLive = true;
    gScanAll = wholeNet;
    gScanTime = NowFixed();
    CtoP("Scanning ", msg); PCat(msg, gZones[sQueue[0]].name);
    SetStatusP(msg);
    StartLookup(sQueue[0], false);
}

void ScanStop(void) { sStop = true; }

/* Before quitting: the driver still owns sPB until the lookup ends. */
void ScanQuit(void)
{
    sStop = true;
    while (gScanning && sPB.ioResult > 0) Yield();   /* not SystemTask: that starves other programs */
}

void ScanIdle(void)
{
    unsigned char msg[80];
    short zi;
    if (!gScanning || sPB.ioResult > 0) return;  /* 1 = still looking */
    zi = sQueue[sQPos];
    if (sPB.ioResult == noErr && sPB.numGotten == 0 && !gZones[zi].retried && !sStop) {
        /* 1.0.1: nothing answered. A zone behind a router over LocalTalk (Lisa_Net_LT on 29 Oct) can be
         * slower than the first lookup's wait, so ask once more, slower, before calling it empty. */
        gZones[zi].retried = true;
        StartLookup(zi, true);
        return;
    }
    gZones[zi].scanned = true;
    gZones[zi].err = sPB.ioResult;
    gZones[zi].got = sPB.numGotten;
    if (sPB.ioResult == noErr || sPB.ioResult == reqFailed) TakeReplies(zi);
    if (PEqualNoCase(gZones[zi].name, gMyZone)) AddThisMac(zi);
    sQPos++;
    if (sQPos < sQLen && !sStop) {
        CtoP("Scanning ", msg); PCat(msg, gZones[sQueue[sQPos]].name);
        PCatC(msg, "  ("); PCatNum(msg, sQPos + 1); PCatC(msg, " of "); PCatNum(msg, sQLen); PCatC(msg, ")");
        SetStatusP(msg);
        StartLookup(sQueue[sQPos], false);
        if ((sQPos & 3) == 0) { BuildNodes(); MapChanged(); }  /* show progress on the map */
        return;
    }
    gScanning = false;
    gScanTime = NowFixed();                      /* 1.0.3: the time the scan finished (status line, saved file) */
    BuildNodes();
    HistRecordScan();
    CtoP(sStop ? "Scan stopped: " : "Scan done: ", msg);
    PCatNum(msg, gNumNodes); PCatC(msg, " devices, ");
    PCatNum(msg, gNumNames); PCatC(msg, " names in ");
    PCatNum(msg, sQPos); PCatC(msg, sQPos == 1 ? " zone" : " zones");
    SetStatusP(msg);
    MapChanged();
}

/* ---- grouping names into devices ---- */

static int CmpName(const void *a, const void *b)
{
    const Name *x = a, *y = b;
    if (x->zone != y->zone) return x->zone - y->zone;
    if (x->net != y->net) return x->net < y->net ? -1 : 1;
    if (x->node != y->node) return x->node - y->node;
    return x->socket - y->socket;
}

static Boolean TypeIs(ConstStringPtr t, const char *s) { return PContains(t, s); }

/* Which icon family a model name belongs to. Order matters: "Color Classic" before "Classic",
 * "LC 5xx" (all-in-ones) before "LC", "IIgs" before "Macintosh II". */
static short ModelKind(ConstStringPtr m)
{
    static const struct { const char *s; short kind; } map[] = {
        { "Lisa", kLisa }, { "Macintosh XL", kLisa },
        { "PowerBook", kPortable }, { "Portable", kPortable },
        { "IIgs", kAppleII }, { "//e", kAppleII }, { "Apple II", kAppleII },
        { "Color Classic", kAllInOne }, { "LC 5", kAllInOne }, { "Performa 5", kAllInOne },
        { "Macintosh TV", kAllInOne }, { "Quadra 630", kPizza },
        { "Plus", kCompact }, { "Classic", kCompact }, { "128K", kCompact }, { "512K", kCompact },
        { "Mac SE", kCompact }, { "Macintosh SE", kCompact },
        { "Power Mac", kPowerMac },
        { "Quadra 800", kTower }, { "Quadra 840", kTower }, { "Quadra 900", kTower },
        { "Quadra 950", kTower }, { "Workgroup Server", kTower },
        { "Quadra 605", kPizza }, { "Quadra 610", kPizza }, { "Centris 610", kPizza },
        { "Quadra 660", kPizza }, { "Centris 660", kPizza }, { "Performa 4", kPizza },
        { "Performa 2", kPizza }, { "LC", kPizza },
        { "Macintosh II", kMacII }, { "Mac II", kMacII }, { "Quadra 700", kMacII },
        { "Quadra 650", kMacII }, { "Centris 650", kMacII }, { "Performa 6", kMacII },
        { 0, 0 }
    };
    short i;
    if (m[0] == 2 && m[1] == 'X' && m[2] == 'L') return kLisa;
    if (m[0] == 2 && m[1] == 'S' && m[2] == 'E') return kCompact;
    for (i = 0; map[i].s; i++) if (TypeIs(m, map[i].s)) return map[i].kind;
    return kDesktop;
}

/* A Mac announces its model as an NBP type, usually on socket 253 but on GlobalTalk also
 * 251 and 252 (scrape of 25 Oct 2026), so it's recognised by the name, not the socket. */
static Boolean IsModelType(ConstStringPtr t)
{
    static const char *pre[] = { "Macintosh", "Mac ", "Power Mac", "PowerBook", "Performa", "Quadra",
                                 "Centris", "LC", "Lisa", "iMac", "Workgroup Server", "Apple IIgs", 0 };
    short i, n, sp = 0;
    if (TypeIs(t, "Router")) return false;
    while (sp < t[0] && t[1 + sp] == ' ') sp++;  /* "  Macintosh" (Emulated Mac, 38.207) */
    for (i = 0; pre[i]; i++) {
        n = strlen(pre[i]);
        if (t[0] - sp >= n && !memcmp(t + 1 + sp, pre[i], n)) return true;
    }
    return false;
}

static Boolean IsPrinterType(ConstStringPtr t)
{
    return TypeIs(t, "LaserWriter") || TypeIs(t, "ImageWriter") || TypeIs(t, "StyleWriter") ||
           TypeIs(t, "Printer") || TypeIs(t, "DeskWriter") || TypeIs(t, "LaserShare") || TypeIs(t, "Writer");
}

void BuildNodes(void)
{
    short i, j;
    for (i = 0; i < gNumNodes; i++) {           /* keep AFP info only for a fresh rebuild */
        if (gNodes[i].info) DisposeHandle(gNodes[i].info);
        if (gNodes[i].srvIcon) DisposeHandle(gNodes[i].srvIcon);
    }
    qsort(gNames, gNumNames, sizeof(Name), CmpName);
    gNumNodes = 0;
    for (i = 0; i < gNumZones; i++) gZones[i].nodes = 0;
    for (i = 0; i < gNumNames && gNumNodes < kMaxNodes; i = j) {
        Node *n = &gNodes[gNumNodes++];
        Boolean onlyPrinters = true, netboot = false;
        memset(n, 0, sizeof *n);
        n->net = gNames[i].net; n->node = gNames[i].node; n->zone = gNames[i].zone;
        n->first = i; n->label = i; n->model = -1; n->hist = -1;
        for (j = i; j < gNumNames && gNames[j].zone == n->zone && gNames[j].net == n->net && gNames[j].node == n->node; j++) {
            Name *nm = &gNames[j];
            if (IsModelType(nm->type) && (n->model < 0 || nm->socket == 253)) n->model = j;
            if (TypeIs(nm->type, "Workstation")) n->label = j;
            else if (TypeIs(nm->type, "AFPServer")) { n->isServer = true; if (!TypeIs(gNames[n->label].type, "Workstation")) n->label = j; }
            if (TypeIs(nm->type, "Router")) n->isRouter = true;
            if (IsPrinterType(nm->type)) n->hasPrinter = true; else if (!IsModelType(nm->type)) onlyPrinters = false;
            if (TypeIs(nm->type, "ImageWriter")) n->dotMatrix = true;   /* the one dot-matrix family on AppleTalk */
            if (TypeIs(nm->type, "ProDOS") || TypeIs(nm->type, "Boot") || TypeIs(nm->type, "netatalk")) netboot = true;
        }
        n->count = j - i;
        /* a Mac that also routes keeps its model's icon (with a router badge): Tim, 26 Oct */
        if (n->model >= 0) n->kind = ModelKind(gNames[n->model].type);
        else if (n->isRouter) n->kind = kRouter;
        else if (n->hasPrinter && onlyPrinters) n->kind = n->dotMatrix ? kDotMatrix : kPrinter;
        else if (netboot) n->kind = kUnix;
        else if (n->isServer) n->kind = kServer;
        else n->kind = kUnknown;
        gZones[n->zone].nodes++;
    }
    HistLink();
}
