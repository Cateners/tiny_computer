/*
 * hextile.c
 *
 * Routines to implement Hextile Encoding
 */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  Original Xvnc code Copyright (C) 1999 AT&T Laboratories Cambridge.  
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <rfb/rfb.h>

static rfbBool sendHextiles8(rfbClientPtr cl, int x, int y, int w, int h);
static rfbBool sendHextiles16(rfbClientPtr cl, int x, int y, int w, int h);
static rfbBool sendHextiles32(rfbClientPtr cl, int x, int y, int w, int h);


/*
 * rfbSendRectEncodingHextile - send a rectangle using hextile encoding.
 */

rfbBool
rfbSendRectEncodingHextile(rfbClientPtr cl,
                           int x,
                           int y,
                           int w,
                           int h)
{
    rfbFramebufferUpdateRectHeader rect;
    
    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingHextile);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
           sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    rfbStatRecordEncodingSent(cl, rfbEncodingHextile,
          sz_rfbFramebufferUpdateRectHeader,
          sz_rfbFramebufferUpdateRectHeader + w * (cl->format.bitsPerPixel / 8) * h);

    switch (cl->format.bitsPerPixel) {
    case 8:
        return sendHextiles8(cl, x, y, w, h);
    case 16:
        return sendHextiles16(cl, x, y, w, h);
    case 32:
        return sendHextiles32(cl, x, y, w, h);
    }

    rfbLog("rfbSendRectEncodingHextile: bpp %d?\n", cl->format.bitsPerPixel);
    return FALSE;
}


#define PUT_PIXEL8(pix) (cl->updateBuf[cl->ublen++] = (pix))

#define PUT_PIXEL16(pix) (cl->updateBuf[cl->ublen++] = ((char*)&(pix))[0], \
                          cl->updateBuf[cl->ublen++] = ((char*)&(pix))[1])

#define PUT_PIXEL32(pix) (cl->updateBuf[cl->ublen++] = ((char*)&(pix))[0], \
                          cl->updateBuf[cl->ublen++] = ((char*)&(pix))[1], \
                          cl->updateBuf[cl->ublen++] = ((char*)&(pix))[2], \
                          cl->updateBuf[cl->ublen++] = ((char*)&(pix))[3])


#define DEFINE_SEND_HEXTILES(bpp)                                               \
                                                                                \
                                                                                \
static rfbBool subrectEncode##bpp(rfbClientPtr cli, uint##bpp##_t *data,        \
		int w, int h, uint##bpp##_t bg, uint##bpp##_t fg, rfbBool mono);\
static void testColours##bpp(uint##bpp##_t *data, int size, rfbBool *mono,      \
                  rfbBool *solid, uint##bpp##_t *bg, uint##bpp##_t *fg);        \
                                                                                \
                                                                                \
/*                                                                              \
 * rfbSendHextiles                                                              \
 */                                                                             \
                                                                                \
static rfbBool                                                                  \
sendHextiles##bpp(rfbClientPtr cl, int rx, int ry, int rw, int rh) {            \
    int x, y, w, h;                                                             \
    int startUblen;                                                             \
    char *fbptr;                                                                \
    uint##bpp##_t bg = 0, fg = 0, newBg, newFg;                                 \
    rfbBool mono, solid;                                                        \
    rfbBool validBg = FALSE;                                                    \
    rfbBool validFg = FALSE;                                                    \
    uint##bpp##_t clientPixelData[16*16*(bpp/8)];                               \
                                                                                \
    for (y = ry; y < ry+rh; y += 16) {                                          \
        for (x = rx; x < rx+rw; x += 16) {                                      \
            w = h = 16;                                                         \
            if (rx+rw - x < 16)                                                 \
                w = rx+rw - x;                                                  \
            if (ry+rh - y < 16)                                                 \
                h = ry+rh - y;                                                  \
                                                                                \
            if ((cl->ublen + 1 + (2 + 16 * 16) * (bpp/8)) >                     \
                UPDATE_BUF_SIZE) {                                              \
                if (!rfbSendUpdateBuf(cl))                                      \
                    return FALSE;                                               \
            }                                                                   \
                                                                                \
            fbptr = (cl->scaledScreen->frameBuffer + (cl->scaledScreen->paddedWidthInBytes * y)   \
                     + (x * (cl->scaledScreen->bitsPerPixel / 8)));                   \
                                                                                \
            (*cl->translateFn)(cl->translateLookupTable, &(cl->screen->serverFormat),      \
                               &cl->format, fbptr, (char *)clientPixelData,     \
                               cl->scaledScreen->paddedWidthInBytes, w, h);           \
                                                                                \
            startUblen = cl->ublen;                                             \
            cl->updateBuf[startUblen] = 0;                                      \
            cl->ublen++;                                                        \
            rfbStatRecordEncodingSentAdd(cl, rfbEncodingHextile, 1);            \
                                                                                \
            testColours##bpp(clientPixelData, w * h,                            \
                             &mono, &solid, &newBg, &newFg);                    \
                                                                                \
            if (!validBg || (newBg != bg)) {                                    \
                validBg = TRUE;                                                 \
                bg = newBg;                                                     \
                cl->updateBuf[startUblen] |= rfbHextileBackgroundSpecified;     \
                PUT_PIXEL##bpp(bg);                                             \
            }                                                                   \
                                                                                \
            if (solid) {                                                        \
                continue;                                                       \
            }                                                                   \
                                                                                \
            cl->updateBuf[startUblen] |= rfbHextileAnySubrects;                 \
                                                                                \
            if (mono) {                                                         \
                if (!validFg || (newFg != fg)) {                                \
                    validFg = TRUE;                                             \
                    fg = newFg;                                                 \
                    cl->updateBuf[startUblen] |= rfbHextileForegroundSpecified; \
                    PUT_PIXEL##bpp(fg);                                         \
                }                                                               \
            } else {                                                            \
                validFg = FALSE;                                                \
                cl->updateBuf[startUblen] |= rfbHextileSubrectsColoured;        \
            }                                                                   \
                                                                                \
            if (!subrectEncode##bpp(cl, clientPixelData, w, h, bg, fg, mono)) { \
                /* encoding was too large, use raw */                           \
                validBg = FALSE;                                                \
                validFg = FALSE;                                                \
                cl->ublen = startUblen;                                         \
                cl->updateBuf[cl->ublen++] = rfbHextileRaw;                     \
                (*cl->translateFn)(cl->translateLookupTable,                    \
                                   &(cl->screen->serverFormat), &cl->format, fbptr,        \
                                   (char *)clientPixelData,                     \
                                   cl->scaledScreen->paddedWidthInBytes, w, h); \
                                                                                \
                memcpy(&cl->updateBuf[cl->ublen], (char *)clientPixelData,      \
                       (size_t)w * h * (bpp/8));                                \
                                                                                \
                cl->ublen += w * h * (bpp/8);                                   \
                rfbStatRecordEncodingSentAdd(cl, rfbEncodingHextile,            \
                             w * h * (bpp/8));                                  \
            }                                                                   \
        }                                                                       \
    }                                                                           \
                                                                                \
    return TRUE;                                                                \
}                                                                               \
                                                                                \
                                                                                \
static rfbBool                                                                  \
subrectEncode##bpp(rfbClientPtr cl, uint##bpp##_t *data, int w, int h,          \
                   uint##bpp##_t bg, uint##bpp##_t fg, rfbBool mono)            \
{                                                                               \
    uint##bpp##_t cl2;                                                          \
    int x,y;                                                                    \
    int i,j;                                                                    \
    int hx=0,hy,vx=0,vy;                                                        \
    int hyflag;                                                                 \
    uint##bpp##_t *seg;                                                         \
    uint##bpp##_t *line;                                                        \
    int hw,hh,vw,vh;                                                            \
    int thex,they,thew,theh;                                                    \
    int numsubs = 0;                                                            \
    int newLen;                                                                 \
    int nSubrectsUblen;                                                         \
                                                                                \
    nSubrectsUblen = cl->ublen;                                                 \
    cl->ublen++;                                                                \
    rfbStatRecordEncodingSentAdd(cl, rfbEncodingHextile, 1);                    \
                                                                                \
    for (y=0; y<h; y++) {                                                       \
        line = data+(y*w);                                                      \
        for (x=0; x<w; x++) {                                                   \
            if (line[x] != bg) {                                                \
                cl2 = line[x];                                                  \
                hy = y-1;                                                       \
                hyflag = 1;                                                     \
                for (j=y; j<h; j++) {                                           \
                    seg = data+(j*w);                                           \
                    if (seg[x] != cl2) {break;}                                 \
                    i = x;                                                      \
                    while ((i < w) && (seg[i] == cl2)) i += 1;                  \
                    i -= 1;                                                     \
                    if (j == y) vx = hx = i;                                    \
                    if (i < vx) vx = i;                                         \
                    if ((hyflag > 0) && (i >= hx)) {                            \
                        hy += 1;                                                \
                    } else {                                                    \
                        hyflag = 0;                                             \
                    }                                                           \
                }                                                               \
                vy = j-1;                                                       \
                                                                                \
                /* We now have two possible subrects: (x,y,hx,hy) and           \
                 * (x,y,vx,vy).  We'll choose the bigger of the two.            \
                 */                                                             \
                hw = hx-x+1;                                                    \
                hh = hy-y+1;                                                    \
                vw = vx-x+1;                                                    \
                vh = vy-y+1;                                                    \
                                                                                \
                thex = x;                                                       \
                they = y;                                                       \
                                                                                \
                if ((hw*hh) > (vw*vh)) {                                        \
                    thew = hw;                                                  \
                    theh = hh;                                                  \
                } else {                                                        \
                    thew = vw;                                                  \
                    theh = vh;                                                  \
                }                                                               \
                                                                                \
                if (mono) {                                                     \
                    newLen = cl->ublen - nSubrectsUblen + 2;                    \
                } else {                                                        \
                    newLen = cl->ublen - nSubrectsUblen + bpp/8 + 2;            \
                }                                                               \
                                                                                \
                if (newLen > (w * h * (bpp/8)))                                 \
                    return FALSE;                                               \
                                                                                \
                numsubs += 1;                                                   \
                                                                                \
                if (!mono) PUT_PIXEL##bpp(cl2);                                 \
                                                                                \
                cl->updateBuf[cl->ublen++] = rfbHextilePackXY(thex,they);       \
                cl->updateBuf[cl->ublen++] = rfbHextilePackWH(thew,theh);       \
                rfbStatRecordEncodingSentAdd(cl, rfbEncodingHextile, 1);        \
                                                                                \
                /*                                                              \
                 * Now mark the subrect as done.                                \
                 */                                                             \
                for (j=they; j < (they+theh); j++) {                            \
                    for (i=thex; i < (thex+thew); i++) {                        \
                        data[j*w+i] = bg;                                       \
                    }                                                           \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }                                                                           \
                                                                                \
    cl->updateBuf[nSubrectsUblen] = numsubs;                                    \
                                                                                \
    return TRUE;                                                                \
}                                                                               \
                                                                                \
                                                                                \
/*                                                                              \
 * testColours() tests if there are one (solid), two (mono) or more             \
 * colours in a tile and gets a reasonable guess at the best background         \
 * pixel, and the foreground pixel for mono.                                    \
 */                                                                             \
                                                                                \
static void                                                                     \
testColours##bpp(uint##bpp##_t *data, int size, rfbBool *mono, rfbBool *solid,  \
                 uint##bpp##_t *bg, uint##bpp##_t *fg) {                        \
    uint##bpp##_t colour1 = 0, colour2 = 0;                                     \
    int n1 = 0, n2 = 0;                                                         \
    *mono = TRUE;                                                               \
    *solid = TRUE;                                                              \
                                                                                \
    for (; size > 0; size--, data++) {                                          \
                                                                                \
        if (n1 == 0)                                                            \
            colour1 = *data;                                                    \
                                                                                \
        if (*data == colour1) {                                                 \
            n1++;                                                               \
            continue;                                                           \
        }                                                                       \
                                                                                \
        if (n2 == 0) {                                                          \
            *solid = FALSE;                                                     \
            colour2 = *data;                                                    \
        }                                                                       \
                                                                                \
        if (*data == colour2) {                                                 \
            n2++;                                                               \
            continue;                                                           \
        }                                                                       \
                                                                                \
        *mono = FALSE;                                                          \
        break;                                                                  \
    }                                                                           \
                                                                                \
    if (n1 > n2) {                                                              \
        *bg = colour1;                                                          \
        *fg = colour2;                                                          \
    } else {                                                                    \
        *bg = colour2;                                                          \
        *fg = colour1;                                                          \
    }                                                                           \
}

DEFINE_SEND_HEXTILES(8)
DEFINE_SEND_HEXTILES(16)
DEFINE_SEND_HEXTILES(32)
