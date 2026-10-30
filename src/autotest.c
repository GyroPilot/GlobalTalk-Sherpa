/* autotest.c: an unattended test run, so emulators can test Sherpa with nobody at the mouse.
 *
 * If a text file named "Sherpa Autotest" sits next to the application, Sherpa reads it at
 * startup and runs one command per line, in order:
 *   open <file>        open a saved scan            world        back to all zones
 *   zone <name>        show a zone                  node <name>  show a device's card
 *   diagram / dashboard  the other top-level views       about        the About box (last: it waits)
 *   scan site | all    start a live scan            waitscan     wait for the scan to end
 *   pict <file>        save the current view        text <file>  save the list
 *   wait <seconds>     pause (for screenshots)      quit
 * Without that file it does nothing. */
#include "sherpa.h"
#include <Multiverse.h>
#include <stdlib.h>

static Handle sCmds;
static long sPos;
static unsigned long sWaitUntil;
static Boolean sWaitScan;

void AutoTestLoad(void)
{
    short ref;
    long len;
    if (FSOpen("\pSherpa Autotest", 0, &ref)) return;
    GetEOF(ref, &len);
    sCmds = NewHandle(len + 1);
    if (sCmds) {
        FSRead(ref, &len, *sCmds);
        (*sCmds)[len] = '\r';
        SetHandleSize(sCmds, len + 1);
    }
    FSClose(ref);
}

static short FindZone(ConstStringPtr z)
{
    short i;
    for (i = 0; i < gNumZones; i++) if (PEqualNoCase(gZones[i].name, z)) return i;
    return -1;
}

void AutoTestIdle(void)
{
    char line[300], *arg;
    unsigned char p[256];
    long len, i;
    short k;
    if (!sCmds || TickCount() < sWaitUntil) return;
    if (sWaitScan) { if (gScanning) return; sWaitScan = false; }
    len = GetHandleSize(sCmds);
    if (sPos >= len) { DisposeHandle(sCmds); sCmds = NULL; return; }
    for (i = 0; sPos < len && (*sCmds)[sPos] != '\r' && (*sCmds)[sPos] != '\n' && i < 299; ) line[i++] = (*sCmds)[sPos++];
    line[i] = 0;
    while (sPos < len && ((*sCmds)[sPos] == '\r' || (*sCmds)[sPos] == '\n')) sPos++;
    arg = strchr(line, ' ');
    if (arg) *arg++ = 0; else arg = "";
    CtoP(arg, p);
    if (!strcmp(line, "open")) OpenScanNamed(p, 0);
    else if (!strcmp(line, "world")) { gPrefs.view = kViewZones; gLevel = 0; MapChanged(); }
    else if (!strcmp(line, "about")) Alert(128, NULL);  /* waits for OK, so put it last */
    else if (!strcmp(line, "diagram")) { gPrefs.view = kViewDiagram; gLevel = 0; MapChanged(); }
    else if (!strcmp(line, "dashboard")) { gPrefs.view = kViewDashboard; gLevel = 0; MapChanged(); }
    else if (!strcmp(line, "zone")) { k = FindZone(p); if (k >= 0) { gCurZone = k; gLevel = 1; MapChanged(); } }
    else if (!strcmp(line, "node")) {
        for (k = 0; k < gNumNodes; k++)
            if (PEqualNoCase(gNames[gNodes[k].label].obj, p)) {
                gCurZone = gNodes[k].zone; gCurNode = k; gLevel = 2;
                MapChanged();
                if (gNodes[k].isServer) { NodeServerInfo(&gNodes[k]); MapChanged(); }
                break;
            }
    }
    else if (!strcmp(line, "router")) { static Str32a z[8]; FindLocalNetwork(z, 8); SetStatusP(gRouterNote); }
    else if (!strcmp(line, "scan")) ScanStart(!strcmp(arg, "all"));
    else if (!strcmp(line, "waitscan")) sWaitScan = true;
    else if (!strcmp(line, "pict")) SavePictNamed(p, 0);
    else if (!strcmp(line, "text")) SaveTextNamed(p, 0);
    else if (!strcmp(line, "wait")) sWaitUntil = TickCount() + atol(arg) * 60;
    else if (!strcmp(line, "quit")) { ExitToShell(); }
}
