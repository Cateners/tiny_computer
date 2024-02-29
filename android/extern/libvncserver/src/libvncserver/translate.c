/*
 * translate.c - translate between different pixel formats
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
#include <rfb/rfbregion.h>

static void PrintPixelFormat(rfbPixelFormat *pf);
static rfbBool rfbSetClientColourMapBGR233(rfbClientPtr cl);

rfbBool rfbEconomicTranslate = FALSE;

/*
 * Some standard pixel formats.
 */

static const rfbPixelFormat BGR233Format = {
    8, 8, 0, 1, 7, 7, 3, 0, 3, 6, 0, 0
};


/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)                                                      \
        ((x.bitsPerPixel == y.bitsPerPixel) &&                          \
         (x.depth == y.depth) &&                                        \
         ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&     \
         (!x.trueColour == !y.trueColour) &&                            \
         (!x.trueColour || ((x.redMax == y.redMax) &&                   \
                            (x.greenMax == y.greenMax) &&               \
                            (x.blueMax == y.blueMax) &&                 \
                            (x.redShift == y.redShift) &&               \
                            (x.greenShift == y.greenShift) &&           \
                            (x.blueShift == y.blueShift))))

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)
#define CONCAT3(a,b,c) a##b##c
#define CONCAT3E(a,b,c) CONCAT3(a,b,c)
#define CONCAT4(a,b,c,d) a##b##c##d
#define CONCAT4E(a,b,c,d) CONCAT4(a,b,c,d)

#undef OUT
#undef IN

#define OUT 8
#include "tableinitcmtemplate.c"
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

#define OUT 16
#include "tableinitcmtemplate.c"
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

#define OUT 32
#include "tableinitcmtemplate.c"
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

#ifdef LIBVNCSERVER_ALLOW24BPP
#define COUNT_OFFSETS 4
#define BPP2OFFSET(bpp) ((bpp)/8-1)
#include "tableinit24.c"
#define BPP 8
#include "tabletrans24template.c"
#undef BPP
#define BPP 16
#include "tabletrans24template.c"
#undef BPP
#define BPP 24
#include "tabletrans24template.c"
#undef BPP
#define BPP 32
#include "tabletrans24template.c"
#undef BPP
#else
#define COUNT_OFFSETS 3
#define BPP2OFFSET(bpp) ((int)(bpp)/16)
#endif

typedef void (*rfbInitCMTableFnType)(char **table, rfbPixelFormat *in,
                                   rfbPixelFormat *out,rfbColourMap* cm);
typedef void (*rfbInitTableFnType)(char **table, rfbPixelFormat *in,
                                   rfbPixelFormat *out);

static rfbInitCMTableFnType rfbInitColourMapSingleTableFns[COUNT_OFFSETS] = {
    rfbInitColourMapSingleTable8,
    rfbInitColourMapSingleTable16,
#ifdef LIBVNCSERVER_ALLOW24BPP
    rfbInitColourMapSingleTable24,
#endif
    rfbInitColourMapSingleTable32
};

static rfbInitTableFnType rfbInitTrueColourSingleTableFns[COUNT_OFFSETS] = {
    rfbInitTrueColourSingleTable8,
    rfbInitTrueColourSingleTable16,
#ifdef LIBVNCSERVER_ALLOW24BPP
    rfbInitTrueColourSingleTable24,
#endif
    rfbInitTrueColourSingleTable32
};

static rfbInitTableFnType rfbInitTrueColourRGBTablesFns[COUNT_OFFSETS] = {
    rfbInitTrueColourRGBTables8,
    rfbInitTrueColourRGBTables16,
#ifdef LIBVNCSERVER_ALLOW24BPP
    rfbInitTrueColourRGBTables24,
#endif
    rfbInitTrueColourRGBTables32
};

static rfbTranslateFnType rfbTranslateWithSingleTableFns[COUNT_OFFSETS][COUNT_OFFSETS] = {
    { rfbTranslateWithSingleTable8to8,
      rfbTranslateWithSingleTable8to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithSingleTable8to24,
#endif
      rfbTranslateWithSingleTable8to32 },
    { rfbTranslateWithSingleTable16to8,
      rfbTranslateWithSingleTable16to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithSingleTable16to24,
#endif
      rfbTranslateWithSingleTable16to32 },
#ifdef LIBVNCSERVER_ALLOW24BPP
    { rfbTranslateWithSingleTable24to8,
      rfbTranslateWithSingleTable24to16,
      rfbTranslateWithSingleTable24to24,
      rfbTranslateWithSingleTable24to32 },
#endif
    { rfbTranslateWithSingleTable32to8,
      rfbTranslateWithSingleTable32to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithSingleTable32to24,
#endif
      rfbTranslateWithSingleTable32to32 }
};

static rfbTranslateFnType rfbTranslateWithRGBTablesFns[COUNT_OFFSETS][COUNT_OFFSETS] = {
    { rfbTranslateWithRGBTables8to8,
      rfbTranslateWithRGBTables8to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithRGBTables8to24,
#endif
      rfbTranslateWithRGBTables8to32 },
    { rfbTranslateWithRGBTables16to8,
      rfbTranslateWithRGBTables16to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithRGBTables16to24,
#endif
      rfbTranslateWithRGBTables16to32 },
#ifdef LIBVNCSERVER_ALLOW24BPP
    { rfbTranslateWithRGBTables24to8,
      rfbTranslateWithRGBTables24to16,
      rfbTranslateWithRGBTables24to24,
      rfbTranslateWithRGBTables24to32 },
#endif
    { rfbTranslateWithRGBTables32to8,
      rfbTranslateWithRGBTables32to16,
#ifdef LIBVNCSERVER_ALLOW24BPP
      rfbTranslateWithRGBTables32to24,
#endif
      rfbTranslateWithRGBTables32to32 }
};



/*
 * rfbTranslateNone is used when no translation is required.
 */

void
rfbTranslateNone(char *table, rfbPixelFormat *in, rfbPixelFormat *out,
                 char *iptr, char *optr, int bytesBetweenInputLines,
                 int width, int height)
{
    int bytesPerOutputLine = width * (out->bitsPerPixel / 8);

    while (height > 0) {
        memcpy(optr, iptr, bytesPerOutputLine);
        iptr += bytesBetweenInputLines;
        optr += bytesPerOutputLine;
        height--;
    }
}


/*
 * rfbSetTranslateFunction sets the translation function.
 */

rfbBool
rfbSetTranslateFunction(rfbClientPtr cl)
{
    rfbLog("Pixel format for client %s:\n",cl->host);
    PrintPixelFormat(&cl->format);

    /*
     * Check that bits per pixel values are valid
     */

    if ((cl->screen->serverFormat.bitsPerPixel != 8) &&
        (cl->screen->serverFormat.bitsPerPixel != 16) &&
#ifdef LIBVNCSERVER_ALLOW24BPP
	(cl->screen->serverFormat.bitsPerPixel != 24) &&
#endif
        (cl->screen->serverFormat.bitsPerPixel != 32))
    {
        rfbErr("%s: server bits per pixel not 8, 16 or 32 (is %d)\n",
	       "rfbSetTranslateFunction", 
	       cl->screen->serverFormat.bitsPerPixel);
        rfbCloseClient(cl);
        return FALSE;
    }

    if ((cl->format.bitsPerPixel != 8) &&
        (cl->format.bitsPerPixel != 16) &&
#ifdef LIBVNCSERVER_ALLOW24BPP
	(cl->format.bitsPerPixel != 24) &&
#endif
        (cl->format.bitsPerPixel != 32))
    {
        rfbErr("%s: client bits per pixel not 8, 16 or 32\n",
                "rfbSetTranslateFunction");
        rfbCloseClient(cl);
        return FALSE;
    }

    if (!cl->format.trueColour && (cl->format.bitsPerPixel != 8)) {
        rfbErr("rfbSetTranslateFunction: client has colour map "
                "but %d-bit - can only cope with 8-bit colour maps\n",
                cl->format.bitsPerPixel);
        rfbCloseClient(cl);
        return FALSE;
    }

    /*
     * bpp is valid, now work out how to translate
     */

    if (!cl->format.trueColour) {
        /*
         * truecolour -> colour map
         *
         * Set client's colour map to BGR233, then effectively it's
         * truecolour as well
         */

        if (!rfbSetClientColourMapBGR233(cl))
            return FALSE;

        cl->format = BGR233Format;
    }

    /* truecolour -> truecolour */

    if (PF_EQ(cl->format,cl->screen->serverFormat)) {

        /* client & server the same */

        rfbLog("no translation needed\n");
        cl->translateFn = rfbTranslateNone;
        return TRUE;
    }

    if ((cl->screen->serverFormat.bitsPerPixel < 16) ||
        ((!cl->screen->serverFormat.trueColour || !rfbEconomicTranslate) &&
	   (cl->screen->serverFormat.bitsPerPixel == 16))) {

        /* we can use a single lookup table for <= 16 bpp */

        cl->translateFn = rfbTranslateWithSingleTableFns
                              [BPP2OFFSET(cl->screen->serverFormat.bitsPerPixel)]
                                  [BPP2OFFSET(cl->format.bitsPerPixel)];

	if(cl->screen->serverFormat.trueColour)
	  (*rfbInitTrueColourSingleTableFns
	   [BPP2OFFSET(cl->format.bitsPerPixel)]) (&cl->translateLookupTable,
						   &(cl->screen->serverFormat), &cl->format);
	else
	  (*rfbInitColourMapSingleTableFns
	   [BPP2OFFSET(cl->format.bitsPerPixel)]) (&cl->translateLookupTable,
						   &(cl->screen->serverFormat), &cl->format,&cl->screen->colourMap);

    } else {

        /* otherwise we use three separate tables for red, green and blue */

        cl->translateFn = rfbTranslateWithRGBTablesFns
                              [BPP2OFFSET(cl->screen->serverFormat.bitsPerPixel)]
                                  [BPP2OFFSET(cl->format.bitsPerPixel)];

        (*rfbInitTrueColourRGBTablesFns
            [BPP2OFFSET(cl->format.bitsPerPixel)]) (&cl->translateLookupTable,
                                             &(cl->screen->serverFormat), &cl->format);
    }

    return TRUE;
}



/*
 * rfbSetClientColourMapBGR233 sets the client's colour map so that it's
 * just like an 8-bit BGR233 true colour client.
 */

static rfbBool
rfbSetClientColourMapBGR233(rfbClientPtr cl)
{
    union {
        char bytes[sz_rfbSetColourMapEntriesMsg + 256 * 3 * 2];
        rfbSetColourMapEntriesMsg msg;
    } buf;
    rfbSetColourMapEntriesMsg *scme = &buf.msg;
    uint16_t *rgb = (uint16_t *)(&buf.bytes[sz_rfbSetColourMapEntriesMsg]);
    int i, len;
    int r, g, b;

    if (cl->format.bitsPerPixel != 8 ) {
        rfbErr("%s: client not 8 bits per pixel\n",
                "rfbSetClientColourMapBGR233");
        rfbCloseClient(cl);
        return FALSE;
    }
    
    scme->type = rfbSetColourMapEntries;

    scme->firstColour = Swap16IfLE(0);
    scme->nColours = Swap16IfLE(256);

    len = sz_rfbSetColourMapEntriesMsg;

    i = 0;

    for (b = 0; b < 4; b++) {
        for (g = 0; g < 8; g++) {
            for (r = 0; r < 8; r++) {
                rgb[i++] = Swap16IfLE(r * 65535 / 7);
                rgb[i++] = Swap16IfLE(g * 65535 / 7);
                rgb[i++] = Swap16IfLE(b * 65535 / 3);
            }
        }
    }

    len += 256 * 3 * 2;

    if (rfbWriteExact(cl, buf.bytes, len) < 0) {
        rfbLogPerror("rfbSetClientColourMapBGR233: write");
        rfbCloseClient(cl);
        return FALSE;
    }
    return TRUE;
}

/* this function is not called very often, so it needn't be
   efficient. */

/*
 * rfbSetClientColourMap is called to set the client's colour map.  If the
 * client is a true colour client, we simply update our own translation table
 * and mark the whole screen as having been modified.
 */

rfbBool
rfbSetClientColourMap(rfbClientPtr cl, int firstColour, int nColours)
{
    if (cl->screen->serverFormat.trueColour || !cl->readyForSetColourMapEntries) {
	return TRUE;
    }

    if (nColours == 0) {
	nColours = cl->screen->colourMap.count;
    }

    if (cl->format.trueColour) {
	LOCK(cl->updateMutex);
	(*rfbInitColourMapSingleTableFns
	    [BPP2OFFSET(cl->format.bitsPerPixel)]) (&cl->translateLookupTable,
					     &cl->screen->serverFormat, &cl->format,&cl->screen->colourMap);

	sraRgnDestroy(cl->modifiedRegion);
	cl->modifiedRegion =
	  sraRgnCreateRect(0,0,cl->screen->width,cl->screen->height);
	UNLOCK(cl->updateMutex);

	return TRUE;
    }

    return rfbSendSetColourMapEntries(cl, firstColour, nColours);
}


/*
 * rfbSetClientColourMaps sets the colour map for each RFB client.
 */

void
rfbSetClientColourMaps(rfbScreenInfoPtr rfbScreen, int firstColour, int nColours)
{
    rfbClientIteratorPtr i;
    rfbClientPtr cl;

    i = rfbGetClientIterator(rfbScreen);
    while((cl = rfbClientIteratorNext(i)))
      rfbSetClientColourMap(cl, firstColour, nColours);
    rfbReleaseClientIterator(i);
}

static void
PrintPixelFormat(rfbPixelFormat *pf)
{
    if (pf->bitsPerPixel == 1) {
        rfbLog("  1 bpp, %s sig bit in each byte is leftmost on the screen.\n",
               (pf->bigEndian ? "most" : "least"));
    } else {
        rfbLog("  %d bpp, depth %d%s\n",pf->bitsPerPixel,pf->depth,
               ((pf->bitsPerPixel == 8) ? ""
                : (pf->bigEndian ? ", big endian" : ", little endian")));
        if (pf->trueColour) {
            rfbLog("  true colour: max r %d g %d b %d, shift r %d g %d b %d\n",
                   pf->redMax, pf->greenMax, pf->blueMax,
                   pf->redShift, pf->greenShift, pf->blueShift);
        } else {
            rfbLog("  uses a colour map (not true colour).\n");
        }
    }
}
