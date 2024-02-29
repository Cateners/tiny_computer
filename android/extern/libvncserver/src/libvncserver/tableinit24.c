/*
  24 bit
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

static void
rfbInitOneRGBTable24 (uint8_t *table, int inMax, int outMax, int outShift,int swap);


static void
rfbInitColourMapSingleTable24(char **table, rfbPixelFormat *in,
                            rfbPixelFormat *out,rfbColourMap* colourMap)
{
    uint32_t i, r, g, b, outValue;
    uint8_t *t;
    uint8_t c;
    unsigned int nEntries = 1 << in->bitsPerPixel;
    int shift = colourMap->is16?16:8;

    if (*table) free(*table);
    *table = (char *)malloc(nEntries * 3 + 1);
    t = (uint8_t *)*table;

    for (i = 0; i < nEntries; i++) {
        r = g = b = 0;
	if(i < colourMap->count) {
	  if(colourMap->is16) {
	    r = colourMap->data.shorts[3*i+0];
	    g = colourMap->data.shorts[3*i+1];
	    b = colourMap->data.shorts[3*i+2];
	  } else {
	    r = colourMap->data.bytes[3*i+0];
	    g = colourMap->data.bytes[3*i+1];
	    b = colourMap->data.bytes[3*i+2];
	  }
	}
        outValue = ((((r * (1 + out->redMax)) >> shift) << out->redShift) |
                (((g * (1 + out->greenMax)) >> shift) << out->greenShift) |
                (((b * (1 + out->blueMax)) >> shift) << out->blueShift));
	*(uint32_t*)&t[3*i] = outValue;
	if(!rfbEndianTest)
	  memmove(t+3*i,t+3*i+1,3);
        if (out->bigEndian != in->bigEndian) {
	  c = t[3*i]; t[3*i] = t[3*i+2]; t[3*i+2] = c;
        }
    }
}

/*
 * rfbInitTrueColourSingleTable sets up a single lookup table for truecolour
 * translation.
 */

static void
rfbInitTrueColourSingleTable24 (char **table, rfbPixelFormat *in,
                                 rfbPixelFormat *out)
{
    int i,outValue;
    int inRed, inGreen, inBlue, outRed, outGreen, outBlue;
    uint8_t *t;
    uint8_t c;
    int nEntries = 1 << in->bitsPerPixel;

    if (*table) free(*table);
    *table = (char *)malloc(nEntries * 3 + 1);
    t = (uint8_t *)*table;

    for (i = 0; i < nEntries; i++) {
        inRed   = (i >> in->redShift)   & in->redMax;
        inGreen = (i >> in->greenShift) & in->greenMax;
        inBlue  = (i >> in->blueShift)  & in->blueMax;

        outRed   = (inRed   * out->redMax   + in->redMax / 2)   / in->redMax;
        outGreen = (inGreen * out->greenMax + in->greenMax / 2) / in->greenMax;
        outBlue  = (inBlue  * out->blueMax  + in->blueMax / 2)  / in->blueMax;

	outValue = ((outRed   << out->redShift)   |
                (outGreen << out->greenShift) |
                (outBlue  << out->blueShift));
	*(uint32_t*)&t[3*i] = outValue;
	if(!rfbEndianTest)
	  memmove(t+3*i,t+3*i+1,3);
        if (out->bigEndian != in->bigEndian) {
	  c = t[3*i]; t[3*i] = t[3*i+2]; t[3*i+2] = c;
        }
    }
}


/*
 * rfbInitTrueColourRGBTables sets up three separate lookup tables for the
 * red, green and blue values.
 */

static void
rfbInitTrueColourRGBTables24 (char **table, rfbPixelFormat *in,
                               rfbPixelFormat *out)
{
    uint8_t *redTable;
    uint8_t *greenTable;
    uint8_t *blueTable;

    if (*table) free(*table);
    *table = (char *)malloc((in->redMax + in->greenMax + in->blueMax + 3)
                            * 3 + 1);
    redTable = (uint8_t *)*table;
    greenTable = redTable + 3*(in->redMax + 1);
    blueTable = greenTable + 3*(in->greenMax + 1);

    rfbInitOneRGBTable24 (redTable, in->redMax, out->redMax,
                           out->redShift, (out->bigEndian != in->bigEndian));
    rfbInitOneRGBTable24 (greenTable, in->greenMax, out->greenMax,
                           out->greenShift, (out->bigEndian != in->bigEndian));
    rfbInitOneRGBTable24 (blueTable, in->blueMax, out->blueMax,
                           out->blueShift, (out->bigEndian != in->bigEndian));
}

static void
rfbInitOneRGBTable24 (uint8_t *table, int inMax, int outMax, int outShift,
                       int swap)
{
    int i;
    int nEntries = inMax + 1;
    uint32_t outValue;
    uint8_t c;

    for (i = 0; i < nEntries; i++) {
      outValue = ((i * outMax + inMax / 2) / inMax) << outShift;
      *(uint32_t *)&table[3*i] = outValue;
      if(!rfbEndianTest) {
	memmove(table+3*i,table+3*i+1,3);
      }
      if (swap) {
        c = table[3*i]; table[3*i] = table[3*i+2];
        table[3*i+2] = c;
      }
    }
}
