/*
 * tabletranstemplate.c - template for translation using lookup tables.
 *
 * This file shouldn't be compiled.  It is included multiple times by
 * translate.c, each time with different definitions of the macros IN and OUT.
 *
 * For each pair of values IN and OUT, this file defines two functions for
 * translating a given rectangle of pixel data.  One uses a single lookup
 * table, and the other uses three separate lookup tables for the red, green
 * and blue values.
 *
 * I know this code isn't nice to read because of all the macros, but
 * efficiency is important here.
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

#if !defined(BPP)
#error "This file shouldn't be compiled."
#error "It is included as part of translate.c"
#endif

#if BPP == 24

/*
 * rfbTranslateWithSingleTableINtoOUT translates a rectangle of pixel data
 * using a single lookup table.
 */

static void
rfbTranslateWithSingleTable24to24 (char *table, rfbPixelFormat *in,
                                    rfbPixelFormat *out,
                                    char *iptr, char *optr,
                                    int bytesBetweenInputLines,
                                    int width, int height)
{
    uint8_t *ip = (uint8_t *)iptr;
    uint8_t *op = (uint8_t *)optr;
    int ipextra = bytesBetweenInputLines - width * 3;
    uint8_t *opLineEnd;
    uint8_t *t = (uint8_t *)table;
    int shift = rfbEndianTest?0:8;
    uint8_t c;

    while (height > 0) {
        opLineEnd = op + width*3;

        while (op < opLineEnd) {
	    *(uint32_t*)op = t[((*(uint32_t *)ip)>>shift)&0x00ffffff];
	    if(!rfbEndianTest)
	      memmove(op,op+1,3);
	    if (out->bigEndian != in->bigEndian) {
	      c = op[0]; op[0] = op[2]; op[2] = c;
	    }
	    op += 3;
	    ip += 3;
        }

        ip += ipextra;
        height--;
    }
}

/*
 * rfbTranslateWithRGBTablesINtoOUT translates a rectangle of pixel data
 * using three separate lookup tables for the red, green and blue values.
 */

static void
rfbTranslateWithRGBTables24to24 (char *table, rfbPixelFormat *in,
                                  rfbPixelFormat *out,
                                  char *iptr, char *optr,
                                  int bytesBetweenInputLines,
                                  int width, int height)
{
    uint8_t *ip = (uint8_t *)iptr;
    uint8_t *op = (uint8_t *)optr;
    int ipextra = bytesBetweenInputLines - width*3;
    uint8_t *opLineEnd;
    uint8_t *redTable = (uint8_t *)table;
    uint8_t *greenTable = redTable + 3*(in->redMax + 1);
    uint8_t *blueTable = greenTable + 3*(in->greenMax + 1);
    uint32_t outValue,inValue;
    int shift = rfbEndianTest?0:8;

    while (height > 0) {
        opLineEnd = op+3*width;

        while (op < opLineEnd) {
	    inValue = ((*(uint32_t *)ip)>>shift)&0x00ffffff;
            outValue = (redTable[(inValue >> in->redShift) & in->redMax] |
                       greenTable[(inValue >> in->greenShift) & in->greenMax] |
                       blueTable[(inValue >> in->blueShift) & in->blueMax]);
	    memcpy(op,&outValue,3);
	    op += 3;
            ip+=3;
        }
        ip += ipextra;
        height--;
    }
}

#else

#define IN_T CONCAT3E(uint,BPP,_t)
#define OUT_T CONCAT3E(uint,BPP,_t)
#define rfbTranslateWithSingleTable24toOUT \
                                CONCAT4E(rfbTranslateWithSingleTable,24,to,BPP)
#define rfbTranslateWithSingleTableINto24 \
                                CONCAT4E(rfbTranslateWithSingleTable,BPP,to,24)
#define rfbTranslateWithRGBTables24toOUT \
                                CONCAT4E(rfbTranslateWithRGBTables,24,to,BPP)
#define rfbTranslateWithRGBTablesINto24 \
                                CONCAT4E(rfbTranslateWithRGBTables,BPP,to,24)

/*
 * rfbTranslateWithSingleTableINtoOUT translates a rectangle of pixel data
 * using a single lookup table.
 */

static void
rfbTranslateWithSingleTable24toOUT (char *table, rfbPixelFormat *in,
                                    rfbPixelFormat *out,
                                    char *iptr, char *optr,
                                    int bytesBetweenInputLines,
                                    int width, int height)
{
    uint8_t *ip = (uint8_t *)iptr;
    OUT_T *op = (OUT_T *)optr;
    int ipextra = bytesBetweenInputLines - width*3;
    OUT_T *opLineEnd;
    OUT_T *t = (OUT_T *)table;
    int shift = rfbEndianTest?0:8;

    while (height > 0) {
        opLineEnd = op + width;

        while (op < opLineEnd) {
            *(op++) = t[((*(uint32_t *)ip)>>shift)&0x00ffffff];
	    ip+=3;
        }

        ip += ipextra;
        height--;
    }
}


/*
 * rfbTranslateWithRGBTablesINtoOUT translates a rectangle of pixel data
 * using three separate lookup tables for the red, green and blue values.
 */

static void
rfbTranslateWithRGBTables24toOUT (char *table, rfbPixelFormat *in,
                                  rfbPixelFormat *out,
                                  char *iptr, char *optr,
                                  int bytesBetweenInputLines,
                                  int width, int height)
{
    uint8_t *ip = (uint8_t *)iptr;
    OUT_T *op = (OUT_T *)optr;
    int ipextra = bytesBetweenInputLines - width*3;
    OUT_T *opLineEnd;
    OUT_T *redTable = (OUT_T *)table;
    OUT_T *greenTable = redTable + in->redMax + 1;
    OUT_T *blueTable = greenTable + in->greenMax + 1;
    uint32_t inValue;
    int shift = rfbEndianTest?0:8;

    while (height > 0) {
        opLineEnd = &op[width];

        while (op < opLineEnd) {
	    inValue = ((*(uint32_t *)ip)>>shift)&0x00ffffff;
            *(op++) = (redTable[(inValue >> in->redShift) & in->redMax] |
                       greenTable[(inValue >> in->greenShift) & in->greenMax] |
                       blueTable[(inValue >> in->blueShift) & in->blueMax]);
            ip+=3;
        }
        ip += ipextra;
        height--;
    }
}

/*
 * rfbTranslateWithSingleTableINto24 translates a rectangle of pixel data
 * using a single lookup table.
 */

static void
rfbTranslateWithSingleTableINto24 (char *table, rfbPixelFormat *in,
                                    rfbPixelFormat *out,
                                    char *iptr, char *optr,
                                    int bytesBetweenInputLines,
                                    int width, int height)
{
    IN_T *ip = (IN_T *)iptr;
    uint8_t *op = (uint8_t *)optr;
    int ipextra = bytesBetweenInputLines / sizeof(IN_T) - width;
    uint8_t *opLineEnd;
    uint8_t *t = (uint8_t *)table;

    while (height > 0) {
        opLineEnd = op + width * 3;

        while (op < opLineEnd) {
	    memcpy(op,&t[3*(*(ip++))],3);
	    op += 3;
        }

        ip += ipextra;
        height--;
    }
}


/*
 * rfbTranslateWithRGBTablesINto24 translates a rectangle of pixel data
 * using three separate lookup tables for the red, green and blue values.
 */

static void
rfbTranslateWithRGBTablesINto24 (char *table, rfbPixelFormat *in,
                                  rfbPixelFormat *out,
                                  char *iptr, char *optr,
                                  int bytesBetweenInputLines,
                                  int width, int height)
{
    IN_T *ip = (IN_T *)iptr;
    uint8_t *op = (uint8_t *)optr;
    int ipextra = bytesBetweenInputLines / sizeof(IN_T) - width;
    uint8_t *opLineEnd;
    uint8_t *redTable = (uint8_t *)table;
    uint8_t *greenTable = redTable + 3*(in->redMax + 1);
    uint8_t *blueTable = greenTable + 3*(in->greenMax + 1);
    uint32_t outValue;

    while (height > 0) {
        opLineEnd = op+3*width;

        while (op < opLineEnd) {
            outValue = (redTable[(*ip >> in->redShift) & in->redMax] |
                       greenTable[(*ip >> in->greenShift) & in->greenMax] |
                       blueTable[(*ip >> in->blueShift) & in->blueMax]);
	    memcpy(op,&outValue,3);
	    op += 3;
            ip++;
        }
        ip += ipextra;
        height--;
    }
}

#undef IN_T
#undef OUT_T
#undef rfbTranslateWithSingleTable24toOUT
#undef rfbTranslateWithRGBTables24toOUT
#undef rfbTranslateWithSingleTableINto24
#undef rfbTranslateWithRGBTablesINto24

#endif
