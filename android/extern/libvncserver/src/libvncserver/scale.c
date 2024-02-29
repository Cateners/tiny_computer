/*
 * scale.c - deal with server-side scaling.
 */

/*
 *  Copyright (C) 2005 Rohit Kumar, Johannes E. Schindelin
 *  Copyright (C) 2002 RealVNC Ltd.
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

#ifdef __STRICT_ANSI__
#define _BSD_SOURCE
#endif
#include <string.h>
#include <rfb/rfb.h>
#include <rfb/rfbregion.h>
#include "private.h"

#ifdef LIBVNCSERVER_HAVE_FCNTL_H
#include <fcntl.h>
#endif


#ifdef DEBUGPROTO
#undef DEBUGPROTO
#define DEBUGPROTO(x) x
#else
#define DEBUGPROTO(x)
#endif

/****************************/
#define CEIL(x)  ( (double) ((int) (x)) == (x) ? \
        (double) ((int) (x)) : (double) ((int) (x) + 1) )
#define FLOOR(x) ( (double) ((int) (x)) )

#ifdef WIN32
#define InlineX __inline
#else
# ifndef __STRICT_ANSI__
#  define InlineX inline
# else
#  define InlineX
# endif
#endif


static InlineX int pad4(int value)
{
    int remainder = value & 3;
    if (!remainder) return value;
    return value + 4 - remainder;
}

int ScaleX(rfbScreenInfoPtr from, rfbScreenInfoPtr to, int x)
{
    if ((from==to) || (from==NULL) || (to==NULL)) return x;
    return ((int)(((double) x / (double)from->width) * (double)to->width ));
}

int ScaleY(rfbScreenInfoPtr from, rfbScreenInfoPtr to, int y)
{
    if ((from==to) || (from==NULL) || (to==NULL)) return y;
    return ((int)(((double) y / (double)from->height) * (double)to->height ));
}

/* So, all of the encodings point to the ->screen->frameBuffer,
 * We need to change this!
 */
void rfbScaledCorrection(rfbScreenInfoPtr from, rfbScreenInfoPtr to, int *x, int *y, int *w, int *h, const char *function)
{
    double x1,y1,w1,h1, x2, y2, w2, h2;
    double scaleW = ((double) to->width) / ((double) from->width);
    double scaleH = ((double) to->height) / ((double) from->height);


    /*
     * rfbLog("rfbScaledCorrection(%p -> %p, %dx%d->%dx%d (%dXx%dY-%dWx%dH)\n",
     * from, to, from->width, from->height, to->width, to->height, *x, *y, *w, *h);
     */

    /* If it's the original framebuffer... */
    if (from==to) return;

    x1 = ((double) *x) * scaleW;
    y1 = ((double) *y) * scaleH;
    w1 = ((double) *w) * scaleW;
    h1 = ((double) *h) * scaleH;


    /*cast from double to int is same as "*x = floor(x1);" */
    x2 = FLOOR(x1);
    y2 = FLOOR(y1);

    /* include into W and H the jitter of scaling X and Y */
    w2 = CEIL(w1 + ( x1 - x2 ));
    h2 = CEIL(h1 + ( y1 - y2 ));

    /*
     * rfbLog("%s (%dXx%dY-%dWx%dH  ->  %fXx%fY-%fWx%fH) {%dWx%dH -> %dWx%dH}\n",
     *    function, *x, *y, *w, *h, x2, y2, w2, h2,
     *    from->width, from->height, to->width, to->height);
     */

    /* simulate ceil() without math library */
    *x = (int)x2;
    *y = (int)y2;
    *w = (int)w2;
    *h = (int)h2;

    /* Small changes for a thumbnail may be scaled to zero */
    if (*w==0) (*w)++;
    if (*h==0) (*h)++;
    /* scaling from small to big may overstep the size a bit */
    if (*x+*w > to->width)  *w=to->width - *x;
    if (*y+*h > to->height) *h=to->height - *y;
}

void rfbScaledScreenUpdateRect(rfbScreenInfoPtr screen, rfbScreenInfoPtr ptr, int x0, int y0, int w0, int h0)
{
    int x,y,w,v,z;
    int x1, y1, w1, h1;
    int bitsPerPixel, bytesPerPixel, bytesPerLine, areaX, areaY, area2;
    unsigned char *srcptr, *dstptr;

    /* Nothing to do!!! */
    if (screen==ptr) return;

    x1 = x0;
    y1 = y0;
    w1 = w0;
    h1 = h0;

    rfbScaledCorrection(screen, ptr, &x1, &y1, &w1, &h1, "rfbScaledScreenUpdateRect");
    x0 = ScaleX(ptr, screen, x1);
    y0 = ScaleY(ptr, screen, y1);
    w0 = ScaleX(ptr, screen, w1);
    h0 = ScaleY(ptr, screen, h1);

    bitsPerPixel = screen->bitsPerPixel;
    bytesPerPixel = bitsPerPixel / 8;
    bytesPerLine = w1 * bytesPerPixel;
    srcptr = (unsigned char *)(screen->frameBuffer +
     (y0 * screen->paddedWidthInBytes + x0 * bytesPerPixel));
    dstptr = (unsigned char *)(ptr->frameBuffer +
     ( y1 * ptr->paddedWidthInBytes + x1 * bytesPerPixel));
    /* The area of the source framebuffer for each destination pixel */
    areaX = ScaleX(ptr,screen,1);
    areaY = ScaleY(ptr,screen,1);
    area2 = areaX*areaY;


    /* Ensure that we do not go out of bounds */
    if ((x1+w1) > (ptr->width))
    {
      if (x1==0) w1=ptr->width; else x1 = ptr->width - w1;
    }
    if ((y1+h1) > (ptr->height))
    {
      if (y1==0) h1=ptr->height; else y1 = ptr->height - h1;
    }
    /*
     * rfbLog("rfbScaledScreenUpdateRect(%dXx%dY-%dWx%dH  ->  %dXx%dY-%dWx%dH <%dx%d>) {%dWx%dH -> %dWx%dH} 0x%p\n",
     *    x0, y0, w0, h0, x1, y1, w1, h1, areaX, areaY,
     *    screen->width, screen->height, ptr->width, ptr->height, ptr->frameBuffer);
     */

    if (screen->serverFormat.trueColour) { /* Blend neighbouring pixels together */
      unsigned char *srcptr2;
      unsigned long pixel_value, red, green, blue;
      unsigned int redShift = screen->serverFormat.redShift;
      unsigned int greenShift = screen->serverFormat.greenShift;
      unsigned int blueShift = screen->serverFormat.blueShift;
      unsigned long redMax = screen->serverFormat.redMax;
      unsigned long greenMax = screen->serverFormat.greenMax;
      unsigned long blueMax = screen->serverFormat.blueMax;

     /* for each *destination* pixel... */
     for (y = 0; y < h1; y++) {
       for (x = 0; x < w1; x++) {
         red = green = blue = 0;
         /* Get the totals for rgb from the source grid... */
         for (w = 0; w < areaX; w++) {
           for (v = 0; v < areaY; v++) {
             srcptr2 = &srcptr[(((x * areaX) + w) * bytesPerPixel) +
                               (v * screen->paddedWidthInBytes)];
             pixel_value = 0;


             switch (bytesPerPixel) {
             case 4: pixel_value = *((unsigned int *)srcptr2);   break;
             case 2: pixel_value = *((unsigned short *)srcptr2); break;
             case 1: pixel_value = *((unsigned char *)srcptr2);  break;
             default:
               /* fixme: endianness problem? */
               for (z = 0; z < bytesPerPixel; z++)
                 pixel_value += ((unsigned long)srcptr2[z] << (8 * z));
                break;
              }
              /*
              srcptr2 += bytesPerPixel;
              */

            red += ((pixel_value >> redShift) & redMax);
            green += ((pixel_value >> greenShift) & greenMax);
            blue += ((pixel_value >> blueShift) & blueMax);

           }
         }
         /* We now have a total for all of the colors, find the average! */
         red /= area2;
         green /= area2;
         blue /= area2;
          /* Stuff the new value back into memory */
         pixel_value = ((red & redMax) << redShift) | ((green & greenMax) << greenShift) | ((blue & blueMax) << blueShift);

         switch (bytesPerPixel) {
         case 4: *((unsigned int *)dstptr)   = (unsigned int)   pixel_value; break;
         case 2: *((unsigned short *)dstptr) = (unsigned short) pixel_value; break;
         case 1: *((unsigned char *)dstptr)  = (unsigned char)  pixel_value; break;
         default:
           /* fixme: endianness problem? */
           for (z = 0; z < bytesPerPixel; z++)
             dstptr[z]=(pixel_value >> (8 * z)) & 0xff;
            break;
          }
          dstptr += bytesPerPixel;
       }
       srcptr += (screen->paddedWidthInBytes * areaY);
       dstptr += (ptr->paddedWidthInBytes - bytesPerLine);
     }
   } else
   { /* Not truecolour, so we can't blend. Just use the top-left pixel instead */
     for (y = y1; y < (y1+h1); y++) {
       for (x = x1; x < (x1+w1); x++)
         memcpy (&ptr->frameBuffer[(y *ptr->paddedWidthInBytes) + (x * bytesPerPixel)],
                 &screen->frameBuffer[(y * areaY * screen->paddedWidthInBytes) + (x *areaX * bytesPerPixel)], bytesPerPixel);
     }
  }
}

void rfbScaledScreenUpdate(rfbScreenInfoPtr screen, int x1, int y1, int x2, int y2)
{
    /* ok, now the task is to update each and every scaled version of the framebuffer
     * and we only have to do this for this specific changed rectangle!
     */
    rfbScreenInfoPtr ptr;
    int count=0;

    /* We don't point to cl->screen as it is the original */
    for (ptr=screen->scaledScreenNext;ptr!=NULL;ptr=ptr->scaledScreenNext)
    {
        /* Only update if it has active clients... */
        if (ptr->scaledScreenRefCount>0)
        {
          rfbScaledScreenUpdateRect(screen, ptr, x1, y1, x2-x1, y2-y1);
          count++;
        }
    }
}

/* Create a new scaled version of the framebuffer */
rfbScreenInfoPtr rfbScaledScreenAllocate(rfbClientPtr cl, int width, int height)
{
    rfbScreenInfoPtr ptr;
    ptr = malloc(sizeof(rfbScreenInfo));
    if (ptr!=NULL)
    {
        int allocSize;

        /* copy *everything* (we don't use most of it, but just in case) */
        memcpy(ptr, cl->screen, sizeof(rfbScreenInfo));

        /* SECURITY: make sure that no integer overflow will occur afterwards.
         * Note: this is defensive coding, as the check should have already been
         * performed during initial, non-scaled screen setup.
         */
        allocSize = pad4(width * (ptr->bitsPerPixel/8)); /* per protocol, width<2**16 and bpp<256 */
        if (height == 0 || allocSize >= SIZE_MAX / height)
        {
          free(ptr);
          return NULL; /* malloc() will allocate an incorrect buffer size - early abort */
        }

        /* Resume copy everything */
        ptr->width = width;
        ptr->height = height;
        ptr->paddedWidthInBytes = (ptr->bitsPerPixel/8)*ptr->width;

        /* Need to by multiples of 4 for Sparc systems */
        ptr->paddedWidthInBytes = pad4(ptr->paddedWidthInBytes);

        /* Reset the reference count to 0! */
        ptr->scaledScreenRefCount = 0;

        ptr->sizeInBytes = ptr->paddedWidthInBytes * ptr->height;
        ptr->serverFormat = cl->screen->serverFormat;

        ptr->frameBuffer = malloc(ptr->sizeInBytes);
        if (ptr->frameBuffer!=NULL)
        {
            /* Reset to a known condition: scale the entire framebuffer */
            rfbScaledScreenUpdateRect(cl->screen, ptr, 0, 0, cl->screen->width, cl->screen->height);
            /* Now, insert into the chain */
            LOCK(cl->updateMutex);
            ptr->scaledScreenNext = cl->screen->scaledScreenNext;
            cl->screen->scaledScreenNext = ptr;
            UNLOCK(cl->updateMutex);
        }
        else
        {
            /* Failed to malloc the new frameBuffer, cleanup */
            free(ptr);
            ptr=NULL;
        }
    }
    return ptr;
}

/* Find an active scaled version of the framebuffer
 * TODO: implement a refcount per scaled screen to prevent
 * unreferenced scaled screens from hanging around
 */
rfbScreenInfoPtr rfbScalingFind(rfbClientPtr cl, int width, int height)
{
    rfbScreenInfoPtr ptr;
    /* include the original in the search (ie: fine 1:1 scaled version of the frameBuffer) */
    for (ptr=cl->screen; ptr!=NULL; ptr=ptr->scaledScreenNext)
    {
        if ((ptr->width==width) && (ptr->height==height))
            return ptr;
    }
    return NULL;
}

/* Future needs "scale to 320x240, as that's the client's screen size */
void rfbScalingSetup(rfbClientPtr cl, int width, int height)
{
    rfbScreenInfoPtr ptr;

    ptr = rfbScalingFind(cl,width,height);
    if (ptr==NULL)
        ptr = rfbScaledScreenAllocate(cl,width,height);
    /* Now, there is a new screen available (if ptr is not NULL) */
    if (ptr!=NULL)
    {
        /* Update it! */
        if (ptr->scaledScreenRefCount<1)
            rfbScaledScreenUpdateRect(cl->screen, ptr, 0, 0, cl->screen->width, cl->screen->height);
        /*
         * rfbLog("Taking one from %dx%d-%d and adding it to %dx%d-%d\n",
         *    cl->scaledScreen->width, cl->scaledScreen->height,
         *    cl->scaledScreen->scaledScreenRefCount,
         *    ptr->width, ptr->height, ptr->scaledScreenRefCount);
         */

        LOCK(cl->updateMutex);
        cl->scaledScreen->scaledScreenRefCount--;
        ptr->scaledScreenRefCount++;
        cl->scaledScreen=ptr;
        cl->newFBSizePending = TRUE;
        UNLOCK(cl->updateMutex);

        rfbLog("Scaling to %dx%d (refcount=%d)\n",width,height,ptr->scaledScreenRefCount);
    }
    else
        rfbLog("Scaling to %dx%d failed, leaving things alone\n",width,height);
}

int rfbSendNewScaleSize(rfbClientPtr cl)
{
    /* if the client supports newFBsize Encoding, use it */
    if (cl->useNewFBSize && cl->newFBSizePending)
	return FALSE;

    LOCK(cl->updateMutex);
    cl->newFBSizePending = FALSE;
    UNLOCK(cl->updateMutex);

    if (cl->PalmVNC==TRUE)
    {
        rfbPalmVNCReSizeFrameBufferMsg pmsg;
        pmsg.type = rfbPalmVNCReSizeFrameBuffer;
        pmsg.pad1 = 0;
        pmsg.desktop_w = Swap16IfLE(cl->screen->width);
        pmsg.desktop_h = Swap16IfLE(cl->screen->height);
        pmsg.buffer_w  = Swap16IfLE(cl->scaledScreen->width);
        pmsg.buffer_h  = Swap16IfLE(cl->scaledScreen->height);
        pmsg.pad2 = 0;

        rfbLog("Sending a response to a PalmVNC style frameuffer resize event (%dx%d)\n", cl->scaledScreen->width, cl->scaledScreen->height);
	LOCK(cl->sendMutex);
        if (rfbWriteExact(cl, (char *)&pmsg, sz_rfbPalmVNCReSizeFrameBufferMsg) < 0) {
            rfbLogPerror("rfbNewClient: write");
            rfbCloseClient(cl);
	    UNLOCK(cl->sendMutex);
            return FALSE;
        }
	UNLOCK(cl->sendMutex);
    }
    else
    {
        rfbResizeFrameBufferMsg        rmsg;
        rmsg.type = rfbResizeFrameBuffer;
        rmsg.pad1=0;
        rmsg.framebufferWidth  = Swap16IfLE(cl->scaledScreen->width);
        rmsg.framebufferHeigth = Swap16IfLE(cl->scaledScreen->height);
        rfbLog("Sending a response to a UltraVNC style frameuffer resize event (%dx%d)\n", cl->scaledScreen->width, cl->scaledScreen->height);
	LOCK(cl->sendMutex);
        if (rfbWriteExact(cl, (char *)&rmsg, sz_rfbResizeFrameBufferMsg) < 0) {
            rfbLogPerror("rfbNewClient: write");
            rfbCloseClient(cl);
	    UNLOCK(cl->sendMutex);
            return FALSE;
        }
	UNLOCK(cl->sendMutex);
    }
    return TRUE;
}
/****************************/
