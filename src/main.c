/* main.c: GlobalTalk Sherpa's menus, window and event loop.
 * Plain System 6 toolbox calls throughout (no Gestalt-only features required), so the
 * same 68000 binary runs on a Plus, an SE, a Mac XL under MacWorks and System 7.6. */
#include "sherpa.h"
#include <Multiverse.h>
#include <Dialogs.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <Fonts.h>
#include <OSUtils.h>

WindowPtr gMapWin;
Prefs gPrefs;
Str32a gLab[kMaxLab];
Str32a gMyZone = "\p*";

enum { mApple = 128, mFile, mEdit, mScan, mMap };
enum { iAbout = 1 };
enum { iOpen = 1, iSavePict = 3, iSaveText, iExport = 6, iImport, iQuit = 9 };
enum { iCopyIcon = 8, iPasteDevice, iPasteModel, iRemoveIcon };
enum { iScanSite = 1, iScanAll, iStop, iLabZones = 5, iRescanOff = 7, iRescan15, iRescan30, iRescan60 };
enum { iAllZones = 1, iDiagram, iDashboard, iBack = 5 };

static ControlHandle sScroll;
static short sScrollPos, sLastLevel = -1;
static Boolean sQuit, sHasWNE, sLastWasAll;
static unsigned long sNextRescan;
static unsigned char sStatus[256];

static Boolean TrapAvailable(short trap)
{
    return NGetTrapAddress(trap, kToolboxTrapType) != NGetTrapAddress(0xA89F, kToolboxTrapType);
}

static void AreaRect(Rect *r)
{
    *r = gMapWin->portRect;
    r->right -= 15;
    r->bottom -= 15;
}

static void StatusRect(Rect *r)
{
    *r = gMapWin->portRect;
    r->top = r->bottom - 14;
    r->right -= 16;
}

static void DrawStatus(void)
{
    Rect r;
    GrafPtr old;
    GetPort(&old);
    SetPort(gMapWin);
    StatusRect(&r);
    EraseRect(&r);
    TextFont(kFontIDGeneva); TextSize(9); TextFace(0);
    MoveTo(r.left + 6, r.bottom - 4);
    DrawString(sStatus);
    if (gScanTime && !gScanning) {               /* 1.0.3 (Tim, 5 Nov): when the scan shown was made, at the right */
        Str255 when, t;
        short w;
        CtoP(gScanIsLive ? "Last scan " : "Saved scan from ", when);
        IUDateString(FixYear(gScanTime), abbrevDate, t); PCat(when, t); PCatC(when, ", ");
        IUTimeString(FixYear(gScanTime), false, t); PCat(when, t);
        w = StringWidth(when);
        if (r.left + 6 + StringWidth(sStatus) + 16 + w <= r.right - 6) {   /* only when it fits beside the message */
            MoveTo(r.right - 6 - w, r.bottom - 4);
            DrawString(when);
        }
    }
    SetPort(old);
}

void SetStatusP(ConstStringPtr p) { PCopy(sStatus, p, 255); DrawStatus(); }
void SetStatus(const char *msg) { CtoP(msg, sStatus); DrawStatus(); }

void Alert1(const char *msg)
{
    unsigned char p[256];
    CtoP(msg, p);
    ParamText(p, "\p", "\p", "\p");
    InitCursor();
    Alert(130, NULL);
}

static void FixScroll(void)
{
    Rect a;
    short h, max;
    AreaRect(&a);
    h = MapHeight(a.right - a.left);
    max = h - (a.bottom - a.top) + 20;
    if (max < 0) max = 0;
    if (sScrollPos > max) sScrollPos = max;
    SetControlMaximum(sScroll, max);
    SetControlValue(sScroll, sScrollPos);
}

void MapChanged(void)
{
    Rect a;
    if (gLevel != sLastLevel) { sScrollPos = 0; sLastLevel = gLevel; }
    SetPort(gMapWin);
    FixScroll();
    AreaRect(&a);
    InvalRect(&a);
}

static void ScrollTo(short v)
{
    Rect a;
    short max = GetControlMaximum(sScroll);
    if (v < 0) v = 0;
    if (v > max) v = max;
    if (v == sScrollPos) return;
    sScrollPos = v;
    SetControlValue(sScroll, v);
    AreaRect(&a);
    MapDraw(&a, sScrollPos);
}

static pascal void ScrollAction(ControlHandle c, short part)
{
    Rect a;
    short page;
    AreaRect(&a);
    page = a.bottom - a.top - 40;
    switch (part) {
    case inUpButton:   ScrollTo(sScrollPos - 16); break;
    case inDownButton: ScrollTo(sScrollPos + 16); break;
    case inPageUp:     ScrollTo(sScrollPos - page); break;
    case inPageDown:   ScrollTo(sScrollPos + page); break;
    }
    (void)c;
}

static void PlaceScroll(void)
{
    Rect r = gMapWin->portRect;
    HideControl(sScroll);
    MoveControl(sScroll, r.right - 15, r.top - 1);
    SizeControl(sScroll, 16, r.bottom - r.top - 13);
    ShowControl(sScroll);
}

static void MakeWindow(void)
{
    Rect r = qd.screenBits.bounds, s;
    r.top += 40; r.left += 4; r.right -= 4; r.bottom -= 4;
    if (r.right - r.left > 760) r.right = r.left + 760;
    if (r.bottom - r.top > 560) r.bottom = r.top + 560;
    gMapWin = NewWindow(NULL, &r, "\pGlobalTalk Sherpa", true, documentProc, (WindowPtr)-1, false, 0);
    SetPort(gMapWin);
    SetRect(&s, 0, 0, 16, 100);
    sScroll = NewControl(gMapWin, &s, "\p", true, 0, 0, 0, scrollBarProc, 0);
    PlaceScroll();
    SetStatus(ATAvailable() ? "Ready. Scan > Scan Local Network looks at your side of the router; Scan All of GlobalTalk looks everywhere."
                         : "AppleTalk is off: you can still open a saved scan (File > Open Scan).");
}

/* ---- Local Network Zones dialog (lab zones in the code) ---- */
static void LabZonesDialog(void)
{
    DialogPtr d = GetNewDialog(129, NULL, (WindowPtr)-1);
    short item, type, i;
    Handle h;
    Rect r;
    unsigned char s[256], one[40];
    if (!d) return;
    CtoP("", s);
    for (i = 0; i < gPrefs.labCount; i++) { if (i) PCatC(s, ", "); PCat(s, gLab[i]); }
    GetDialogItem(d, 4, &type, &h, &r);
    SetDialogItemText(h, s);
    SelectDialogItemText(d, 4, 0, 255);
    ShowWindow(d);
    for (;;) {
        ModalDialog(NULL, &item);
        if (item == 1 || item == 2) break;
        if (item == 3) {                         /* Ask the Router */
            static Str32a lz[kMaxLab];
            short n = FindLocalNetwork(lz, kMaxLab), t2;
            Handle h2;
            Rect r2;
            GetDialogItem(d, 5, &t2, &h2, &r2);
            SetDialogItemText(h2, gRouterNote);
            if (n > 0) {
                CtoP("", s);
                for (i = 0; i < n; i++) { if (i) PCatC(s, ", "); PCat(s, lz[i]); }
                SetDialogItemText(h, s);
            } else SysBeep(10);
        }
    }
    if (item == 1) {
        short k = 0;
        GetDialogItemText(h, s);
        gPrefs.labCount = 0;
        for (i = 1; i <= s[0] + 1 && gPrefs.labCount < kMaxLab; i++) {
            if (i > s[0] || s[i] == ',') {
                while (k > 0 && one[k] == ' ') k--;
                one[0] = k > 32 ? 32 : k;
                if (k) PCopy(gLab[gPrefs.labCount++], one, 32);
                k = 0;
            } else if (k > 0 || s[i] != ' ') { if (k < 38) one[++k] = s[i]; }
        }
        PrefsSave();
        for (i = 0; i < gNumZones; i++) gZones[i].lab = IsLabZone(gZones[i].name);
        MapChanged();
    }
    DisposeDialog(d);
}

/* ---- menus ---- */
static void SetupMenus(void)
{
    Handle bar = GetNewMBar(128);
    SetMenuBar(bar);
    AppendResMenu(GetMenuHandle(mApple), 'DRVR');
    DrawMenuBar();
}

static void AdjustMenus(void)
{
    MenuHandle f = GetMenuHandle(mFile), e = GetMenuHandle(mEdit), s = GetMenuHandle(mScan), m = GetMenuHandle(mMap);
    Boolean da = FrontWindow() != gMapWin, node = gLevel == 2 && !da;
    short i;
    #define EN(menu, item, on) do { if (on) EnableItem(menu, item); else DisableItem(menu, item); } while (0)
    EN(f, iSavePict, !da && gNumZones > 0);
    EN(f, iSaveText, gNumZones > 0);
    for (i = 1; i <= 6; i++) EN(e, i, da);
    EN(e, iCopyIcon, node);
    EN(e, iPasteDevice, node);
    EN(e, iPasteModel, node);
    EN(e, iRemoveIcon, node);
    EN(s, iScanSite, !gScanning);
    EN(s, iScanAll, !gScanning);
    EN(s, iStop, gScanning);
    CheckItem(s, iRescanOff, gPrefs.rescanMins == 0);
    CheckItem(s, iRescan15, gPrefs.rescanMins == 15);
    CheckItem(s, iRescan30, gPrefs.rescanMins == 30);
    CheckItem(s, iRescan60, gPrefs.rescanMins == 60);
    EN(m, iBack, gLevel > 0);
    CheckItem(m, iAllZones, gPrefs.view == kViewZones);
    CheckItem(m, iDiagram, gPrefs.view == kViewDiagram);
    CheckItem(m, iDashboard, gPrefs.view == kViewDashboard);
}

static void SetRescan(short mins)
{
    gPrefs.rescanMins = mins;
    sNextRescan = TickCount() + mins * 3600L;
    PrefsSave();
}

static void StartScan(Boolean all)
{
    sLastWasAll = all;
    ScanStart(all);
    if (gPrefs.rescanMins) sNextRescan = TickCount() + gPrefs.rescanMins * 3600L;
    MapChanged();
}

static void PasteIcon(Boolean model)
{
    Handle icn = IconFromScrap();
    if (!icn) { Alert1("Copy a picture or icon first (32 by 32 pixels looks best), then choose Paste Icon."); return; }
    SaveCustomIcon(&gNodes[gCurNode], model, icn);
    MapChanged();
}

static void DoMenu(long choice)
{
    short menu = HiWord(choice), item = LoWord(choice);
    Str255 name;
    switch (menu) {
    case mApple:
        if (item == iAbout) Alert(128, NULL);
        else { GetMenuItemText(GetMenuHandle(mApple), item, name); OpenDeskAcc(name); }
        break;
    case mFile:
        switch (item) {
        case iOpen: OpenScanFile(); break;
        case iSavePict: SavePictFile(); break;
        case iSaveText: SaveTextFile(); break;
        case iExport: ExportIconPack(); break;
        case iImport: ImportIconPack(); break;
        case iQuit: sQuit = true; break;
        }
        break;
    case mEdit:
        if (item <= 6) { SystemEdit(item - 1); break; }
        if (gLevel != 2) break;
        switch (item) {
        case iCopyIcon: CopyIconToScrap(&gNodes[gCurNode]); break;
        case iPasteDevice: PasteIcon(false); break;
        case iPasteModel: PasteIcon(true); break;
        case iRemoveIcon: RemoveCustomIcon(&gNodes[gCurNode]); MapChanged(); break;
        }
        break;
    case mScan:
        switch (item) {
        case iScanSite: StartScan(false); break;
        case iScanAll: StartScan(true); break;
        case iStop: ScanStop(); break;
        case iLabZones: LabZonesDialog(); break;
        case iRescanOff: SetRescan(0); break;
        case iRescan15: SetRescan(15); break;
        case iRescan30: SetRescan(30); break;
        case iRescan60: SetRescan(60); break;
        }
        break;
    case mMap:
        if (item == iAllZones || item == iDiagram || item == iDashboard) {
            gPrefs.view = item - iAllZones;
            PrefsSave();
            gLevel = 0;
            sLastLevel = -1;                     /* back to the top of the view */
            MapChanged();
        }
        else if (item == iBack) MapZoomOut();
        break;
    }
    HiliteMenu(0);
}

/* ---- events ---- */
static void DoUpdate(WindowPtr w)
{
    Rect a;
    SetPort(w);
    BeginUpdate(w);
    AreaRect(&a);
    MapDraw(&a, sScrollPos);
    DrawControls(w);
    DrawGrowIcon(w);
    DrawStatus();
    EndUpdate(w);
}

static void DoContent(WindowPtr w, Point where)
{
    ControlHandle c;
    short part;
    Rect a;
    SetPort(w);
    GlobalToLocal(&where);
    part = FindControl(where, w, &c);
    if (c == sScroll) {
        if (part == inThumb) {
            TrackControl(c, where, NULL);
            sScrollPos = -1;
            ScrollTo(GetControlValue(c));
        } else TrackControl(c, where, NewControlActionUPP(ScrollAction));
        return;
    }
    AreaRect(&a);
    if (PtInRect(where, &a)) MapClick(where, &a, sScrollPos);
}

static void DoMouse(EventRecord *e)
{
    WindowPtr w;
    short part = FindWindow(e->where, &w);
    Rect lim;
    long sz;
    switch (part) {
    case inMenuBar: AdjustMenus(); DoMenu(MenuSelect(e->where)); break;
    case inSysWindow: SystemClick(e, w); break;
    case inDrag: DragWindow(w, e->where, &qd.screenBits.bounds); break;
    case inGoAway: if (TrackGoAway(w, e->where)) sQuit = true; break;
    case inGrow:
        SetRect(&lim, 300, 200, 2000, 2000);
        sz = GrowWindow(w, e->where, &lim);
        if (sz) {
            SizeWindow(w, LoWord(sz), HiWord(sz), true);
            SetPort(w);
            InvalRect(&w->portRect);
            PlaceScroll();
            FixScroll();
        }
        break;
    case inContent:
        if (w != FrontWindow()) SelectWindow(w);
        else DoContent(w, e->where);
        break;
    }
}

static void DoKey(EventRecord *e)
{
    char c = e->message & charCodeMask;
    if (e->modifiers & cmdKey) { AdjustMenus(); DoMenu(MenuKey(c)); return; }
    if (c == 8 || c == 27) MapZoomOut();                 /* delete or esc: back out */
    else if (c == 30) ScrollTo(sScrollPos - 16);         /* arrows */
    else if (c == 31) ScrollTo(sScrollPos + 16);
}

/* Give other programs time while Sherpa waits on the network. SystemTask alone does not: under
 * System 7 only WaitNextEvent lets another application (MiniVNC, say) run, and a scan that only called
 * SystemTask froze the Performa's VNC server for the length of every lookup (29 Oct 2026). Mask 0 yields
 * without taking any event out of the queue. */
void Yield(void)
{
    EventRecord e;
    if (sHasWNE) WaitNextEvent(0, &e, 5, NULL);
    else SystemTask();
}

int main(void)
{
    EventRecord e;
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
    sHasWNE = TrapAvailable(0xA860);
    PrefsLoad();
    SetupMenus();
    MakeWindow();
    if (gPrefs.rescanMins) sNextRescan = TickCount() + gPrefs.rescanMins * 3600L;
    AutoTestLoad();

    while (!sQuit) {
        Boolean got;
        if (sHasWNE) got = WaitNextEvent(everyEvent, &e, gScanning ? 1 : 30, NULL);
        else { SystemTask(); got = GetNextEvent(everyEvent, &e); }
        if (got) switch (e.what) {
            case mouseDown: DoMouse(&e); break;
            case keyDown: case autoKey: DoKey(&e); break;
            case updateEvt: DoUpdate((WindowPtr)e.message); break;
            case activateEvt:
                if ((WindowPtr)e.message == gMapWin) {
                    SetPort(gMapWin);
                    HiliteControl(sScroll, (e.modifiers & activeFlag) ? 0 : 255);
                    DrawGrowIcon(gMapWin);
                }
                break;
        }
        ScanIdle();
        AutoTestIdle();
        if (gPrefs.rescanMins && !gScanning && TickCount() > sNextRescan && ATAvailable())
            StartScan(sLastWasAll);
    }
    ScanQuit();
    return 0;
}
