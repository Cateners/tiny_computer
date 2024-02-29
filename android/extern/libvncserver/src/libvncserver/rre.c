/*
 * rre.c
 *
 * Routines to implement Rise-and-Run-length Encoding (RRE).  This
 * code is based on krw's original javatel rfbserver.
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

/*
 * cl->beforeEncBuf contains pixel data in the client's format.
 * cl->afterEncBuf contains the RRE encoded version.  If the RRE encoded version is
 * larger than the raw data or if it exceeds cl->afterEncBufSize then
 * raw encoding is used instead.
 */

static int subrectEncode8(rfbClientPtr cl, uint8_t *data, int w, int h);
static int subrectEncode16(rfbClientPtr cl, uint16_t *data, int w, int h);
static int subrectEncode32(rfbClientPtr cl, uint32_t *data, int w, int h);
static uint32_t getBgColour(char *data, int size, int bpp);


/*
 * rfbSendRectEncodingRRE - send a given rectangle using RRE encoding.
 */

rfbBool
rfbSendRectEncodingRRE(rfbClientPtr cl,
                       int x,
                       int y,
                       int w,
                       int h)
{
    rfbFramebufferUpdateRectHeader rect;
    rfbRREHeader hdr;
    int nSubrects;
    int i;
    char *fbptr = (cl->scaledScreen->frameBuffer + (cl->scaledScreen->paddedWidthInBytes * y)
                   + (x * (cl->scaledScreen->bitsPerPixel / 8)));

    int maxRawSize = (cl->scaledScreen->width * cl->scaledScreen->height
                      * (cl->format.bitsPerPixel / 8));

    if (!cl->beforeEncBuf || cl->beforeEncBufSize < maxRawSize) {
        if (cl->beforeEncBuf == NULL)
            cl->beforeEncBuf = (char *)malloc(maxRawSize);
        else {
            char *reallocedBeforeEncBuf = (char *)realloc(cl->beforeEncBuf, maxRawSize);
            if (!reallocedBeforeEncBuf) return FALSE;
            cl->beforeEncBuf = reallocedBeforeEncBuf;
        }
        if(cl->beforeEncBuf)
            cl->beforeEncBufSize = maxRawSize;
    }

    if (!cl->afterEncBuf || cl->afterEncBufSize < maxRawSize) {
        if (cl->afterEncBuf == NULL)
            cl->afterEncBuf = (char *)malloc(maxRawSize);
        else {
            char *reallocedAfterEncBuf = (char *)realloc(cl->afterEncBuf, maxRawSize);
            if (!reallocedAfterEncBuf) return FALSE;
            cl->afterEncBuf = reallocedAfterEncBuf;
        }
        if(cl->afterEncBuf)
            cl->afterEncBufSize = maxRawSize;
    }

    if (!cl->beforeEncBuf || !cl->afterEncBuf)
    {
        rfbLog("rfbSendRectEncodingRRE: failed to allocate memory\n");
        return FALSE;
    }

    (*cl->translateFn)(cl->translateLookupTable,
		       &(cl->screen->serverFormat),
                       &cl->format, fbptr, cl->beforeEncBuf,
                       cl->scaledScreen->paddedWidthInBytes, w, h);

    switch (cl->format.bitsPerPixel) {
    case 8:
        nSubrects = subrectEncode8(cl, (uint8_t *)cl->beforeEncBuf, w, h);
        break;
    case 16:
        nSubrects = subrectEncode16(cl, (uint16_t *)cl->beforeEncBuf, w, h);
        break;
    case 32:
        nSubrects = subrectEncode32(cl, (uint32_t *)cl->beforeEncBuf, w, h);
        break;
    default:
        rfbLog("getBgColour: bpp %d?\n",cl->format.bitsPerPixel);
        return FALSE;
    }
        
    if (nSubrects < 0) {

        /* RRE encoding was too large, use raw */

        return rfbSendRectEncodingRaw(cl, x, y, w, h);
    }

    rfbStatRecordEncodingSent(cl, rfbEncodingRRE,
                              sz_rfbFramebufferUpdateRectHeader + sz_rfbRREHeader + cl->afterEncBufLen,
                              sz_rfbFramebufferUpdateRectHeader + w * h * (cl->format.bitsPerPixel / 8));

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbRREHeader
        > UPDATE_BUF_SIZE)
    {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingRRE);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
           sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    hdr.nSubrects = Swap32IfLE(nSubrects);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&hdr, sz_rfbRREHeader);
    cl->ublen += sz_rfbRREHeader;

    for (i = 0; i < cl->afterEncBufLen;) {

        int bytesToCopy = UPDATE_BUF_SIZE - cl->ublen;

        if (i + bytesToCopy > cl->afterEncBufLen) {
            bytesToCopy = cl->afterEncBufLen - i;
        }

        memcpy(&cl->updateBuf[cl->ublen], &cl->afterEncBuf[i], bytesToCopy);

        cl->ublen += bytesToCopy;
        i += bytesToCopy;

        if (cl->ublen == UPDATE_BUF_SIZE) {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }
    }

    return TRUE;
}



/*
 * subrectEncode() encodes the given multicoloured rectangle as a background 
 * colour overwritten by single-coloured rectangles.  It returns the number 
 * of subrectangles in the encoded buffer, or -1 if subrect encoding won't
 * fit in the buffer.  It puts the encoded rectangles in cl->afterEncBuf.  The
 * single-colour rectangle partition is not optimal, but does find the biggest
 * horizontal or vertical rectangle top-left anchored to each consecutive 
 * coordinate position.
 *
 * The coding scheme is simply [<bgcolour><subrect><subrect>...] where each 
 * <subrect> is [<colour><x><y><w><h>].
 */

#define DEFINE_SUBRECT_ENCODE(bpp)                                            \
static int                                                                    \
 subrectEncode##bpp(rfbClientPtr client, uint##bpp##_t *data, int w, int h) { \
    uint##bpp##_t cl;                                                         \
    rfbRectangle subrect;                                                     \
    int x,y;                                                                  \
    int i,j;                                                                  \
    int hx=0,hy,vx=0,vy;                                                      \
    int hyflag;                                                               \
    uint##bpp##_t *seg;                                                       \
    uint##bpp##_t *line;                                                      \
    int hw,hh,vw,vh;                                                          \
    int thex,they,thew,theh;                                                  \
    int numsubs = 0;                                                          \
    int newLen;                                                               \
    uint##bpp##_t bg = (uint##bpp##_t)getBgColour((char*)data,w*h,bpp);       \
                                                                              \
    *((uint##bpp##_t*)client->afterEncBuf) = bg;                                      \
                                                                              \
    client->afterEncBufLen = (bpp/8);                                                 \
                                                                              \
    for (y=0; y<h; y++) {                                                     \
      line = data+(y*w);                                                      \
      for (x=0; x<w; x++) {                                                   \
        if (line[x] != bg) {                                                  \
          cl = line[x];                                                       \
          hy = y-1;                                                           \
          hyflag = 1;                                                         \
          for (j=y; j<h; j++) {                                               \
            seg = data+(j*w);                                                 \
            if (seg[x] != cl) {break;}                                        \
            i = x;                                                            \
            while ((i < w) && (seg[i] == cl)) i += 1;                         \
            i -= 1;                                                           \
            if (j == y) vx = hx = i;                                          \
            if (i < vx) vx = i;                                               \
            if ((hyflag > 0) && (i >= hx)) {hy += 1;} else {hyflag = 0;}      \
          }                                                                   \
          vy = j-1;                                                           \
                                                                              \
          /*  We now have two possible subrects: (x,y,hx,hy) and (x,y,vx,vy)  \
           *  We'll choose the bigger of the two.                             \
           */                                                                 \
          hw = hx-x+1;                                                        \
          hh = hy-y+1;                                                        \
          vw = vx-x+1;                                                        \
          vh = vy-y+1;                                                        \
                                                                              \
          thex = x;                                                           \
          they = y;                                                           \
                                                                              \
          if ((hw*hh) > (vw*vh)) {                                            \
            thew = hw;                                                        \
            theh = hh;                                                        \
          } else {                                                            \
            thew = vw;                                                        \
            theh = vh;                                                        \
          }                                                                   \
                                                                              \
          subrect.x = Swap16IfLE(thex);                                       \
          subrect.y = Swap16IfLE(they);                                       \
          subrect.w = Swap16IfLE(thew);                                       \
          subrect.h = Swap16IfLE(theh);                                       \
                                                                              \
          newLen = client->afterEncBufLen + (bpp/8) + sz_rfbRectangle;                \
          if ((newLen > (w * h * (bpp/8))) || (newLen > client->afterEncBufSize))     \
            return -1;                                                        \
                                                                              \
          numsubs += 1;                                                       \
          *((uint##bpp##_t*)(client->afterEncBuf + client->afterEncBufLen)) = cl;             \
          client->afterEncBufLen += (bpp/8);                                          \
          memcpy(&client->afterEncBuf[client->afterEncBufLen],&subrect,sz_rfbRectangle);      \
          client->afterEncBufLen += sz_rfbRectangle;                                  \
                                                                              \
          /*                                                                  \
           * Now mark the subrect as done.                                    \
           */                                                                 \
          for (j=they; j < (they+theh); j++) {                                \
            for (i=thex; i < (thex+thew); i++) {                              \
              data[j*w+i] = bg;                                               \
            }                                                                 \
          }                                                                   \
        }                                                                     \
      }                                                                       \
    }                                                                         \
                                                                              \
    return numsubs;                                                           \
}

DEFINE_SUBRECT_ENCODE(8)
DEFINE_SUBRECT_ENCODE(16)
DEFINE_SUBRECT_ENCODE(32)


/*
 * getBgColour() gets the most prevalent colour in a byte array.
 */
static uint32_t
getBgColour(char *data, int size, int bpp)
{
    
#define NUMCLRS 256
  
  static int counts[NUMCLRS];
  int i,j,k;

  int maxcount = 0;
  uint8_t maxclr = 0;

  if (bpp != 8) {
    if (bpp == 16) {
      return ((uint16_t *)data)[0];
    } else if (bpp == 32) {
      return ((uint32_t *)data)[0];
    } else {
      rfbLog("getBgColour: bpp %d?\n",bpp);
      return 0;
    }
  }

  for (i=0; i<NUMCLRS; i++) {
    counts[i] = 0;
  }

  for (j=0; j<size; j++) {
    k = (int)(((uint8_t *)data)[j]);
#if NUMCLRS != 256
    if (k >= NUMCLRS) {
      rfbErr("getBgColour: unusual colour = %d\n", k);
      return 0;
    }
#endif
    counts[k] += 1;
    if (counts[k] > maxcount) {
      maxcount = counts[k];
      maxclr = ((uint8_t *)data)[j];
    }
  }
  
  return maxclr;
}
