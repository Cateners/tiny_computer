/*
 * cursor.c - support for cursor shape updates.
 */

/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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
#include "private.h"

void rfbScaledScreenUpdate(rfbScreenInfoPtr screen, int x1, int y1, int x2, int y2);

/*
 * Send cursor shape either in X-style format or in client pixel format.
 */

rfbBool
rfbSendCursorShape(rfbClientPtr cl)
{
    rfbCursorPtr pCursor;
    rfbFramebufferUpdateRectHeader rect;
    rfbXCursorColors colors;
    int saved_ublen;
    int bitmapRowBytes, maskBytes, dataBytes;
    int i, j;
    uint8_t *bitmapData;
    uint8_t bitmapByte;

    /* TODO: scale the cursor data to the correct size */

    pCursor = cl->screen->getCursorPtr(cl);
    /*if(!pCursor) return TRUE;*/

    if (cl->useRichCursorEncoding) {
      if(pCursor && !pCursor->richSource)
	rfbMakeRichCursorFromXCursor(cl->screen,pCursor);
      rect.encoding = Swap32IfLE(rfbEncodingRichCursor);
    } else {
       if(pCursor && !pCursor->source)
	 rfbMakeXCursorFromRichCursor(cl->screen,pCursor);
       rect.encoding = Swap32IfLE(rfbEncodingXCursor);
    }

    /* If there is no cursor, send update with empty cursor data. */

    if ( pCursor && pCursor->width == 1 &&
	 pCursor->height == 1 &&
	 pCursor->mask[0] == 0 ) {
	pCursor = NULL;
    }

    if (pCursor == NULL) {
	if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE ) {
	    if (!rfbSendUpdateBuf(cl))
		return FALSE;
	}
	rect.r.x = rect.r.y = 0;
	rect.r.w = rect.r.h = 0;
	memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	       sz_rfbFramebufferUpdateRectHeader);
	cl->ublen += sz_rfbFramebufferUpdateRectHeader;

	if (!rfbSendUpdateBuf(cl))
	    return FALSE;

	return TRUE;
    }

    /* Calculate data sizes. */

    bitmapRowBytes = (pCursor->width + 7) / 8;
    maskBytes = bitmapRowBytes * pCursor->height;
    dataBytes = (cl->useRichCursorEncoding) ?
	(pCursor->width * pCursor->height *
	 (cl->format.bitsPerPixel / 8)) : maskBytes;

    /* Send buffer contents if needed. */

    if ( cl->ublen + sz_rfbFramebufferUpdateRectHeader +
	 sz_rfbXCursorColors + maskBytes + dataBytes > UPDATE_BUF_SIZE ) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    if ( cl->ublen + sz_rfbFramebufferUpdateRectHeader +
	 sz_rfbXCursorColors + maskBytes + dataBytes > UPDATE_BUF_SIZE ) {
	return FALSE;		/* FIXME. */
    }

    saved_ublen = cl->ublen;

    /* Prepare rectangle header. */

    rect.r.x = Swap16IfLE(pCursor->xhot);
    rect.r.y = Swap16IfLE(pCursor->yhot);
    rect.r.w = Swap16IfLE(pCursor->width);
    rect.r.h = Swap16IfLE(pCursor->height);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    /* Prepare actual cursor data (depends on encoding used). */

    if (!cl->useRichCursorEncoding) {
	/* XCursor encoding. */
	colors.foreRed   = (char)(pCursor->foreRed   >> 8);
	colors.foreGreen = (char)(pCursor->foreGreen >> 8);
	colors.foreBlue  = (char)(pCursor->foreBlue  >> 8);
	colors.backRed   = (char)(pCursor->backRed   >> 8);
	colors.backGreen = (char)(pCursor->backGreen >> 8);
	colors.backBlue  = (char)(pCursor->backBlue  >> 8);

	memcpy(&cl->updateBuf[cl->ublen], (char *)&colors, sz_rfbXCursorColors);
	cl->ublen += sz_rfbXCursorColors;

	bitmapData = (uint8_t *)pCursor->source;

	for (i = 0; i < pCursor->height; i++) {
	    for (j = 0; j < bitmapRowBytes; j++) {
		bitmapByte = bitmapData[i * bitmapRowBytes + j];
		cl->updateBuf[cl->ublen++] = (char)bitmapByte;
	    }
	}
    } else {
	/* RichCursor encoding. */
       int bpp1=cl->screen->serverFormat.bitsPerPixel/8,
	 bpp2=cl->format.bitsPerPixel/8;
       (*cl->translateFn)(cl->translateLookupTable,
		       &(cl->screen->serverFormat),
                       &cl->format, (char*)pCursor->richSource,
		       &cl->updateBuf[cl->ublen],
                       pCursor->width*bpp1, pCursor->width, pCursor->height);

       cl->ublen += pCursor->width*bpp2*pCursor->height;
    }

    /* Prepare transparency mask. */

    bitmapData = (uint8_t *)pCursor->mask;

    for (i = 0; i < pCursor->height; i++) {
	for (j = 0; j < bitmapRowBytes; j++) {
	    bitmapByte = bitmapData[i * bitmapRowBytes + j];
	    cl->updateBuf[cl->ublen++] = (char)bitmapByte;
	}
    }

    /* Send everything we have prepared in the cl->updateBuf[]. */
    rfbStatRecordEncodingSent(cl, (cl->useRichCursorEncoding ? rfbEncodingRichCursor : rfbEncodingXCursor), 
        sz_rfbFramebufferUpdateRectHeader + (cl->ublen - saved_ublen), sz_rfbFramebufferUpdateRectHeader + (cl->ublen - saved_ublen));

    if (!rfbSendUpdateBuf(cl))
	return FALSE;

    return TRUE;
}

/*
 * Send cursor position (PointerPos pseudo-encoding).
 */

rfbBool
rfbSendCursorPos(rfbClientPtr cl)
{
  rfbFramebufferUpdateRectHeader rect;

  if (cl->ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
    if (!rfbSendUpdateBuf(cl))
      return FALSE;
  }

  rect.encoding = Swap32IfLE(rfbEncodingPointerPos);
  rect.r.x = Swap16IfLE(cl->screen->cursorX);
  rect.r.y = Swap16IfLE(cl->screen->cursorY);
  rect.r.w = 0;
  rect.r.h = 0;

  memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	 sz_rfbFramebufferUpdateRectHeader);
  cl->ublen += sz_rfbFramebufferUpdateRectHeader;

  rfbStatRecordEncodingSent(cl, rfbEncodingPointerPos, sz_rfbFramebufferUpdateRectHeader, sz_rfbFramebufferUpdateRectHeader);

  if (!rfbSendUpdateBuf(cl))
    return FALSE;

  return TRUE;
}

/* conversion routine for predefined cursors in LSB order */
unsigned char rfbReverseByte[0x100] = {
  /* copied from Xvnc/lib/font/util/utilbitmap.c */
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

void rfbConvertLSBCursorBitmapOrMask(int width,int height,unsigned char* bitmap)
{
   int i,t=(width+7)/8*height;
   for(i=0;i<t;i++)
     bitmap[i]=rfbReverseByte[(int)bitmap[i]];
}

/* Cursor creation. You "paint" a cursor and let these routines do the work */

rfbCursorPtr rfbMakeXCursor(int width,int height,char* cursorString,char* maskString)
{
   int i,j,w=(width+7)/8;
   rfbCursorPtr cursor = (rfbCursorPtr)calloc(1,sizeof(rfbCursor));
   char* cp;
   unsigned char bit;

   if (!cursor)
       return NULL;

   cursor->cleanup=TRUE;
   cursor->width=width;
   cursor->height=height;
   /*cursor->backRed=cursor->backGreen=cursor->backBlue=0xffff;*/
   cursor->foreRed=cursor->foreGreen=cursor->foreBlue=0xffff;
   
   cursor->source = (unsigned char*)calloc(w,height);
   if (!cursor->source) {
       free(cursor);
       return NULL;
   }
   cursor->cleanupSource = TRUE;
   for(j=0,cp=cursorString;j<height;j++)
      for(i=0,bit=0x80;i<width;i++,bit=(bit&1)?0x80:bit>>1,cp++)
	if(*cp!=' ') cursor->source[j*w+i/8]|=bit;

   if(maskString) {
      cursor->mask = (unsigned char*)calloc(w,height);
      if (!cursor->mask) {
          free(cursor->source);
          free(cursor);
          return NULL;
      }
      for(j=0,cp=maskString;j<height;j++)
	for(i=0,bit=0x80;i<width;i++,bit=(bit&1)?0x80:bit>>1,cp++)
	  if(*cp!=' ') cursor->mask[j*w+i/8]|=bit;
   } else
     cursor->mask = (unsigned char*)rfbMakeMaskForXCursor(width,height,(char*)cursor->source);
   cursor->cleanupMask = TRUE;

   return(cursor);
}

char* rfbMakeMaskForXCursor(int width,int height,char* source)
{
   int i,j,w=(width+7)/8;
   char* mask=(char*)calloc(w,height);
   unsigned char c;
   
   if (!mask)
       return NULL;

   for(j=0;j<height;j++)
     for(i=w-1;i>=0;i--) {
	c=source[j*w+i];
	if(j>0) c|=source[(j-1)*w+i];
	if(j<height-1) c|=source[(j+1)*w+i];
	
	if(i>0 && (c&0x80))
	  mask[j*w+i-1]|=0x01;
	if(i<w-1 && (c&0x01))
	  mask[j*w+i+1]|=0x80;
	
	mask[j*w+i]|=(c<<1)|c|(c>>1);
     }
   
   return(mask);
}

/* this function dithers the alpha using Floyd-Steinberg */

char* rfbMakeMaskFromAlphaSource(int width,int height,unsigned char* alphaSource)
{
	int* error=(int*)calloc(sizeof(int),width);
	int i,j,currentError=0,maskStride=(width+7)/8;
	unsigned char* result=(unsigned char*)calloc(maskStride,height);
	
	if (!error || !result) {
		free(error);
		free(result);
		return NULL;
	}

	for(j=0;j<height;j++)
		for(i=0;i<width;i++) {
			int right,middle,left;
			currentError+=alphaSource[i+width*j]+error[i];
		
			if(currentError<0x80) {
				/* set to transparent */
				/* alpha was treated as 0 */
			} else {
				/* set to solid */
				result[i/8+j*maskStride]|=(0x100>>(i&7));
				/* alpha was treated as 0xff */
				currentError-=0xff;
			}
			/* propagate to next row */
			right=currentError/16;
			middle=currentError*5/16;
			left=currentError*3/16;
			currentError-=right+middle+left;
			error[i]=right;
			if(i>0) {
				error[i-1]=middle;
				if(i>1)
					error[i-2]=left;
			}
		}
	free(error);
	return (char *) result;
}

void rfbFreeCursor(rfbCursorPtr cursor)
{
   if(cursor) {
       if(cursor->cleanupRichSource && cursor->richSource)
	   free(cursor->richSource);
       if(cursor->cleanupRichSource && cursor->alphaSource)
	   free(cursor->alphaSource);
       if(cursor->cleanupSource && cursor->source)
	   free(cursor->source);
       if(cursor->cleanupMask && cursor->mask)
	   free(cursor->mask);
       if(cursor->cleanup)
	   free(cursor);
       else {
	   cursor->cleanup=cursor->cleanupSource=cursor->cleanupMask
	       =cursor->cleanupRichSource=FALSE;
	   cursor->source=cursor->mask=cursor->richSource=NULL;
	   cursor->alphaSource=NULL;
       }
   }
   
}

/* background and foregroud colour have to be set beforehand */
void rfbMakeXCursorFromRichCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor)
{
   rfbPixelFormat* format=&rfbScreen->serverFormat;
   int i,j,w=(cursor->width+7)/8,bpp=format->bitsPerPixel/8,
     width=cursor->width*bpp;
   uint32_t background;
   char *back=(char*)&background;
   unsigned char bit;
   int interp = 0;

   if(cursor->source && cursor->cleanupSource)
       free(cursor->source);
   cursor->source=(unsigned char*)calloc(w,cursor->height);
   if(!cursor->source)
       return;
   cursor->cleanupSource=TRUE;
   
   if(format->bigEndian) {
      back+=4-bpp;
   }

	/* all zeros means we should interpolate to black+white ourselves */
	if (!cursor->backRed && !cursor->backGreen && !cursor->backBlue &&
	    !cursor->foreRed && !cursor->foreGreen && !cursor->foreBlue) {
		if (format->trueColour && (bpp == 1 || bpp == 2 || bpp == 4)) {
			interp = 1;
			cursor->foreRed = cursor->foreGreen = cursor->foreBlue = 0xffff;
		}
	}

   background = ((format->redMax   * cursor->backRed)   / 0xffff) << format->redShift   |
                ((format->greenMax * cursor->backGreen) / 0xffff) << format->greenShift |
                ((format->blueMax  * cursor->backBlue)  / 0xffff) << format->blueShift;

#define SETRGB(u) \
   r = (255 * (((format->redMax   << format->redShift)   & (*u)) >> format->redShift))   / format->redMax; \
   g = (255 * (((format->greenMax << format->greenShift) & (*u)) >> format->greenShift)) / format->greenMax; \
   b = (255 * (((format->blueMax  << format->blueShift)  & (*u)) >> format->blueShift))  / format->blueMax;

#ifdef DEBUG_CURSOR
   fprintf(stderr, "interp: %d\n", interp);
#endif

   for(j=0;j<cursor->height;j++) {
	for(i=0,bit=0x80;i<cursor->width;i++,bit=(bit&1)?0x80:bit>>1) {
		if (interp) {
			int r = 0, g = 0, b = 0, grey;
			unsigned char *p = cursor->richSource+j*width+i*bpp;
			if (bpp == 1) {
				unsigned char*  uc = (unsigned char*)  p;
				SETRGB(uc);
			} else if (bpp == 2) {
				unsigned short* us = (unsigned short*) p;
				SETRGB(us);
			} else if (bpp == 4) {
				unsigned int*   ui = (unsigned int*)   p;
				SETRGB(ui);
			}
			grey = (r + g + b) / 3;
			if (grey >= 128) {
				cursor->source[j*w+i/8]|=bit;
#ifdef DEBUG_CURSOR
				fprintf(stderr, "1");
			} else {
				fprintf(stderr, "0");
#endif
			}
			
		} else if(memcmp(cursor->richSource+j*width+i*bpp, back, bpp)) {
			cursor->source[j*w+i/8]|=bit;
		}
	}
#ifdef DEBUG_CURSOR
	fprintf(stderr, "\n");
#endif
   }
}

void rfbMakeRichCursorFromXCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr cursor)
{
   rfbPixelFormat* format=&rfbScreen->serverFormat;
   int i,j,w=(cursor->width+7)/8,bpp=format->bitsPerPixel/8;
   uint32_t background,foreground;
   char *back=(char*)&background,*fore=(char*)&foreground;
   unsigned char *cp;
   unsigned char bit;

   if(cursor->richSource && cursor->cleanupRichSource)
       free(cursor->richSource);
   cp=cursor->richSource=(unsigned char*)calloc((size_t)cursor->width*bpp,cursor->height);
   if(!cp)
       return;
   cursor->cleanupRichSource=TRUE;
   
   if(format->bigEndian) {
      back+=4-bpp;
      fore+=4-bpp;
   }

   background=(uint32_t)cursor->backRed<<format->redShift|
     (uint32_t)cursor->backGreen<<format->greenShift|(uint32_t)cursor->backBlue<<format->blueShift;
   foreground=(uint32_t)cursor->foreRed<<format->redShift|
     (uint32_t)cursor->foreGreen<<format->greenShift|(uint32_t)cursor->foreBlue<<format->blueShift;
   
   for(j=0;j<cursor->height;j++)
     for(i=0,bit=0x80;i<cursor->width;i++,bit=(bit&1)?0x80:bit>>1,cp+=bpp)
       if(cursor->source[j*w+i/8]&bit) memcpy(cp,fore,bpp);
       else memcpy(cp,back,bpp);
}

/* functions to draw/hide cursor directly in the frame buffer */

void rfbHideCursor(rfbClientPtr cl)
{
   rfbScreenInfoPtr s=cl->screen;
   rfbCursorPtr c;
   int j,x1,x2,y1,y2,bpp=s->serverFormat.bitsPerPixel/8,
     rowstride=s->paddedWidthInBytes;
   LOCK(s->cursorMutex);
   c=s->cursor;
   if(!c) {
     UNLOCK(s->cursorMutex);
     return;
   }
   
   /* restore what is under the cursor */
   x1=cl->cursorX-c->xhot;
   x2=x1+c->width;
   if(x1<0) x1=0;
   if(x2>=s->width) x2=s->width-1;
   x2-=x1; if(x2<=0) {
     UNLOCK(s->cursorMutex);
     return;
   }
   y1=cl->cursorY-c->yhot;
   y2=y1+c->height;
   if(y1<0) y1=0;
   if(y2>=s->height) y2=s->height-1;
   y2-=y1; if(y2<=0) {
     UNLOCK(s->cursorMutex);
     return;
   }

   /* get saved data */
   for(j=0;j<y2;j++)
     memcpy(s->frameBuffer+(y1+j)*rowstride+x1*bpp,
	    s->underCursorBuffer+j*x2*bpp,
	    (size_t)x2*bpp);

   /* Copy to all scaled versions */
   rfbScaledScreenUpdate(s, x1, y1, x1+x2, y1+y2);
   
   UNLOCK(s->cursorMutex);
}

void rfbShowCursor(rfbClientPtr cl)
{
   rfbScreenInfoPtr s=cl->screen;
   rfbCursorPtr c;
   int i,j,x1,x2,y1,y2,i1,j1,bpp=s->serverFormat.bitsPerPixel/8,
     rowstride=s->paddedWidthInBytes,
     bufSize,w;
   rfbBool wasChanged=FALSE;

   LOCK(s->cursorMutex);
   c=s->cursor;
   if(!c) {
	UNLOCK(s->cursorMutex);
	return;
   }

   bufSize=c->width*c->height*bpp;
   w=(c->width+7)/8;
   if(s->underCursorBufferLen<bufSize) {
      if(s->underCursorBuffer!=NULL)
	free(s->underCursorBuffer);
      s->underCursorBuffer=malloc(bufSize);
      s->underCursorBufferLen=bufSize;
   }

   /* save what is under the cursor */
   i1=j1=0; /* offset in cursor */
   x1=cl->cursorX-c->xhot;
   x2=x1+c->width;
   if(x1<0) { i1=-x1; x1=0; }
   if(x2>=s->width) x2=s->width-1;
   x2-=x1; if(x2<=0) {
     UNLOCK(s->cursorMutex);
     return; /* nothing to do */
   }

   y1=cl->cursorY-c->yhot;
   y2=y1+c->height;
   if(y1<0) { j1=-y1; y1=0; }
   if(y2>=s->height) y2=s->height-1;
   y2-=y1; if(y2<=0) {
     UNLOCK(s->cursorMutex);
     return; /* nothing to do */
   }

   /* save data */
   for(j=0;j<y2;j++) {
     char* dest=s->underCursorBuffer+j*x2*bpp;
     const char* src=s->frameBuffer+(y1+j)*rowstride+x1*bpp;
     unsigned int count=x2*bpp;
     if(wasChanged || memcmp(dest,src,count)) {
       wasChanged=TRUE;
       memcpy(dest,src,count);
     }
   }
   
   if(!c->richSource)
     rfbMakeRichCursorFromXCursor(s,c);
  
   if (c->alphaSource) {
	int rmax, rshift;
	int gmax, gshift;
	int bmax, bshift;
	int amax = 255;	/* alphaSource is always 8bits of info per pixel */
	unsigned int rmask, gmask, bmask;

	rmax   = s->serverFormat.redMax;
	gmax   = s->serverFormat.greenMax;
	bmax   = s->serverFormat.blueMax;
	rshift = s->serverFormat.redShift;
	gshift = s->serverFormat.greenShift;
	bshift = s->serverFormat.blueShift;

	rmask = (rmax << rshift);
	gmask = (gmax << gshift);
	bmask = (bmax << bshift);

	for(j=0;j<y2;j++) {
		for(i=0;i<x2;i++) {
			/*
			 * we loop over the whole cursor ignoring c->mask[],
			 * using the extracted alpha value instead.
			 */
			char *dest;
			unsigned char *src, *aptr;
			unsigned int val, dval, sval;
			int rdst, gdst, bdst;		/* fb RGB */
			int asrc, rsrc, gsrc, bsrc;	/* rich source ARGB */

			dest = s->frameBuffer + (j+y1)*rowstride + (i+x1)*bpp;
			src  = c->richSource  + (j+j1)*c->width*bpp + (i+i1)*bpp;
			aptr = c->alphaSource + (j+j1)*c->width + (i+i1);

			asrc = *aptr;
			if (!asrc) {
				continue;
			}

			if (bpp == 1) {
				dval = *((unsigned char*) dest);
				sval = *((unsigned char*) src);
			} else if (bpp == 2) {
				dval = *((unsigned short*) dest);
				sval = *((unsigned short*) src);
			} else if (bpp == 3) {
				unsigned char *dst = (unsigned char *) dest;
				dval = 0;
				dval |= ((*(dst+0)) << 0);
				dval |= ((*(dst+1)) << 8);
				dval |= ((*(dst+2)) << 16);
				sval = 0;
				sval |= ((*(src+0)) << 0);
				sval |= ((*(src+1)) << 8);
				sval |= ((*(src+2)) << 16);
			} else if (bpp == 4) {
				dval = *((unsigned int*) dest);
				sval = *((unsigned int*) src);
			} else {
				continue;
			}

			/* extract dest and src RGB */
			rdst = (dval & rmask) >> rshift;	/* fb */
			gdst = (dval & gmask) >> gshift;
			bdst = (dval & bmask) >> bshift;

			rsrc = (sval & rmask) >> rshift;	/* richcursor */
			gsrc = (sval & gmask) >> gshift;
			bsrc = (sval & bmask) >> bshift;

			/* blend in fb data. */
			if (! c->alphaPreMultiplied) {
				rsrc = (asrc * rsrc)/amax;
				gsrc = (asrc * gsrc)/amax;
				bsrc = (asrc * bsrc)/amax;
			}
			rdst = rsrc + ((amax - asrc) * rdst)/amax;
			gdst = gsrc + ((amax - asrc) * gdst)/amax;
			bdst = bsrc + ((amax - asrc) * bdst)/amax;

			val = 0;
			val |= (rdst << rshift);
			val |= (gdst << gshift);
			val |= (bdst << bshift);

			/* insert the cooked pixel into the fb */
			memcpy(dest, &val, bpp);
		}
	}
   } else {
      /* now the cursor has to be drawn */
      for(j=0;j<y2;j++)
        for(i=0;i<x2;i++)
          if((c->mask[(j+j1)*w+(i+i1)/8]<<((i+i1)&7))&0x80)
   	 memcpy(s->frameBuffer+(j+y1)*rowstride+(i+x1)*bpp,
   		c->richSource+(j+j1)*c->width*bpp+(i+i1)*bpp,bpp);
   }

   /* Copy to all scaled versions */
   rfbScaledScreenUpdate(s, x1, y1, x1+x2, y1+y2);

   UNLOCK(s->cursorMutex);
}

/* 
 * If enableCursorShapeUpdates is FALSE, and the cursor is hidden, make sure
 * that if the frameBuffer was transmitted with a cursor drawn, then that
 * region gets redrawn.
 */

void rfbRedrawAfterHideCursor(rfbClientPtr cl,sraRegionPtr updateRegion)
{
    rfbScreenInfoPtr s = cl->screen;
    rfbCursorPtr c = s->cursor;
    
    if(c) {
	int x,y,x2,y2;

	x = cl->cursorX-c->xhot;
	y = cl->cursorY-c->yhot;
	x2 = x+c->width;
	y2 = y+c->height;

	if(sraClipRect2(&x,&y,&x2,&y2,0,0,s->width,s->height)) {
	    sraRegionPtr rect;
	    rect = sraRgnCreateRect(x,y,x2,y2);
	    if(updateRegion) {
	    	sraRgnOr(updateRegion,rect);
	    } else {
		    LOCK(cl->updateMutex);
		    sraRgnOr(cl->modifiedRegion,rect);
		    UNLOCK(cl->updateMutex);
	    }
	    sraRgnDestroy(rect);
	}
    }
}

#ifdef DEBUG

static void rfbPrintXCursor(rfbCursorPtr cursor)
{
   int i,i1,j,w=(cursor->width+7)/8;
   unsigned char bit;
   for(j=0;j<cursor->height;j++) {
      for(i=0,i1=0,bit=0x80;i1<cursor->width;i1++,i+=(bit&1)?1:0,bit=(bit&1)?0x80:bit>>1)
	if(cursor->source[j*w+i]&bit) putchar('#'); else putchar(' ');
      putchar(':');
      for(i=0,i1=0,bit=0x80;i1<cursor->width;i1++,i+=(bit&1)?1:0,bit=(bit&1)?0x80:bit>>1)
	if(cursor->mask[j*w+i]&bit) putchar('#'); else putchar(' ');
      putchar('\n');
   }
}

#endif

void rfbSetCursor(rfbScreenInfoPtr rfbScreen,rfbCursorPtr c)
{
  rfbClientIteratorPtr iterator;
  rfbClientPtr cl;

  LOCK(rfbScreen->cursorMutex);

  if(rfbScreen->cursor) {
    iterator=rfbGetClientIterator(rfbScreen);
    while((cl=rfbClientIteratorNext(iterator)))
	if(!cl->enableCursorShapeUpdates)
	  rfbRedrawAfterHideCursor(cl,NULL);
    rfbReleaseClientIterator(iterator);

    if(rfbScreen->cursor->cleanup)
	 rfbFreeCursor(rfbScreen->cursor);
  }

  rfbScreen->cursor = c;

  iterator=rfbGetClientIterator(rfbScreen);
  while((cl=rfbClientIteratorNext(iterator))) {
    cl->cursorWasChanged = TRUE;
    if(!cl->enableCursorShapeUpdates)
      rfbRedrawAfterHideCursor(cl,NULL);
  }
  rfbReleaseClientIterator(iterator);

  UNLOCK(rfbScreen->cursorMutex);
}

