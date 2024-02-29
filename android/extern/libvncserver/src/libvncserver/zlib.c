/*
 * zlib.c
 *
 * Routines to implement zlib based encoding (deflate).
 */

/*
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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
 *
 * For the latest source code, please check:
 *
 * http://www.developVNC.org/
 *
 * or send email to feedback@developvnc.org.
 */

#include <rfb/rfb.h>

/*
 * cl->beforeEncBuf contains pixel data in the client's format.
 * cl->afterEncBuf contains the zlib (deflated) encoding version.
 * If the zlib compressed/encoded version is
 * larger than the raw data or if it exceeds cl->afterEncBufSize then
 * raw encoding is used instead.
 */

/*
 * rfbSendOneRectEncodingZlib - send a given rectangle using one Zlib
 *                              rectangle encoding.
 */

static rfbBool
rfbSendOneRectEncodingZlib(rfbClientPtr cl,
                           int x,
                           int y,
                           int w,
                           int h)
{
    rfbFramebufferUpdateRectHeader rect;
    rfbZlibHeader hdr;
    int deflateResult;
    int previousOut;
    int i;
    char *fbptr = (cl->scaledScreen->frameBuffer + (cl->scaledScreen->paddedWidthInBytes * y)
    	   + (x * (cl->scaledScreen->bitsPerPixel / 8)));

    int maxRawSize;
    int maxCompSize;

    maxRawSize = (cl->scaledScreen->width * cl->scaledScreen->height
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

    /* zlib compression is not useful for very small data sets.
     * So, we just send these raw without any compression.
     */
    if (( w * h * (cl->scaledScreen->bitsPerPixel / 8)) <
          VNC_ENCODE_ZLIB_MIN_COMP_SIZE ) {

        int result;

        /* The translation function (used also by the in raw encoding)
         * requires 4/2/1 byte alignment in the output buffer (which is
         * updateBuf for the raw encoding) based on the bitsPerPixel of
         * the viewer/client.  This prevents SIGBUS errors on some
         * architectures like SPARC, PARISC...
         */
        if (( cl->format.bitsPerPixel > 8 ) &&
            ( cl->ublen % ( cl->format.bitsPerPixel / 8 )) != 0 ) {
            if (!rfbSendUpdateBuf(cl))
                return FALSE;
        }

        result = rfbSendRectEncodingRaw(cl, x, y, w, h);

        return result;

    }

    /*
     * zlib requires output buffer to be slightly larger than the input
     * buffer, in the worst case.
     */
    maxCompSize = maxRawSize + (( maxRawSize + 99 ) / 100 ) + 12;

    if (!cl->afterEncBuf || cl->afterEncBufSize < maxCompSize) {
        if (cl->afterEncBuf == NULL)
            cl->afterEncBuf = (char *)malloc(maxCompSize);
        else {
            char *reallocedAfterEncBuf = (char *)realloc(cl->afterEncBuf, maxCompSize);
            if (!reallocedAfterEncBuf) return FALSE;
            cl->afterEncBuf = reallocedAfterEncBuf;
        }
        if(cl->afterEncBuf)
            cl->afterEncBufSize = maxCompSize;
    }

    if (!cl->beforeEncBuf || !cl->afterEncBuf)
    {
        rfbLog("rfbSendOneRectEncodingZlib: failed to allocate memory\n");
        return FALSE;
    }

    /* 
     * Convert pixel data to client format.
     */
    (*cl->translateFn)(cl->translateLookupTable, &cl->screen->serverFormat,
               &cl->format, fbptr, cl->beforeEncBuf,
		       cl->scaledScreen->paddedWidthInBytes, w, h);

    cl->compStream.next_in = ( Bytef * )cl->beforeEncBuf;
    cl->compStream.avail_in = w * h * (cl->format.bitsPerPixel / 8);
    cl->compStream.next_out = ( Bytef * )cl->afterEncBuf;
    cl->compStream.avail_out = maxCompSize;
    cl->compStream.data_type = Z_BINARY;

    /* Initialize the deflation state. */
    if ( cl->compStreamInited == FALSE ) {

        cl->compStream.total_in = 0;
        cl->compStream.total_out = 0;
        cl->compStream.zalloc = Z_NULL;
        cl->compStream.zfree = Z_NULL;
        cl->compStream.opaque = Z_NULL;

        deflateInit2( &(cl->compStream),
                        cl->zlibCompressLevel,
                        Z_DEFLATED,
                        MAX_WBITS,
                        MAX_MEM_LEVEL,
                        Z_DEFAULT_STRATEGY );
        /* deflateInit( &(cl->compStream), Z_BEST_COMPRESSION ); */
        /* deflateInit( &(cl->compStream), Z_BEST_SPEED ); */
        cl->compStreamInited = TRUE;

    }

    previousOut = cl->compStream.total_out;

    /* Perform the compression here. */
    deflateResult = deflate( &(cl->compStream), Z_SYNC_FLUSH );

    /* Find the total size of the resulting compressed data. */
    cl->afterEncBufLen = cl->compStream.total_out - previousOut;

    if ( deflateResult != Z_OK ) {
        rfbErr("zlib deflation error: %s\n", cl->compStream.msg);
        return FALSE;
    }

    /* Note that it is not possible to switch zlib parameters based on
     * the results of the compression pass.  The reason is
     * that we rely on the compressor and decompressor states being
     * in sync.  Compressing and then discarding the results would
     * cause lose of synchronization.
     */

    /* Update statics */
    rfbStatRecordEncodingSent(cl, rfbEncodingZlib, sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader + cl->afterEncBufLen,
        + w * (cl->format.bitsPerPixel / 8) * h);

    if (cl->ublen + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader
	> UPDATE_BUF_SIZE)
    {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingZlib);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    hdr.nBytes = Swap32IfLE(cl->afterEncBufLen);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&hdr, sz_rfbZlibHeader);
    cl->ublen += sz_rfbZlibHeader;

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
 * rfbSendRectEncodingZlib - send a given rectangle using one or more
 *                           Zlib encoding rectangles.
 */

rfbBool
rfbSendRectEncodingZlib(rfbClientPtr cl,
                        int x,
                        int y,
                        int w,
                        int h)
{
    int  maxLines;
    int  linesRemaining;
    rfbRectangle partialRect;

    partialRect.x = x;
    partialRect.y = y;
    partialRect.w = w;
    partialRect.h = h;

    /* Determine maximum pixel/scan lines allowed per rectangle. */
    maxLines = ( ZLIB_MAX_SIZE(w) / w );

    /* Initialize number of scan lines left to do. */
    linesRemaining = h;

    /* Loop until all work is done. */
    while ( linesRemaining > 0 ) {

        int linesToComp;

        if ( maxLines < linesRemaining )
            linesToComp = maxLines;
        else
            linesToComp = linesRemaining;

        partialRect.h = linesToComp;

        /* Encode (compress) and send the next rectangle. */
        if ( ! rfbSendOneRectEncodingZlib( cl,
                                           partialRect.x,
                                           partialRect.y,
                                           partialRect.w,
                                           partialRect.h )) {

            return FALSE;
        }

        /* Technically, flushing the buffer here is not extremely
         * efficient.  However, this improves the overall throughput
         * of the system over very slow networks.  By flushing
         * the buffer with every maximum size zlib rectangle, we
         * improve the pipelining usage of the server CPU, network,
         * and viewer CPU components.  Insuring that these components
         * are working in parallel actually improves the performance
         * seen by the user.
         * Since, zlib is most useful for slow networks, this flush
         * is appropriate for the desired behavior of the zlib encoding.
         */
        if (( cl->ublen > 0 ) &&
            ( linesToComp == maxLines )) {
            if (!rfbSendUpdateBuf(cl)) {

                return FALSE;
            }
        }

        /* Update remaining and incremental rectangle location. */
        linesRemaining -= linesToComp;
        partialRect.y += linesToComp;

    }

    return TRUE;

}

