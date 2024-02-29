/*
 * Copyright (C) 2002 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

/*
 * Before including this file, you must define a number of CPP macros.
 *
 * BPP should be 8, 16 or 32 depending on the bits per pixel.
 * GET_IMAGE_INTO_BUF should be some code which gets a rectangle of pixel data
 * into the given buffer.  EXTRA_ARGS can be defined to pass any other
 * arguments needed by GET_IMAGE_INTO_BUF.
 *
 * Note that the buf argument to ZRLE_ENCODE needs to be at least one pixel
 * bigger than the largest tile of pixel data, since the ZRLE encoding
 * algorithm writes to the position one past the end of the pixel data.
 */

#include "zrleoutstream.h"
#include "zrlepalettehelper.h"
#include <assert.h>

/* __RFB_CONCAT2 concatenates its two arguments.  __RFB_CONCAT2E does the same
   but also expands its arguments if they are macros */

#ifndef __RFB_CONCAT2E
#define __RFB_CONCAT2(a,b) a##b
#define __RFB_CONCAT2E(a,b) __RFB_CONCAT2(a,b)
#endif

#ifndef __RFB_CONCAT3E
#define __RFB_CONCAT3(a,b,c) a##b##c
#define __RFB_CONCAT3E(a,b,c) __RFB_CONCAT3(a,b,c)
#endif

#undef END_FIX
#if ZYWRLE_ENDIAN == ENDIAN_LITTLE
#  define END_FIX LE
#elif ZYWRLE_ENDIAN == ENDIAN_BIG
#  define END_FIX BE
#else
#  define END_FIX NE
#endif

#ifdef CPIXEL
#define PIXEL_T __RFB_CONCAT2E(zrle_U,BPP)
#define zrleOutStreamWRITE_PIXEL __RFB_CONCAT2E(zrleOutStreamWriteOpaque,CPIXEL)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode,CPIXEL,END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile,CPIXEL,END_FIX)
#define BPPOUT 24
#elif BPP==15
#define PIXEL_T __RFB_CONCAT2E(zrle_U,16)
#define zrleOutStreamWRITE_PIXEL __RFB_CONCAT2E(zrleOutStreamWriteOpaque,16)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode,BPP,END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile,BPP,END_FIX)
#define BPPOUT 16
#else
#define PIXEL_T __RFB_CONCAT2E(zrle_U,BPP)
#define zrleOutStreamWRITE_PIXEL __RFB_CONCAT2E(zrleOutStreamWriteOpaque,BPP)
#define ZRLE_ENCODE __RFB_CONCAT3E(zrleEncode,BPP,END_FIX)
#define ZRLE_ENCODE_TILE __RFB_CONCAT3E(zrleEncodeTile,BPP,END_FIX)
#define BPPOUT BPP
#endif

#ifndef ZRLE_ONCE
#define ZRLE_ONCE

static const int bitsPerPackedPixel[] = {
  0, 1, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

#endif /* ZRLE_ONCE */

void ZRLE_ENCODE_TILE (PIXEL_T* data, int w, int h, zrleOutStream* os,
		int zywrle_level, int *zywrleBuf, void *paletteHelper);

#if BPP!=8
#define ZYWRLE_ENCODE
#include "zywrletemplate.c"
#endif

static void ZRLE_ENCODE (int x, int y, int w, int h,
		  zrleOutStream* os, void* buf
                  EXTRA_ARGS
                  )
{
  int ty;
  for (ty = y; ty < y+h; ty += rfbZRLETileHeight) {
    int tx, th = rfbZRLETileHeight;
    if (th > y+h-ty) th = y+h-ty;
    for (tx = x; tx < x+w; tx += rfbZRLETileWidth) {
      int tw = rfbZRLETileWidth;
      if (tw > x+w-tx) tw = x+w-tx;

      GET_IMAGE_INTO_BUF(tx,ty,tw,th,buf);

      if (cl->paletteHelper == NULL) {
          cl->paletteHelper = (void *) calloc(sizeof(zrlePaletteHelper), 1);
      }

      ZRLE_ENCODE_TILE((PIXEL_T*)buf, tw, th, os,
		      cl->zywrleLevel, cl->zywrleBuf, cl->paletteHelper);
    }
  }
  zrleOutStreamFlush(os);
}


void ZRLE_ENCODE_TILE(PIXEL_T* data, int w, int h, zrleOutStream* os,
	int zywrle_level, int *zywrleBuf,  void *paletteHelper)
{
  /* First find the palette and the number of runs */

  zrlePaletteHelper *ph;

  int runs = 0;
  int singlePixels = 0;

  rfbBool useRle;
  rfbBool usePalette;

  int estimatedBytes;
  int plainRleBytes;
  int i;

  PIXEL_T* ptr = data;
  PIXEL_T* end = ptr + h * w;
  *end = ~*(end-1); /* one past the end is different so the while loop ends */

  ph = (zrlePaletteHelper *) paletteHelper;
  zrlePaletteHelperInit(ph);

  while (ptr < end) {
    PIXEL_T pix = *ptr;
    if (*++ptr != pix) {
      singlePixels++;
    } else {
      while (*++ptr == pix) ;
      runs++;
    }
    zrlePaletteHelperInsert(ph, pix);
  }

  /* Solid tile is a special case */

  if (ph->size == 1) {
    zrleOutStreamWriteU8(os, 1);
    zrleOutStreamWRITE_PIXEL(os, ph->palette[0]);
    return;
  }

  /* Try to work out whether to use RLE and/or a palette.  We do this by
     estimating the number of bytes which will be generated and picking the
     method which results in the fewest bytes.  Of course this may not result
     in the fewest bytes after compression... */

  useRle = FALSE;
  usePalette = FALSE;

  estimatedBytes = w * h * (BPPOUT/8); /* start assuming raw */

#if BPP!=8
  if (zywrle_level > 0 && !(zywrle_level & 0x80))
	  estimatedBytes >>= zywrle_level;
#endif

  plainRleBytes = ((BPPOUT/8)+1) * (runs + singlePixels);

  if (plainRleBytes < estimatedBytes) {
    useRle = TRUE;
    estimatedBytes = plainRleBytes;
  }

  if (ph->size < 128) {
    int paletteRleBytes = (BPPOUT/8) * ph->size + 2 * runs + singlePixels;

    if (paletteRleBytes < estimatedBytes) {
      useRle = TRUE;
      usePalette = TRUE;
      estimatedBytes = paletteRleBytes;
    }

    if (ph->size < 17) {
      int packedBytes = ((BPPOUT/8) * ph->size +
                         w * h * bitsPerPackedPixel[ph->size-1] / 8);

      if (packedBytes < estimatedBytes) {
        useRle = FALSE;
        usePalette = TRUE;
        estimatedBytes = packedBytes;
      }
    }
  }

  if (!usePalette) ph->size = 0;

  zrleOutStreamWriteU8(os, (useRle ? 128 : 0) | ph->size);

  for (i = 0; i < ph->size; i++) {
    zrleOutStreamWRITE_PIXEL(os, ph->palette[i]);
  }

  if (useRle) {

    PIXEL_T* ptr = data;
    PIXEL_T* end = ptr + w * h;
    PIXEL_T* runStart;
    PIXEL_T pix;
    while (ptr < end) {
      int len;
      runStart = ptr;
      pix = *ptr++;
      while (*ptr == pix && ptr < end)
        ptr++;
      len = ptr - runStart;
      if (len <= 2 && usePalette) {
        int index = zrlePaletteHelperLookup(ph, pix);
        if (len == 2)
          zrleOutStreamWriteU8(os, index);
        zrleOutStreamWriteU8(os, index);
        continue;
      }
      if (usePalette) {
        int index = zrlePaletteHelperLookup(ph, pix);
        zrleOutStreamWriteU8(os, index | 128);
      } else {
        zrleOutStreamWRITE_PIXEL(os, pix);
      }
      len -= 1;
      while (len >= 255) {
        zrleOutStreamWriteU8(os, 255);
        len -= 255;
      }
      zrleOutStreamWriteU8(os, len);
    }

  } else {

    /* no RLE */

    if (usePalette) {
      int bppp;
      PIXEL_T* ptr = data;

      /* packed pixels */

      assert (ph->size < 17);

      bppp = bitsPerPackedPixel[ph->size-1];

      for (i = 0; i < h; i++) {
        zrle_U8 nbits = 0;
        zrle_U8 byte = 0;

        PIXEL_T* eol = ptr + w;

        while (ptr < eol) {
          PIXEL_T pix = *ptr++;
          zrle_U8 index = zrlePaletteHelperLookup(ph, pix);
          byte = (byte << bppp) | index;
          nbits += bppp;
          if (nbits >= 8) {
            zrleOutStreamWriteU8(os, byte);
            nbits = 0;
          }
        }
        if (nbits > 0) {
          byte <<= 8 - nbits;
          zrleOutStreamWriteU8(os, byte);
        }
      }
    } else {

      /* raw */

#if BPP!=8
      if (zywrle_level > 0 && !(zywrle_level & 0x80)) {
        ZYWRLE_ANALYZE(data, data, w, h, w, zywrle_level, zywrleBuf);
	ZRLE_ENCODE_TILE(data, w, h, os, zywrle_level | 0x80, zywrleBuf, paletteHelper);
      }
      else
#endif
      {
#ifdef CPIXEL
        PIXEL_T *ptr;
        for (ptr = data; ptr < data+w*h; ptr++)
          zrleOutStreamWRITE_PIXEL(os, *ptr);
#else
        zrleOutStreamWriteBytes(os, (zrle_U8 *)data, w*h*(BPP/8));
#endif
      }
    }
  }
}

#undef PIXEL_T
#undef zrleOutStreamWRITE_PIXEL
#undef ZRLE_ENCODE
#undef ZRLE_ENCODE_TILE
#undef ZYWRLE_ENCODE_TILE
#undef BPPOUT
