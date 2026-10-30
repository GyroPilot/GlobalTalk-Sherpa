/* util.c: Pascal-string and handle helpers. */
#include "sherpa.h"
#include <Multiverse.h>
#include <TextUtils.h>

void CtoP(const char *c, unsigned char *p)
{
    short n = strlen(c);
    if (n > 255) n = 255;
    p[0] = n;
    memcpy(p + 1, c, n);
}

void PCopy(unsigned char *dst, ConstStringPtr src, short max)
{
    short n = src[0] > max ? max : src[0];
    dst[0] = n;
    memcpy(dst + 1, src + 1, n);
}

void PCat(unsigned char *p, ConstStringPtr q)
{
    short n = q[0];
    if (p[0] + n > 255) n = 255 - p[0];
    memcpy(p + 1 + p[0], q + 1, n);
    p[0] += n;
}

void PCatC(unsigned char *p, const char *c)
{
    unsigned char q[256];
    CtoP(c, q);
    PCat(p, q);
}

void PCatNum(unsigned char *p, long n)
{
    Str255 q;
    NumToString(n, q);
    PCat(p, q);
}

static unsigned char Lower(unsigned char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

Boolean PEqualNoCase(ConstStringPtr a, ConstStringPtr b)
{
    short i;
    if (a[0] != b[0]) return false;
    for (i = 1; i <= a[0]; i++) if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}

Boolean PContains(ConstStringPtr hay, const char *needle)
{
    short n = strlen(needle), i, j;
    for (i = 1; i + n - 1 <= hay[0]; i++) {
        for (j = 0; j < n && Lower(hay[i + j]) == Lower(needle[j]); j++) ;
        if (j == n) return true;
    }
    return false;
}

void HAppendC(Handle h, const char *c) { if (h) PtrAndHand(c, h, strlen(c)); }
void HAppendP(Handle h, ConstStringPtr p) { if (h) PtrAndHand(p + 1, h, p[0]); }

void PCatU(unsigned char *p, unsigned long n)
{
    char b[12];
    short i = 11;
    b[i] = 0;
    do { b[--i] = '0' + n % 10; n /= 10; } while (n);
    PCatC(p, b + i);
}

/* System 7's Date & Time panel takes a two-digit year, so a Mac set to "26" sits in 1926 (Tim's
 * Performa, 29 Oct 2026) and every date Sherpa printed said 1926. A year before 1984 cannot be
 * right for a scan, so a century is added. Saved dates from before this fix pass through FixYear. */
unsigned long FixYear(unsigned long secs)
{
    DateTimeRec d;
    SecondsToDate(secs, &d);
    if (d.year >= 1984) return secs;
    d.year += 100;
    DateToSeconds(&d, &secs);
    return secs;
}

unsigned long NowFixed(void)
{
    unsigned long now;
    GetDateTime(&now);
    return FixYear(now);
}
