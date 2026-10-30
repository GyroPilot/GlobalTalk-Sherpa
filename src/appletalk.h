/* appletalk.h: the classic AppleTalk Manager driver interface, as far as Sherpa needs it.
 *
 * Retro68's Multiversal headers have no AppleTalk, so these are written from the record
 * layouts in Inside Macintosh: Networking (MPP, XPP and ASP parameter blocks). Offsets are
 * noted so they can be checked against Apple's AppleTalk.h if a copy turns up.
 * The calls go straight to the drivers with PBControl, which works from System 6 on. */
#pragma once
#include <Devices.h>

enum {
    mppRefNum = -10,                 /* .MPP: DDP and NBP */
    xppRefNum = -41,                 /* .XPP: ASP, AFP and the ZIP calls */

    /* .MPP csCodes */
    writeDDP = 246,
    closeSkt = 247,
    openSkt = 248,
    lookupName = 251,
    killNBP = 254,
    GetATalkInfo = 258,

    /* .XPP csCodes */
    getStatus = 251,                 /* ASPGetStatus = AFP FPGetSrvrInfo, no session needed */
    afpCall = 250,
    xCall = 246,

    /* xCall sub-codes */
    zipGetLocalZones = 5,
    zipGetZoneList = 6,
    zipGetMyZone = 7,

    /* AFP commands */
    afpGetSrvrParms = 16,
    afpLogin = 18,
    afpLogout = 20,

    scbMemSize = 192,
    noBridgeErr = -93,
    reqFailed = -1096
};

#pragma pack(push, 2)

typedef struct {                     /* 4 bytes */
    unsigned short aNet;
    unsigned char aNode;
    unsigned char aSocket;
} AddrBlock;

#define ATPB_HEADER \
    void *qLink;            /*  0 */ \
    short qType;            /*  4 */ \
    short ioTrap;           /*  6 */ \
    Ptr ioCmdAddr;          /*  8 */ \
    void *ioCompletion;     /* 12 */ \
    volatile OSErr ioResult; /* 16 */

/* MPP NBP lookup (lookupName) */
typedef struct {
    ATPB_HEADER
    long userData;          /* 18 */
    short reqTID;           /* 22 */
    short ioRefNum;         /* 24 */
    short csCode;           /* 26 */
    unsigned char interval; /* 28: retry interval, 8-tick units */
    unsigned char count;    /* 29: number of tries */
    Ptr entityPtr;          /* 30: packed object, type, zone */
    Ptr retBuffPtr;         /* 34 */
    short retBuffSize;      /* 38 */
    short maxToGet;         /* 40 */
    short numGotten;        /* 42 */
} NBPLookupPB;

/* MPP GetATalkInfo (PGetAppleTalkInfo) */
typedef struct {
    ATPB_HEADER
    long userData;          /* 18 */
    short reqTID;           /* 22 */
    short ioRefNum;         /* 24 */
    short csCode;           /* 26 */
    short version;          /* 28: in, 1 */
    Ptr varsPtr;            /* 30 */
    Ptr DCEPtr;             /* 34 */
    short portID;           /* 38 */
    long configuration;     /* 40 */
    short selfSend;         /* 44 */
    short netLo;            /* 46 */
    short netHi;            /* 48 */
    long ourAdd;            /* 50: net (2), node (1), socket (1) */
    long routerAddr;        /* 54 */
    short numOfPHs;         /* 58 */
    short numOfSkts;        /* 60 */
    short numNBPEs;         /* 62 */
    Ptr nTQueue;            /* 64 */
    short LAlength;         /* 68 */
    Ptr linkAddr;           /* 70 */
    Ptr zoneName;           /* 74 */
} ATInfoPB;

/* XPP xCall: the ZIP zone calls */
typedef struct {
    ATPB_HEADER
    long cmdResult;         /* 18 */
    short ioVRefNum;        /* 22 */
    short ioRefNum;         /* 24 */
    short csCode;           /* 26 */
    short xppSubCode;       /* 28 */
    unsigned char xppTimeout; /* 30: seconds */
    unsigned char xppRetry; /* 31 */
    short filler1;          /* 32 */
    Ptr zipBuffPtr;         /* 34: 578-byte buffer of packed zone names */
    short zipNumZones;      /* 38 */
    unsigned char zipLastFlag; /* 40 */
    unsigned char filler2;  /* 41 */
    unsigned char zipInfoField[70]; /* 42: the driver's state; zero before the first call */
} XCallPB;

/* XPP ASP/AFP calls. ASPGetStatus puts the server address over cbSize (offset 32), as
 * ASPOpenPrm does; AFP login puts it after rbPtr (offset 44). */
typedef struct {
    ATPB_HEADER
    long cmdResult;         /* 18: the AFP result code */
    short ioVRefNum;        /* 22 */
    short ioRefNum;         /* 24 */
    short csCode;           /* 26 */
    short sessRefnum;       /* 28 */
    unsigned char aspTimeout; /* 30: seconds */
    unsigned char aspRetry; /* 31 */
    union {
        struct { short cbSize; Ptr cbPtr; } cmd;     /* 32, 34 */
        AddrBlock serverAddr;                        /* 32: ASPGetStatus */
    } u;
    short rbSize;           /* 38 */
    Ptr rbPtr;              /* 40 */
    AddrBlock afpAddrBlock; /* 44: afpLogin */
    Ptr afpSCBPtr;          /* 48: afpLogin, scbMemSize bytes */
    Ptr afpAttnRoutine;     /* 52: afpLogin */
    unsigned char ccbFill[144]; /* 56 */
} XPPPB;

/* MPP DDP calls: POpenSkt, PCloseSkt, PWriteDDP */
typedef struct {
    ATPB_HEADER
    long userData;          /* 18 */
    short reqTID;           /* 22 */
    short ioRefNum;         /* 24 */
    short csCode;           /* 26 */
    unsigned char socket;   /* 28: in 0 for any, out the socket (POpenSkt) */
    unsigned char checksumFlag; /* 29 */
    Ptr listener;           /* 30: POpenSkt's socket listener, or PWriteDDP's write-data structure */
} DDPPB;

#pragma pack(pop)
