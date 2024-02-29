/*
 * ultra.c
 *
 * Routines to implement ultra based encoding (minilzo).
 * ultrazip supports packed rectangles if the rects are tiny...
 * This improves performance as lzo has more data to work with at once
 * This is 'UltraZip' and is currently not implemented.
 */

#include <rfb/rfb.h>
#ifdef LIBVNCSERVER_HAVE_LZO
#include <lzo/lzo1x.h>
#else
#include "minilzo.h"
#endif

/*
 * cl->beforeEncBuf contains pixel data in the client's format.
 * cl->afterEncBuf contains the lzo (deflated) encoding version.
 * If the lzo compressed/encoded version is
 * larger than the raw data or if it exceeds cl->afterEncBufSize then
 * raw encoding is used instead.
 */


/*
 * rfbSendOneRectEncodingZlib - send a given rectangle using one Zlib
 *                              rectangle encoding.
 */

#define MAX_WRKMEM ((LZO1X_1_MEM_COMPRESS) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)


void rfbFreeUltraData(rfbClientPtr cl) {
  if (cl->compStreamInitedLZO) {
    free(cl->lzoWrkMem);
    cl->compStreamInitedLZO=FALSE;
  }
}


static rfbBool
rfbSendOneRectEncodingUltra(rfbClientPtr cl,
                           int x,
                           int y,
                           int w,
                           int h)
{
    rfbFramebufferUpdateRectHeader rect;
    rfbZlibHeader hdr;
    int deflateResult;
    int i;
    char *fbptr = (cl->scaledScreen->frameBuffer + (cl->scaledScreen->paddedWidthInBytes * y)
    	   + (x * (cl->scaledScreen->bitsPerPixel / 8)));

    int maxRawSize;
    lzo_uint maxCompSize;

    maxRawSize = (w * h * (cl->format.bitsPerPixel / 8));

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

    /*
     * lzo requires output buffer to be slightly larger than the input
     * buffer, in the worst case.
     */
    maxCompSize = (maxRawSize + maxRawSize / 16 + 64 + 3);

    if (!cl->afterEncBuf || cl->afterEncBufSize < (int)maxCompSize) {
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
        rfbLog("rfbSendOneRectEncodingUltra: failed to allocate memory\n");
        return FALSE;
    }

    /* 
     * Convert pixel data to client format.
     */
    (*cl->translateFn)(cl->translateLookupTable, &cl->screen->serverFormat,
		       &cl->format, fbptr, cl->beforeEncBuf,
		       cl->scaledScreen->paddedWidthInBytes, w, h);

    if ( cl->compStreamInitedLZO == FALSE ) {
        cl->compStreamInitedLZO = TRUE;
        /* Work-memory needed for compression. Allocate memory in units
         * of `lzo_align_t' (instead of `char') to make sure it is properly aligned.
         */  
        cl->lzoWrkMem = malloc(sizeof(lzo_align_t) * (((LZO1X_1_MEM_COMPRESS) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)));
    }

    /* Perform the compression here. */
    deflateResult = lzo1x_1_compress((unsigned char *)cl->beforeEncBuf, (lzo_uint)w * h * (cl->format.bitsPerPixel / 8), (unsigned char *)cl->afterEncBuf, &maxCompSize, cl->lzoWrkMem);
    /* maxCompSize now contains the compressed size */

    /* Find the total size of the resulting compressed data. */
    cl->afterEncBufLen = maxCompSize;

    if ( deflateResult != LZO_E_OK ) {
        rfbErr("lzo deflation error: %d\n", deflateResult);
        return FALSE;
    }

    /* Update statics */
    rfbStatRecordEncodingSent(cl, rfbEncodingUltra, sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader + cl->afterEncBufLen, maxRawSize);

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
    rect.encoding = Swap32IfLE(rfbEncodingUltra);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;

    hdr.nBytes = Swap32IfLE(cl->afterEncBufLen);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&hdr, sz_rfbZlibHeader);
    cl->ublen += sz_rfbZlibHeader;

    /* We might want to try sending the data directly... */
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
 * rfbSendRectEncodingUltra - send a given rectangle using one or more
 *                           LZO encoding rectangles.
 */

rfbBool
rfbSendRectEncodingUltra(rfbClientPtr cl,
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
    maxLines = ( ULTRA_MAX_SIZE(w) / w );

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
        if ( ! rfbSendOneRectEncodingUltra( cl,
                                           partialRect.x,
                                           partialRect.y,
                                           partialRect.w,
                                           partialRect.h )) {

            return FALSE;
        }

        /* Technically, flushing the buffer here is not extremely
         * efficient.  However, this improves the overall throughput
         * of the system over very slow networks.  By flushing
         * the buffer with every maximum size lzo rectangle, we
         * improve the pipelining usage of the server CPU, network,
         * and viewer CPU components.  Insuring that these components
         * are working in parallel actually improves the performance
         * seen by the user.
         * Since, lzo is most useful for slow networks, this flush
         * is appropriate for the desired behavior of the lzo encoding.
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
