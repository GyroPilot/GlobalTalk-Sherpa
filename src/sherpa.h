/* sherpa.h: GlobalTalk Sherpa, an AppleTalk scanner and mapper for 68k Macs (System 6.0.8 and up). */
#pragma once
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Events.h>
#include <Resources.h>
#include <Files.h>
#include <Memory.h>
#include <string.h>

#define kMaxZones   160
#define kMaxNames   700
#define kMaxNodes   400
#define kMaxHist    600
#define kMaxLab     8

/* icon kinds; ICN# resource id = 200 + kind */
enum { kUnknown, kCompact, kDesktop, kLisa, kPrinter, kServer, kRouter, kAppleII,
       kPortable, kZone, kUnix,
       kMacII, kPizza, kTower, kAllInOne, kPowerMac,   /* Mac families, 0.1d5 (appended: kinds are saved in icon names) */
       kDotMatrix,                                     /* 0.1d7: printers are two classes, box (laser, inkjet) and dot matrix */
       kNumKinds };

typedef unsigned char Str32a[33];

typedef struct {
    Str32a name;
    short nodes;            /* devices seen in the last scan */
    short names;
    Boolean scanned;        /* covered by the last scan */
    Boolean lab;
    short err, got;         /* the lookup's result code and reply count, for the list */
    Boolean retried;        /* 1.0.1: looked up a second time, slower, after 0 replies */
} ATZone;

typedef struct {
    unsigned short net;
    unsigned char node, socket;
    short zone;
    Str32a obj, type;
} Name;

typedef struct {
    unsigned short net;
    unsigned char node;
    short zone;
    short first, count;     /* range in the sorted names */
    short kind;
    short label;            /* index of the name used as its label */
    short model;            /* index of the socket-253 name, or -1 */
    short hist;             /* index in hist[], or -1 */
    Boolean isRouter, isServer, hasPrinter, dotMatrix;
    Boolean infoDone;       /* AFP server info fetched */
    Handle info;            /* text lines from the AFP check */
    Handle srvIcon;         /* 256 bytes: icon + mask from FPGetSrvrInfo */
} Node;

typedef struct {            /* reliability history, kept in the prefs file's data fork */
    Str32a zone, label;
    short seen, chances;    /* scans that saw it / scans of its zone since it was first seen */
    unsigned long first, last;
    short misses;           /* 1.0.1: scans of its zone in a row that did not see it; "gone" at 2 */
} Hist;

enum { kViewZones, kViewDiagram, kViewDashboard };

typedef struct {
    short version;
    Boolean view;           /* top level: kViewZones, kViewDiagram or kViewDashboard (was the guest volume flag) */
    short rescanMins;       /* 0 = off */
    short nbpInterval, nbpCount;
    short labCount;
} Prefs;

/* One line of the dashboard's scan history, kept in the prefs file ('SScn' 128, newest first). */
#define kMaxScans 12
typedef struct {
    unsigned long when;
    short zones, devices, names;
    short added, gone;      /* devices new to, and missing from, zones scanned before */
    Boolean all, pad;       /* Scan All, or the local network */
    short kinds[kNumKinds];
} ScanSum;

typedef struct { Str32a zone, label; } DevRef;
#define kMaxChanges 40

/* main.c */
extern WindowPtr gMapWin;
extern Prefs gPrefs;
extern Str32a gLab[kMaxLab];
extern Str32a gMyZone;
void SetStatus(const char *msg);
void SetStatusP(ConstStringPtr p);
void Alert1(const char *msg);
void MapChanged(void);

/* scan.c */
extern ATZone gZones[kMaxZones];
extern short gNumZones;
extern Name gNames[kMaxNames];
extern short gNumNames;
extern Node gNodes[kMaxNodes];
extern short gNumNodes;
extern unsigned long gScanTime;
extern Boolean gScanning, gScanIsLive;
OSErr ATOpen(void);
Boolean ATAvailable(void);
void ScanStart(Boolean wholeNet);
void ScanIdle(void);
void Yield(void);                       /* main.c: let other programs run while we wait */
unsigned long NowFixed(void);           /* util.c: GetDateTime, with a two-digit-year Mac's 1926 read as 2026 */
unsigned long FixYear(unsigned long secs);
void ScanStop(void);
void ScanQuit(void);
void BuildNodes(void);
Boolean IsLabZone(ConstStringPtr z);
OSErr GetLocalZoneNames(Str32a *out, short max, short *n);
extern Boolean gScanAll;

/* local.c */
short FindLocalNetwork(Str32a *out, short max);
extern unsigned char gRouterNote[100];   /* what the router said, as a Pascal string */

/* afp.c */
void NodeServerInfo(Node *n);

/* map.c */
extern short gLevel, gCurZone, gCurNode;
void MapDraw(Rect *area, short scroll);
short MapHeight(short width);
void MapClick(Point p, Rect *area, short scroll);
void MapZoomOut(void);
void DrawRelBar(short x, short y, Node *n);
PicHandle MapPicture(short width);
Handle MapText(void);

/* store.c */
extern Hist gHist[kMaxHist];
extern short gNumHist;
extern ScanSum gScans[kMaxScans];
extern short gNumScans;
extern DevRef gAdded[kMaxChanges], gGone[kMaxChanges];
extern short gNumAdded, gNumGone;       /* listed after a live scan in this session; -1 before */
void PrefsLoad(void);
void PrefsSave(void);
void HistRecordScan(void);
void HistLink(void);
short HistRating(Node *n);      /* 0 = new, 1..5 */
Handle KindIcon(short kind);
Handle IconFor(Node *n, short *kind);
OSErr SaveCustomIcon(Node *n, Boolean wholeModel, Handle icn);
void RemoveCustomIcon(Node *n);
void SavePictFile(void);
void SaveTextFile(void);
void OpenScanFile(void);
void OpenScanNamed(ConstStringPtr name, short vref);
OSErr SavePictNamed(ConstStringPtr name, short vref);
OSErr SaveTextNamed(ConstStringPtr name, short vref);
Handle MapText(void);

/* autotest.c */
void AutoTestLoad(void);
void AutoTestIdle(void);
void ExportIconPack(void);
void ImportIconPack(void);
Handle IconFromScrap(void);
void CopyIconToScrap(Node *n);

/* util */
void CtoP(const char *c, unsigned char *p);
void PCat(unsigned char *p, ConstStringPtr q);
void PCatC(unsigned char *p, const char *c);
void PCatNum(unsigned char *p, long n);
void PCatU(unsigned char *p, unsigned long n);
extern short gZipErr, gInfoErr;
extern long gOurAddr, gRouterAddr;
extern unsigned short gNetLo, gNetHi;
void SplitAddr(long a, unsigned short lo, unsigned short hi, unsigned short *net, unsigned char *node);
void PCopy(unsigned char *dst, ConstStringPtr src, short max);
Boolean PEqualNoCase(ConstStringPtr a, ConstStringPtr b);
Boolean PContains(ConstStringPtr hay, const char *needle);
void HAppendC(Handle h, const char *c);
void HAppendP(Handle h, ConstStringPtr p);
