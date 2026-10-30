/* local.c: which zones make up the local network.
 *
 * The local network is the networks plugged straight into this network's router, the
 * IIsi in Tim's lab: LisaGaming (net 38, Ethernet), Lisa_Net_LT (35, LocalTalk) and
 * Lisa_Net_E (37). ZIP's GetLocalZones only knows this Mac's own cable, and Lisa_Net_E has
 * no devices, so NBP can't find it either: only the router knows. So Sherpa asks it:
 *   1. RTMP Route Data Request to the router's RTMP socket (1): its routing table comes
 *      back, and the networks at distance 0 are the ones plugged into it.
 *   2. ZIP Query to its zone information socket (6) for those networks: their zone names.
 * Both answers come back to a DDP socket of Sherpa's own, read by the socket listener below.
 * If the router doesn't answer, the local network is this cable's zones (ZIP GetLocalZones).
 * Packet layouts: Inside AppleTalk, 2nd ed., chapters 5 (RTMP) and 8 (ZIP). */
#include "sherpa.h"
#include <Multiverse.h>
#include "appletalk.h"

#define kDDPBufSize 4096
#define kMaxNets 16

/* Packets to our socket, appended as <length word><data, padded to even>. */
unsigned char gDDPBuf[kDDPBufSize];
volatile short gDDPUsed;

/* The socket listener. .MPP calls it at interrupt time with the packet's DDP header already
 * read, and passes everything in registers (Inside Macintosh: Networking, "DDP socket
 * listener"): A4 = ReadPacket (ReadRest = 2(A4)), D1 = data bytes left. Until ReadRest it
 * may use only A3, D0, D2 and D3; ReadRest leaves D2 alone. A full buffer drops the packet
 * (ReadRest with D3 = 0). ReadRest is asked for exactly D1 bytes: D3 comes back 0 when it
 * read them all, positive when the buffer was bigger than the packet. 0.1d5 asked for 590
 * and took anything but an exact fit as an error, so it dropped every answer (Tim's
 * Performa, 27 Oct: the IIsi answers fine, ask_router.py on the Pi 3 got all 3 zones).
 * 0.1d6 tested D3 for zero after the read; Apple's sample listener (IM Networking,
 * listing 7-6) does "MOVE.L D1,D3; JSR 2(A4); BEQ ok", so since 0.1d7 the condition
 * codes decide, as Apple does, and D3 is ignored.
 * Globals here are absolute addresses, not A5-relative, so they are safe at interrupt time. */
extern void SherpaListener(void);
asm(
"       .text\n"
"       .even\n"
"       .globl  SherpaListener\n"
"SherpaListener:\n"
"       move.w  gDDPUsed, %d2\n"
"       cmp.w   #4096-600, %d2\n"
"       bhi.s   9f\n"
"       lea     gDDPBuf, %a3\n"
"       adda.w  %d2, %a3\n"
"       move.w  %d1, (%a3)+\n"          /* length word */
"       move.w  %d1, %d3\n"             /* exactly the bytes left (at most 586) */
"       jsr     2(%a4)\n"               /* ReadRest into the buffer */
"       bne.s   8f\n"                   /* error (condition codes, as in IM Networking listing 7-6): drop it */
"       lea     gDDPBuf, %a3\n"
"       adda.w  %d2, %a3\n"
"       move.w  (%a3), %d0\n"
"       addq.w  #3, %d0\n"
"       and.w   #0xFFFE, %d0\n"         /* 2 + length, rounded up to even */
"       add.w   %d0, %d2\n"
"       move.w  %d2, gDDPUsed\n"
"8:     rts\n"
"9:     moveq   #0, %d3\n"
"       jmp     2(%a4)\n"
);

#pragma pack(push, 2)
static struct { short len0; Ptr hdr; short len1; Ptr data; short end; } sWDS;
#pragma pack(pop)
static unsigned char sHdrBuf[18] __attribute__((aligned(2)));   /* header at sHdrBuf + 1 (odd) */
static short sMPPRef = -10;

/* What the router said, for the Local Network Zones dialog and the status line. */
unsigned char gRouterNote[100];

static OSErr SendDDP(unsigned char sock, unsigned short net, unsigned char node,
                     unsigned char dsock, unsigned char type, void *data, short len)
{
    static DDPPB pb;
    unsigned char *h = sHdrBuf + 1;
    memset(sHdrBuf, 0, sizeof sHdrBuf);
    h[7] = net >> 8; h[8] = net;        /* the 16-byte LAP + long DDP header: destination */
    h[11] = node;
    h[13] = dsock;
    h[15] = type;
    sWDS.len0 = 0; sWDS.hdr = (Ptr)h;
    sWDS.len1 = len; sWDS.data = (Ptr)data;
    sWDS.end = 0;
    memset(&pb, 0, sizeof pb);
    pb.ioRefNum = sMPPRef;
    pb.csCode = writeDDP;
    pb.socket = sock;
    pb.listener = (Ptr)&sWDS;
    return PBControlSync((ParmBlkPtr)&pb);
}

/* Wait for answers: up to 3 seconds, or half a second after the last packet. */
static void Collect(void)
{
    unsigned long start = TickCount(), lastChange = start;
    short seen = 0;
    while (TickCount() - start < 180) {
        if (gDDPUsed != seen) { seen = gDDPUsed; lastChange = TickCount(); }
        if (seen && TickCount() - lastChange > 30) break;
        Yield();
    }
}

/* Each packet in gDDPBuf in turn: *p and *len, or false when done. */
static Boolean NextPacket(short *pos, unsigned char **p, short *len)
{
    if (*pos + 2 > gDDPUsed) return false;
    *len = *(short *)(gDDPBuf + *pos);
    *p = gDDPBuf + *pos + 2;
    *pos += (2 + *len + 1) & ~1;
    return true;
}

static short AskRouter(Str32a *out, short max)
{
    static ATInfoPB info;
    static DDPPB pb;
    unsigned short nets[kMaxNets], rnet;
    unsigned char rnode, sock, req[2 + 2 * kMaxNets], *p;
    short nNets = 0, n = 0, pos, len, i, k, packets = 0;
    OSErr err;

    memset(&info, 0, sizeof info);
    info.ioRefNum = sMPPRef;
    info.csCode = GetATalkInfo;
    info.version = 1;
    CtoP("No router answer: ", gRouterNote);
    if ((err = PBControlSync((ParmBlkPtr)&info)) != noErr) {
        PCatC(gRouterNote, "AppleTalk info error "); PCatNum(gRouterNote, err); return 0;
    }
    rnet = (unsigned long)info.routerAddr >> 16;
    rnode = ((unsigned long)info.routerAddr >> 8) & 0xFF;
    if (rnode == 0) { PCatC(gRouterNote, "no router heard on this network."); return 0; }
    CtoP("Router ", gRouterNote);
    PCatNum(gRouterNote, rnet); PCatC(gRouterNote, "."); PCatNum(gRouterNote, rnode);

    memset(&pb, 0, sizeof pb);
    pb.ioRefNum = sMPPRef;
    pb.csCode = openSkt;
    pb.socket = 0;                                  /* any free socket */
    pb.listener = (Ptr)SherpaListener;
    if ((err = PBControlSync((ParmBlkPtr)&pb)) != noErr) {
        PCatC(gRouterNote, ": socket error "); PCatNum(gRouterNote, err); return 0;
    }
    sock = pb.socket;

    /* 1. RTMP Route Data Request (DDP type 5, command 3: the whole table, no split horizon) */
    gDDPUsed = 0;
    req[0] = 3;
    if ((err = SendDDP(sock, rnet, rnode, 1, 5, req, 1)) == noErr) Collect();
    for (pos = 0; NextPacket(&pos, &p, &len); ) {
        packets++;
        /* router's net (2), ID length in bits (1), node ID, then the routing tuples */
        i = 4;
        if (len >= 7 && p[4] == 0 && p[5] == 0 && p[6] == 0x82) i = 7;   /* non-extended marker */
        while (i + 3 <= len) {
            unsigned short net = (p[i] << 8) | p[i + 1];
            unsigned char d = p[i + 2];
            i += 3;
            if (d & 0x80) i += 3;                   /* extended: range end and version */
            if ((d & 0x1F) != 0 || net == 0 || nNets == kMaxNets) continue;
            for (k = 0; k < nNets && nets[k] != net; k++) ;
            if (k == nNets) nets[nNets++] = net;
        }
    }

    /* 2. ZIP Query (DDP type 6) for the zones of those networks */
    if (nNets > 0) {
        gDDPUsed = 0;
        req[0] = 1;                                 /* Query */
        req[1] = nNets;
        for (k = 0; k < nNets; k++) { req[2 + 2 * k] = nets[k] >> 8; req[3 + 2 * k] = nets[k]; }
        if (SendDDP(sock, rnet, rnode, 6, 6, req, 2 + 2 * nNets) == noErr) Collect();
        for (pos = 0; NextPacket(&pos, &p, &len); ) {
            if (len < 2 || (p[0] != 2 && p[0] != 8)) continue;   /* Reply or Extended Reply */
            for (i = 2; i + 3 <= len && i + 3 + p[i + 2] <= len; i += 3 + p[i + 2]) {
                unsigned char *z = p + i + 2;       /* the zone, as a Pascal string */
                if (z[0] == 0 || z[0] > 32 || n == max) continue;
                for (k = 0; k < n && !PEqualNoCase(out[k], z); k++) ;
                if (k == n) PCopy(out[n++], z, 32);
            }
        }
    }

    if (err) { PCatC(gRouterNote, ": send error "); PCatNum(gRouterNote, err); }
    else if (packets == 0) PCatC(gRouterNote, " didn't answer.");
    else {
        PCatC(gRouterNote, ": "); PCatNum(gRouterNote, nNets);
        PCatC(gRouterNote, nNets == 1 ? " network, " : " networks, "); PCatNum(gRouterNote, n);
        PCatC(gRouterNote, n == 1 ? " zone." : " zones.");
    }

    pb.csCode = closeSkt;
    pb.socket = sock;
    PBControlSync((ParmBlkPtr)&pb);
    return n;
}

short FindLocalNetwork(Str32a *out, short max)
{
    short n;
    CtoP("AppleTalk is off.", gRouterNote);
    if (ATOpen() != noErr) return 0;
    n = AskRouter(out, max);
    if (n <= 0) {
        if (GetLocalZoneNames(out, max, &n) != noErr) n = 0;
        PCatC(gRouterNote, " Using this cable's zones.");
    }
    return n;
}
