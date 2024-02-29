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

#include "zrleoutstream.h"
#include <stdlib.h>

#define ZRLE_IN_BUFFER_SIZE  16384
#define ZRLE_OUT_BUFFER_SIZE 1024
#undef  ZRLE_DEBUG

static rfbBool zrleBufferAlloc(zrleBuffer *buffer, int size)
{
  buffer->ptr = buffer->start = malloc(size);
  if (buffer->start == NULL) {
    buffer->end = NULL;
    return FALSE;
  }

  buffer->end = buffer->start + size;

  return TRUE;
}

static void zrleBufferFree(zrleBuffer *buffer)
{
  if (buffer->start)
    free(buffer->start);
  buffer->start = buffer->ptr = buffer->end = NULL;
}

static rfbBool zrleBufferGrow(zrleBuffer *buffer, int size)
{
  int offset;
  void *new_buffer;

  size  += buffer->end - buffer->start;
  offset = ZRLE_BUFFER_LENGTH (buffer);

  new_buffer = realloc(buffer->start, size);
  if (!new_buffer) {
    return FALSE;
  }

  buffer->start = new_buffer;

  buffer->end = buffer->start + size;
  buffer->ptr = buffer->start + offset;

  return TRUE;
}

zrleOutStream *zrleOutStreamNew(void)
{
  zrleOutStream *os;

  os = malloc(sizeof(zrleOutStream));
  if (os == NULL)
    return NULL;

  if (!zrleBufferAlloc(&os->in, ZRLE_IN_BUFFER_SIZE)) {
    free(os);
    return NULL;
  }

  if (!zrleBufferAlloc(&os->out, ZRLE_OUT_BUFFER_SIZE)) {
    zrleBufferFree(&os->in);
    free(os);
    return NULL;
  }

  os->zs.zalloc = Z_NULL;
  os->zs.zfree  = Z_NULL;
  os->zs.opaque = Z_NULL;
  if (deflateInit(&os->zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
    zrleBufferFree(&os->in);
    free(os);
    return NULL;
  }

  return os;
}

void zrleOutStreamFree (zrleOutStream *os)
{
  deflateEnd(&os->zs);
  zrleBufferFree(&os->in);
  zrleBufferFree(&os->out);
  free(os);
}

rfbBool zrleOutStreamFlush(zrleOutStream *os)
{
  os->zs.next_in = os->in.start;
  os->zs.avail_in = ZRLE_BUFFER_LENGTH (&os->in);
  
#ifdef ZRLE_DEBUG
  rfbLog("zrleOutStreamFlush: avail_in %d\n", os->zs.avail_in);
#endif

  while (os->zs.avail_in != 0) {
    do {
      int ret;

      if (os->out.ptr >= os->out.end &&
	  !zrleBufferGrow(&os->out, os->out.end - os->out.start)) {
	rfbLog("zrleOutStreamFlush: failed to grow output buffer\n");
	return FALSE;
      }

      os->zs.next_out = os->out.ptr;
      os->zs.avail_out = os->out.end - os->out.ptr;

#ifdef ZRLE_DEBUG
      rfbLog("zrleOutStreamFlush: calling deflate, avail_in %d, avail_out %d\n",
	     os->zs.avail_in, os->zs.avail_out);
#endif 

      if ((ret = deflate(&os->zs, Z_SYNC_FLUSH)) != Z_OK) {
	rfbLog("zrleOutStreamFlush: deflate failed with error code %d\n", ret);
	return FALSE;
      }

#ifdef ZRLE_DEBUG
      rfbLog("zrleOutStreamFlush: after deflate: %d bytes\n",
	     os->zs.next_out - os->out.ptr);
#endif

      os->out.ptr = os->zs.next_out;
    } while (os->zs.avail_out == 0);
  }

  os->in.ptr = os->in.start;
 
  return TRUE;
}

static int zrleOutStreamOverrun(zrleOutStream *os,
				int            size)
{
#ifdef ZRLE_DEBUG
  rfbLog("zrleOutStreamOverrun\n");
#endif

  while (os->in.end - os->in.ptr < size && os->in.ptr > os->in.start) {
    os->zs.next_in = os->in.start;
    os->zs.avail_in = ZRLE_BUFFER_LENGTH (&os->in);

    do {
      int ret;

      if (os->out.ptr >= os->out.end &&
	  !zrleBufferGrow(&os->out, os->out.end - os->out.start)) {
	rfbLog("zrleOutStreamOverrun: failed to grow output buffer\n");
	return FALSE;
      }

      os->zs.next_out = os->out.ptr;
      os->zs.avail_out = os->out.end - os->out.ptr;

#ifdef ZRLE_DEBUG
      rfbLog("zrleOutStreamOverrun: calling deflate, avail_in %d, avail_out %d\n",
	     os->zs.avail_in, os->zs.avail_out);
#endif

      if ((ret = deflate(&os->zs, 0)) != Z_OK) {
	rfbLog("zrleOutStreamOverrun: deflate failed with error code %d\n", ret);
	return 0;
      }

#ifdef ZRLE_DEBUG
      rfbLog("zrleOutStreamOverrun: after deflate: %d bytes\n",
	     os->zs.next_out - os->out.ptr);
#endif

      os->out.ptr = os->zs.next_out;
    } while (os->zs.avail_out == 0);

    /* output buffer not full */

    if (os->zs.avail_in == 0) {
      os->in.ptr = os->in.start;
    } else {
      /* but didn't consume all the data?  try shifting what's left to the
       * start of the buffer.
       */
      rfbLog("zrleOutStreamOverrun: out buf not full, but in data not consumed\n");
      memmove(os->in.start, os->zs.next_in, os->in.ptr - os->zs.next_in);
      os->in.ptr -= os->zs.next_in - os->in.start;
    }
  }

  if (size > os->in.end - os->in.ptr)
    size = os->in.end - os->in.ptr;

  return size;
}

static int zrleOutStreamCheck(zrleOutStream *os, int size)
{
  if (os->in.ptr + size > os->in.end) {
    return zrleOutStreamOverrun(os, size);
  }
  return size;
}

void zrleOutStreamWriteBytes(zrleOutStream *os,
			     const zrle_U8 *data,
			     int            length)
{
  const zrle_U8* dataEnd = data + length;
  while (data < dataEnd) {
    int n = zrleOutStreamCheck(os, dataEnd - data);
    memcpy(os->in.ptr, data, n);
    os->in.ptr += n;
    data += n;
  }
}

void zrleOutStreamWriteU8(zrleOutStream *os, zrle_U8 u)
{
  zrleOutStreamCheck(os, 1);
  *os->in.ptr++ = u;
}

void zrleOutStreamWriteOpaque8(zrleOutStream *os, zrle_U8 u)
{
  zrleOutStreamCheck(os, 1);
  *os->in.ptr++ = u;
}

void zrleOutStreamWriteOpaque16 (zrleOutStream *os, zrle_U16 u)
{
  zrleOutStreamCheck(os, 2);
  *os->in.ptr++ = ((zrle_U8*)&u)[0];
  *os->in.ptr++ = ((zrle_U8*)&u)[1];
}

void zrleOutStreamWriteOpaque32 (zrleOutStream *os, zrle_U32 u)
{
  zrleOutStreamCheck(os, 4);
  *os->in.ptr++ = ((zrle_U8*)&u)[0];
  *os->in.ptr++ = ((zrle_U8*)&u)[1];
  *os->in.ptr++ = ((zrle_U8*)&u)[2];
  *os->in.ptr++ = ((zrle_U8*)&u)[3];
}

void zrleOutStreamWriteOpaque24A(zrleOutStream *os, zrle_U32 u)
{
  zrleOutStreamCheck(os, 3);
  *os->in.ptr++ = ((zrle_U8*)&u)[0];
  *os->in.ptr++ = ((zrle_U8*)&u)[1];
  *os->in.ptr++ = ((zrle_U8*)&u)[2];
}

void zrleOutStreamWriteOpaque24B(zrleOutStream *os, zrle_U32 u)
{
  zrleOutStreamCheck(os, 3);
  *os->in.ptr++ = ((zrle_U8*)&u)[1];
  *os->in.ptr++ = ((zrle_U8*)&u)[2];
  *os->in.ptr++ = ((zrle_U8*)&u)[3];
}
