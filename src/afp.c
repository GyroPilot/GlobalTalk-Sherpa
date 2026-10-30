/* afp.c: what an AFP file server says about itself.
 *
 * FPGetSrvrInfo (ASPGetStatus) needs no login: it's the question the Chooser asks. Its list
 * of login methods says whether the server takes guests ("No User Authent"), and that is all
 * Sherpa reports: no login, no volume names (Tim, 26 Oct 2026). */
#include "sherpa.h"
#include <Multiverse.h>
#include "appletalk.h"

static short sXPP;

static short XPP(void)
{
    if (!sXPP && OpenDriver("\p.XPP", &sXPP)) sXPP = 0;
    return sXPP;
}

static unsigned short Word(unsigned char *b, short off) { return (b[off] << 8) | b[off + 1]; }

static Boolean HasGuest(unsigned char *st)
{
    unsigned short off = Word(st, 4);
    short n = st[off], i;
    unsigned char *p;
    for (i = 0, p = st + off + 1; i < n; i++, p += p[0] + 1)
        if (PContains(p, "No User Authent")) return true;
    return false;
}

void NodeServerInfo(Node *n)
{
    static XPPPB pb;
    AddrBlock addr;
    Ptr buf;
    unsigned char *st;
    short i, k;
    unsigned short off;
    OSErr err;

    if (n->infoDone || !n->isServer || !gScanIsLive) return;
    n->infoDone = true;
    for (i = n->first; i < n->first + n->count; i++)
        if (PContains(gNames[i].type, "AFPServer")) break;
    if (i == n->first + n->count || ATOpen() || !XPP()) return;
    addr.aNet = gNames[i].net; addr.aNode = gNames[i].node; addr.aSocket = gNames[i].socket;

    n->info = NewHandle(0);
    buf = NewPtr(1024);
    if (!buf || !n->info) return;
    SetStatus("Asking the file server about itself\311");
    memset(&pb, 0, sizeof pb);
    pb.ioRefNum = XPP(); pb.csCode = getStatus;
    pb.aspTimeout = 2; pb.aspRetry = 2;
    pb.u.serverAddr = addr;
    pb.rbSize = 1024; pb.rbPtr = buf;
    err = PBControlSync((ParmBlkPtr)&pb);
    if (err) {
        HAppendC(n->info, "File server did not answer\r");
        DisposePtr(buf);
        SetStatus("");
        return;
    }
    st = (unsigned char *)buf;
    HAppendC(n->info, "Server name: "); HAppendP(n->info, st + 10); HAppendC(n->info, "\r");
    off = Word(st, 0);
    if (off) { HAppendC(n->info, "Machine: "); HAppendP(n->info, st + off); HAppendC(n->info, "\r"); }
    off = Word(st, 2);
    HAppendC(n->info, "AFP:");
    for (k = 0, i = off + 1; k < st[off]; k++, i += st[i] + 1) { HAppendC(n->info, " "); HAppendP(n->info, st + i); }
    HAppendC(n->info, "\r");
    HAppendC(n->info, HasGuest(st) ? "Guest access: yes\r" : "Guest access: no\r");
    off = Word(st, 6);
    if (off && off + 256 <= 1024) {
        n->srvIcon = NewHandle(256);
        if (n->srvIcon) BlockMoveData(st + off, *n->srvIcon, 256);
    }
    DisposePtr(buf);
    SetStatus("");
}
